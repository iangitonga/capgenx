#include <torch/torch.h>
#include <cstdio>
#include <vector>
#include "decoder.h"
#include "tokenizer.h"
#include "utils.h"


static const torch::Tensor BLANK_TOKENS = torch::tensor({220, 50258});
static const torch::Tensor FORBIDDEN_TOKENS = torch::tensor(
  {1,2,7,8,9,10,14,25,26,27,28,29,31,58,59,60,61,62,63,90,91,92,93,359,503,522,542,873,
  893,902,918,922,931,1350,1853,1982,2460,2627,3246,3253,3268,3536,3846,3961,4183,4667,
  6585,6647,7273,9061,9383,10428,10929,11938,12033,12331,12562,13793,14157,14635,15265,
  15618,16553,16604,18362,18956,20075,21675,22520,26130,26161,26435,28279,29464,31650,
  32302,32470,36865,42863,47425,49870,50254,50258,50360,50361,50362
});


static void apply_timestamp_rules(torch::Tensor& logits, const torch::Tensor& tokens, const capgen::Tokenizer& tokenizer_) {
  using namespace torch::indexing;
  // Suppress <|notimestamps|> because we always want timestamps.
  logits.index_put_({Slice(NULL), tokenizer_.no_timestamps}, -INFINITY);
  // timestamps have to appear in pairs, except directly before EOT; mask logits accordingly
  for (int k = 0; k < tokens.size(0); ++k) {
    // Select all the predicted tokens.
    auto seq = tokens[k].index({Slice(tokenizer_.sample_begin, NULL)});
    bool last_was_timestamp = false;
    // check if the seq has atleast two preds and if the last pred is a timestamp.
    if (seq.size(0) >= 1 && seq[-1].item().toInt() >= tokenizer_.timestamp_begin)
      last_was_timestamp = true;
    bool penultimate_was_timestamp = false;
    // check if the second last item was a timestamp.
    if (seq.size(0) < 2 || seq[-2].item().toInt() >= tokenizer_.timestamp_begin)
      penultimate_was_timestamp = true;
    if (last_was_timestamp) {
      // Timestamps have appeared in pairs, suppress all timestamps for next pred cycle.
      if (penultimate_was_timestamp)
        logits[k].index_put_({Slice(tokenizer_.timestamp_begin, NULL)}, -INFINITY);
      else
        logits[k].index_put_({Slice(NULL, tokenizer_.eot)}, -INFINITY);
    }
  }

  double precision = 30.0 / 448.0;
  // Initial timestamp cannot be later than this.
  double max_initial_timestamp = 1.0;
  int max_initial_timesamp_index = round(max_initial_timestamp / precision);
  if (tokens.size(1) == tokenizer_.sample_begin) {
    int last_allowed = tokenizer_.timestamp_begin + max_initial_timesamp_index;
    logits.index_put_({Slice(NULL), Slice(last_allowed + 1, NULL)}, -INFINITY);
  }

  // if sum of probability over timestamps is above any other token, sample timestamp 
  torch::Tensor logprobs = torch::log_softmax(logits.to(torch::kFloat32), -1);
  for (int k = 0; k < tokens.size(0); ++k) {
    torch::Tensor timestamp_logprob = logprobs[k].index({Slice(tokenizer_.timestamp_begin + 1)}).logsumexp(-1);
    torch::Tensor max_text_token_logprob = logprobs[k].index({Slice(NULL, tokenizer_.timestamp_begin + 1)}).max();
    if (timestamp_logprob.item().toDouble() > max_text_token_logprob.item().toDouble()) {
      logits[k].index_put_({Slice(NULL, tokenizer_.timestamp_begin + 1)}, -INFINITY);
    }
  }
}


static inline void suppress_forbidden(torch::Tensor& logits, const torch::Tensor& tokens, const capgen::Tokenizer& tokenizer) {
  logits.index_put_({torch::indexing::Slice(NULL), BLANK_TOKENS}, -INFINITY);
  logits.index_put_({torch::indexing::Slice(NULL), FORBIDDEN_TOKENS}, -INFINITY);
  apply_timestamp_rules(logits, tokens, tokenizer);
}


