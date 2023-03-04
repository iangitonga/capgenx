#include <cstdio>
#include <vector>
#include "decoder.h"
#include "log.h"
#include "tokenizer.h"
#include "utils.h"


static void apply_timestamp_rules(at::Tensor& logits,
                                  const at::Tensor& tokens,
                                  const capgen::Tokenizer& tokenizer_)
{
  using namespace at::indexing;
  // Suppress <|notimestamps|> because we always want timestamps.
  logits.index_put_({Slice(NULL), tokenizer_.m_special_tokens.no_timestamps}, -INFINITY);
  // timestamps have to appear in pairs, except directly before EOT; mask logits accordingly
  for (int k = 0; k < tokens.size(0); ++k) {
    // Select all the predicted tokens.
    auto seq = tokens[k].index({Slice(tokenizer_.m_prompt_length, NULL)});
    bool last_was_timestamp = false;
    // check if the seq has atleast two preds and if the last pred is a timestamp.
    if (seq.size(0) >= 1 && seq[-1].item().toInt() >= tokenizer_.m_special_tokens.timestamp_begin)
      last_was_timestamp = true;
    bool penultimate_was_timestamp = false;
    // check if the second last item was a timestamp.
    if (seq.size(0) < 2 || seq[-2].item().toInt() >= tokenizer_.m_special_tokens.timestamp_begin)
      penultimate_was_timestamp = true;
    if (last_was_timestamp) {
      // Timestamps have appeared in pairs, suppress all timestamps for next pred cycle.
      if (penultimate_was_timestamp)
        logits[k].index_put_({Slice(tokenizer_.m_special_tokens.timestamp_begin, NULL)}, -INFINITY);
      else
        logits[k].index_put_({Slice(NULL, tokenizer_.m_special_tokens.eot)}, -INFINITY);
    }
  }

  double precision = 30.0 / 448.0;
  // Initial timestamp cannot be later than this.
  double max_initial_timestamp = 1.0;
  int max_initial_timesamp_index = round(max_initial_timestamp / precision);
  if (tokens.size(1) == tokenizer_.m_prompt_length) {
    int last_allowed = tokenizer_.m_special_tokens.timestamp_begin + max_initial_timesamp_index;
    logits.index_put_({Slice(NULL), Slice(last_allowed + 1, NULL)}, -INFINITY);
  }

  // if sum of probability over timestamps is above any other token, sample timestamp
  at::Tensor logprobs = at::log_softmax(logits.to(at::kFloat), -1);
  for (int k = 0; k < tokens.size(0); ++k) {
    at::Tensor timestamp_logprob = logprobs[k].index({Slice(tokenizer_.m_special_tokens.timestamp_begin + 1)}).logsumexp(-1);
    at::Tensor max_text_token_logprob = logprobs[k].index({Slice(NULL, tokenizer_.m_special_tokens.timestamp_begin + 1)}).max();
    if (timestamp_logprob.item().toDouble() > max_text_token_logprob.item().toDouble()) {
      logits[k].index_put_({Slice(NULL, tokenizer_.m_special_tokens.timestamp_begin + 1)}, -INFINITY);
    }
  }
}

static void suppress_forbidden(at::Tensor& logits,
                                      const at::Tensor& tokens,
                                      const capgen::Tokenizer& tokenizer)
{
  if (tokenizer.is_multilingual())
    logits.index_put_({at::indexing::Slice(NULL), capgen::s_MULTILINGUAL_TOKENIZER_BAD_TOKENS}, -INFINITY);
  else
    logits.index_put_({at::indexing::Slice(NULL), capgen::s_ENGLISH_TOKENIZER_BAD_TOKENS}, -INFINITY);
  apply_timestamp_rules(logits, tokens, tokenizer);
}