namespace capgen {

SubSegmentTranscription::SubSegmentTranscription() {
  text_tokens = std::vector<int>();
  text_tokens.reserve(20);
  start_time = 0.0f;
  end_time = 0.0f;
}

SegmentTranscription::SegmentTranscription(std::array<int, 256>& tokens, unsigned int n_tokens, int _segment_index, const Tokenizer& tokenizer) {
  segment_index = _segment_index;
  if (tokenizer.is_timestamp(tokens[n_tokens - 1]))
    end_time = tokenizer.time_from_token(tokens[n_tokens - 1]);
  else {
    end_time = 30.0f;
    tokens[n_tokens] = tokenizer.timestamp_end;
    n_tokens += 1;
  }
  SubSegmentTranscription sub_seg_tr;
  for (int i = 0; i < n_tokens; ++i) {
    if (tokenizer.is_timestamp(tokens[i])) {
      if (sub_seg_tr.text_tokens.size() == 0)
        sub_seg_tr.start_time = tokenizer.time_from_token(tokens[i]);
      else {
        sub_seg_tr.end_time = tokenizer.time_from_token(tokens[i]);
        sub_segments.push_back(std::move(sub_seg_tr));
        sub_seg_tr = SubSegmentTranscription();
      }
    } else
      sub_seg_tr.text_tokens.push_back(tokens[i]);
  }
}


void greedy_decode_segment(Whisper& model, const Tokenizer& tokenizer,
                   const torch::Tensor& spectrogram, std::vector<SegmentTranscription>& out_transcriptions,
                   const int segment_index) {
  const torch::Tensor audio_features = model.embed_audio(spectrogram);
  torch::Tensor tokens = torch::tensor({{tokenizer.sot, 50259, tokenizer.transcribe}});
  std::array<int, 256> pred_tokens;
  unsigned int n_pred_tokens = 0;
  for (int i = 0; i < model.n_ctx; ++i) {
    torch::Tensor logits = model.logits(tokens, audio_features, segment_index);
    logits = logits.index({torch::indexing::Slice(NULL), torch::indexing::Slice(-1)}).squeeze(1);
    suppress_forbidden(logits, tokens, tokenizer);
    const torch::Tensor probs = torch::softmax(logits, -1);
    const torch::Tensor pred_token = probs.argmax(-1);
    const int pred_token_int = pred_token.item().toInt();
    if (pred_token_int == tokenizer.eot)
      break;
    pred_tokens[i] = pred_token_int;
    n_pred_tokens += 1;
    // Add predicted token to the context.
    tokens = torch::cat({tokens, pred_token.view({1, 1})}, 1);
  }
  out_transcriptions.push_back(SegmentTranscription(pred_tokens, n_pred_tokens, segment_index, tokenizer));
}


void greedy_decode_segment_realtime(Whisper& model, const Tokenizer& tokenizer,
                                    const torch::Tensor& spectrogram, const int segment_index) {
  const torch::Tensor audio_features = model.embed_audio(spectrogram);
  torch::Tensor tokens = torch::tensor({{tokenizer.sot, 50259, tokenizer.transcribe, tokenizer.no_timestamps}});
  for (int i = 0; i < model.n_ctx; ++i) {
    torch::Tensor logits = model.logits(tokens, audio_features, segment_index);
    logits = logits.index({torch::indexing::Slice(NULL), torch::indexing::Slice(-1)}).squeeze(1);
    suppress_forbidden(logits, tokens, tokenizer);
    const torch::Tensor probs = torch::softmax(logits, -1);
    const torch::Tensor pred_token = probs.argmax(-1);
    const int pred_token_int = pred_token.item().toInt();
    if (pred_token_int == tokenizer.eot)
      break;
    // To cerr because it is unbuffered.
    std::cerr << tokenizer.decode_token(pred_token_int);
    // Add predicted token to the context.
    tokens = torch::cat({tokens, pred_token.view({1, 1})}, 1);
  }
  std::cerr << "\n\n";
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


void save_to_srt(const std::vector<SegmentTranscription>& transcription, const Tokenizer& tokenizer, const std::string& filename) {
  std::FILE *outfile = std::fopen(filename.c_str(), "w");
  if (!outfile) {
    std::cerr << "Failed to open file" << std::endl;
    throw std::exception();
  }
  float time_offset = 0.0f;
  int segment_index = 1;
  for (auto &seg_tr : transcription) {
    for (auto &sub_seg_tr : seg_tr.sub_segments) {
      std::fprintf(outfile, "%d\n", segment_index);
      segment_index += 1;
      write_time(time_offset + sub_seg_tr.start_time, outfile, true);
      write_time(time_offset + sub_seg_tr.end_time, outfile, false);
      for (auto &token : sub_seg_tr.text_tokens)
        std::fprintf(outfile, tokenizer.decode_token(token));
      std::fprintf(outfile, "\n\n");
    }
    time_offset += seg_tr.end_time;
  }
  std::fclose(outfile);
}

} // namespace capgen