namespace capgen {

int detect_language(const at::Tensor &spectrogram,
                    std::shared_ptr<Whisper> model,
                    const Tokenizer &tokenizer) 
{
  at::Tensor audio_segment = spectrogram.index_select(-1, at::arange(0, 3000));
  at::Tensor audio_features = model->embed_audio(audio_segment);
  const at::TensorOptions tensor_opts = at::TensorOptions(at::kLong);
  at::Tensor tokens = at::tensor({tokenizer.m_special_tokens.sot}, tensor_opts);
  tokens = tokens.unsqueeze(0);
  at::Tensor logits = model->logits(tokens, audio_features, 0);
  logits = logits.index({at::indexing::Slice(NULL), 0});
  at::Tensor mask = at::ones(logits.size(-1), at::kBool);
  // Mask language tokens.
  mask.index_put_({at::indexing::Slice(50259, 50357 + 1)}, false);
  logits.index_put_({at::indexing::Slice(NULL), mask}, -INFINITY);
  at::Tensor language_token_probs = logits.softmax(-1);
  at::Tensor language_token = language_token_probs.argmax(-1);
  return language_token.item().toInt();
}

TimestampedTranscription::TimestampedTranscription() {
  text_tokens = std::vector<int>();
  text_tokens.reserve(20);
  start_time = 0.0f;
  end_time = 0.0f;
}

SegmentTranscription::SegmentTranscription(std::array<int, 256>& tokens,
                                           unsigned int n_tokens,
                                           int _segment_index,
                                           const Tokenizer& tokenizer)
{
  segment_index = _segment_index;
  if (tokenizer.is_timestamp(tokens[n_tokens - 1]))
    end_time = tokenizer.decode_timestamp_token(tokens[n_tokens - 1]);
  else {
    end_time = 30.0f;
    tokens[n_tokens] = tokenizer.m_special_tokens.timestamp_end;
    n_tokens += 1;
  }
  TimestampedTranscription sub_seg_tr;
  for (int i = 0; i < n_tokens; ++i) {
    if (tokenizer.is_timestamp(tokens[i])) {
      if (sub_seg_tr.text_tokens.size() == 0)
        sub_seg_tr.start_time = tokenizer.decode_timestamp_token(tokens[i]);
      else {
        sub_seg_tr.end_time = tokenizer.decode_timestamp_token(tokens[i]);
        sub_segments.push_back(std::move(sub_seg_tr));
        sub_seg_tr = TimestampedTranscription();
      }
    } else
      sub_seg_tr.text_tokens.push_back(tokens[i]);
  }
}


void greedy_decode_segment(const at::Tensor& spectrogram,
                           capgen::TranscriptionTask task,
                           const int language_id,
                           const int segment_index,
                           std::shared_ptr<Whisper> model,
                           const Tokenizer& tokenizer,
                           std::vector<SegmentTranscription>& out_transcriptions)
{
  const at::Tensor audio_features = model->embed_audio(spectrogram);
  const auto tensor_opts = at::TensorOptions(at::kLong);
  at::Tensor tokens;
  if (tokenizer.is_multilingual()) {
    int task = (task == capgen::TranscriptionTask::Transcribe) ? tokenizer.m_special_tokens.transcribe : tokenizer.m_special_tokens.translate;
    tokens = at::tensor({tokenizer.m_special_tokens.sot, language_id, task}, tensor_opts);
  }
  else
    tokens = at::tensor({tokenizer.m_special_tokens.sot}, tensor_opts);
  tokens = tokens.unsqueeze(0);
  std::array<int, 256> pred_tokens;
  unsigned int n_pred_tokens = 0;
  for (int i = 0; i < model->n_ctx; ++i) {
    at::Tensor logits = model->logits(tokens, audio_features, segment_index);
    logits = logits.index({at::indexing::Slice(NULL), at::indexing::Slice(-1)}).squeeze(1);
    suppress_forbidden(logits, tokens, tokenizer);
    // const at::Tensor probs = at::softmax(logits, -1);
    const at::Tensor pred_token = logits.argmax(-1);
    const int pred_token_int = pred_token.item().toInt();
    if (pred_token_int == tokenizer.m_special_tokens.eot)
      break;
    pred_tokens[i] = pred_token_int;
    n_pred_tokens += 1;
    // Add predicted token to the context.
    tokens = at::cat({tokens, pred_token.view({1, 1})}, 1);
  }
  out_transcriptions.push_back(SegmentTranscription(pred_tokens, n_pred_tokens, segment_index, tokenizer));
}


void write_time(float seconds, std::FILE *outfile, bool is_start=true) {
  int ms = seconds * 1000;
  int hrs = exact_div(ms, 3600000l);
  ms -= hrs * 3600000;
  int mins = exact_div(ms, 60000l);
  ms -= mins * 60000;
  int secs = exact_div(ms, 1000l);
  ms -= secs * 1000;
  if (is_start)
    std::fprintf(outfile, "%02d:%02d:%02d,%03d --> ", hrs, mins, secs, ms);
  else
    std::fprintf(outfile, "%02d:%02d:%02d,%03d\n", hrs, mins, secs, ms);  
}


void save_to_srt(const std::vector<SegmentTranscription>& transcription,
                 const Tokenizer& tokenizer,
                 const std::string& filename)
{
  std::FILE *outfile = std::fopen(filename.c_str(), "w");
  // TODO: Put in current dir.
  if (!outfile) {
    CG_LOG_ERROR("Failed to create captions srt file at %s", filename.c_str());
    throw std::exception();
  }
  float time_offset = 0.0f;
  int segment_index = 1;
  for (auto &segment_transcription : transcription) {
    for (auto &timestamped_transcription : segment_transcription.sub_segments) {
      std::fprintf(outfile, "%d\n", segment_index);
      segment_index += 1;
      write_time(time_offset + timestamped_transcription.start_time, outfile, true);
      write_time(time_offset + timestamped_transcription.end_time, outfile, false);
      for (auto &token : timestamped_transcription.text_tokens)
        std::fprintf(outfile, "%s", tokenizer.decode_token(token));
      std::fprintf(outfile, "\n\n");
    }
    time_offset += segment_transcription.end_time;
  }
  std::fclose(outfile);
}
} // namespace capgen
