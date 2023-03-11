#include "decoder.h"
#include "log.h"
#include "tokenizer.h"
#include "utils.h"

#include <algorithm>
#include <cstdio>
#include <vector>



// All non-language tokens that are suppressed during decoding.
static const at::Tensor s_ENGLISH_VOCAB_BAD_TOKENS = at::tensor(
    {
        1, 2, 7, 8, 9, 10, 14, 25, 26, 27, 28, 29, 31, 58, 59, 60, 61, 62, 63, 90, 91, 92,
        93, 357, 366, 438, 532, 685, 705, 796, 930, 1058, 1220, 1267, 1279, 1303, 1343, 1377,
        1391, 1635, 1782, 1875, 2162, 2361, 2488, 3467, 3880, 4008, 4211, 4600, 4808, 5299,
        5855, 6329, 7203, 8864, 9609, 9959, 10221, 10563, 10786, 11420, 11709, 11907, 13163,
        13697, 13700, 14808, 15306, 16410, 16791, 17174, 17992, 19203, 19510, 20368, 20724,
        22305, 22935, 23090, 27007, 29113, 30109, 30420, 30906, 33409, 34949, 40283, 40493, 
        40549, 41906, 46111, 47282, 49146, 49704, 50361
    },
    at::TensorOptions(at::kLong)
);

static const at::Tensor s_MULTILINGUAL_VOCAB_BAD_TOKENS = at::tensor(
    {
        1, 2, 7, 8, 9, 10, 14, 25, 26, 27, 28, 29, 31, 58, 59, 60, 61, 62, 63, 90, 91, 92,
        93, 220, 359, 503, 522, 542, 873, 893, 902, 918, 922, 931, 1350, 1853, 1982, 2460, 
        2627, 3246, 3253, 3268, 3536, 3846, 3961, 4183, 4667, 6585, 6647, 7273, 9061, 9383,
        10428, 10929, 11938, 12033, 12331, 12562, 13793, 14157, 14635, 15265, 15618, 16553,
        16604, 18362, 18956, 20075, 21675, 22520, 26130, 26161, 26435, 28279, 29464, 31650,
        32302, 32470, 36865, 42863, 47425, 49870, 50254, 50258, 50360, 50361, 50362
    },
    at::TensorOptions(at::kLong)
);


static void apply_timestamp_rules(at::Tensor& logits,
                                  const at::Tensor& tokens,
                                  const capgen::Tokenizer& tokenizer)
{
    using namespace at::indexing;

    // Suppress <|notimestamps|> because we always want timestamps.
    logits.index_put_({Slice(NULL), tokenizer.no_timestamps()}, -INFINITY);
    // timestamps have to appear in pairs, except directly before EOT; mask logits accordingly.
    for (int k = 0; k < tokens.size(0); ++k)
    {
        // Select all the predicted tokens.
        auto seq = tokens[k].index({Slice(tokenizer.prompt_length(), NULL)});
        bool last_was_timestamp = false;
        // check if the seq has atleast two preds and if the last pred is a timestamp.
        if (seq.size(0) >= 1 && seq[-1].item().toInt() >= tokenizer.timestamp_begin())
            last_was_timestamp = true;
        bool penultimate_was_timestamp = false;
        // check if the second last item was a timestamp.
        if (seq.size(0) < 2 || seq[-2].item().toInt() >= tokenizer.timestamp_begin())
            penultimate_was_timestamp = true;
        if (last_was_timestamp)
        {
            // Timestamps have appeared in pairs, suppress all timestamps for next pred cycle.
            if (penultimate_was_timestamp)
                logits[k].index_put_({Slice(tokenizer.timestamp_begin(), NULL)}, -INFINITY);
            else
                logits[k].index_put_({Slice(NULL, tokenizer.eot())}, -INFINITY);
        }
    }

    double precision = 30.0 / 448.0;
    // Initial timestamp cannot be later than this.
    double max_initial_timestamp = 1.0;
    int max_initial_timesamp_index = round(max_initial_timestamp / precision);
    if (tokens.size(1) == tokenizer.prompt_length())
    {
        int last_allowed = tokenizer.timestamp_begin() + max_initial_timesamp_index;
        logits.index_put_({Slice(NULL), Slice(last_allowed + 1, NULL)}, -INFINITY);
    }

    // if sum of probability over timestamps is above any other token, sample timestamp
    at::Tensor logprobs = at::log_softmax(logits.to(at::kFloat), -1);
    for (int k = 0; k < tokens.size(0); ++k)
    {
        at::Tensor timestamp_logprob = logprobs[k].index({Slice(tokenizer.timestamp_begin() + 1)}).logsumexp(-1);
        at::Tensor max_text_token_logprob = logprobs[k].index({Slice(NULL, tokenizer.timestamp_begin() + 1)}).max();

        if (timestamp_logprob.item().toDouble() > max_text_token_logprob.item().toDouble()) 
        {
            logits[k].index_put_({Slice(NULL, tokenizer.timestamp_begin() + 1)}, -INFINITY);
        }
    }
}

static void suppress_forbidden(at::Tensor& logits,
                                      const at::Tensor& tokens,
                                      const capgen::Tokenizer& tokenizer)
{
    if (tokenizer.is_multilingual())
        logits.index_put_({at::indexing::Slice(NULL), s_MULTILINGUAL_VOCAB_BAD_TOKENS}, -INFINITY);
    else
        logits.index_put_({at::indexing::Slice(NULL), s_ENGLISH_VOCAB_BAD_TOKENS}, -INFINITY);
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
    at::Tensor tokens = at::tensor({tokenizer.sot()}, tensor_opts);
    tokens = tokens.unsqueeze(0);
    at::Tensor logits = model->logits(tokens, audio_features, -2);
    logits = logits.index({at::indexing::Slice(NULL), 0});
    at::Tensor mask = at::ones(logits.size(-1), at::kBool);
    // Mask language tokens.
    mask.index_put_({at::indexing::Slice(50259, 50357 + 1)}, false);
    logits.index_put_({at::indexing::Slice(NULL), mask}, -INFINITY);
    at::Tensor language_token_probs = logits.softmax(-1);
    at::Tensor language_token = language_token_probs.argmax(-1);
    return language_token.item().toInt();
}

TimestampedTranscription::TimestampedTranscription()
{
    m_text_tokens = std::vector<uint32_t>();
    m_text_tokens.reserve(20);
    m_start_time = 0.0f;
    m_end_time = 0.0f;
}

SegmentTranscription::SegmentTranscription(std::vector<uint32_t>& tokens,
                                           uint32_t segment_index,
                                           const Tokenizer& tokenizer)
{
    m_segment_index = segment_index;
    if (tokenizer.is_timestamp(tokens.back()))
        m_end_time = tokenizer.decode_timestamp_token(tokens.back());
    else
    {
        m_end_time = 30.0f;
        tokens.push_back(tokenizer.timestamp_end());
    }

    TimestampedTranscription timestamped_trx;
    for (uint32_t token : tokens) {
        if (tokenizer.is_timestamp(token))
        {
            if (timestamped_trx.m_text_tokens.size() == 0)
                timestamped_trx.m_start_time = tokenizer.decode_timestamp_token(token);
            else
            {
                timestamped_trx.m_end_time = tokenizer.decode_timestamp_token(token);
                sub_segments.push_back(std::move(timestamped_trx));
                timestamped_trx = TimestampedTranscription();
            }
        }
        else
            timestamped_trx.m_text_tokens.push_back(token);
    }
}


void greedy_decode_segment(const at::Tensor& spectrogram,
                           capgen::TranscriptionTask task,
                           const uint32_t language_id,
                           const uint32_t segment_index,
                           std::shared_ptr<Whisper> model,
                           const Tokenizer& tokenizer,
                           std::vector<SegmentTranscription>& out_transcriptions)
{
    const at::Tensor audio_features = model->embed_audio(spectrogram);
    const auto tensor_opts = at::TensorOptions(at::kLong);
    at::Tensor tokens;
    if (tokenizer.is_multilingual())
    {
        int task = (task == capgen::TranscriptionTask::Transcribe) ? tokenizer.transcribe() : tokenizer.translate();
        tokens = at::tensor({tokenizer.sot(), (int)language_id, task}, tensor_opts);
    }
    else
        tokens = at::tensor({tokenizer.sot()}, tensor_opts);
    tokens = tokens.unsqueeze(0);
    std::vector<uint32_t> pred_tokens;
    pred_tokens.reserve(model->n_ctx());
    for (int i = 0; i < model->n_ctx(); ++i)
    {
        at::Tensor logits = model->logits(tokens, audio_features, segment_index);
        logits = logits.index({at::indexing::Slice(NULL), at::indexing::Slice(-1)}).squeeze(1);
        suppress_forbidden(logits, tokens, tokenizer);
        // const at::Tensor probs = at::softmax(logits, -1);
        const at::Tensor pred_token = logits.argmax(-1);
        const int pred_token_int = pred_token.item().toInt();
        if (pred_token_int == tokenizer.eot())
            break;
        pred_tokens.push_back((uint32_t)pred_token_int);
        // Add predicted token to the context.
        tokens = at::cat({tokens, pred_token.view({1, 1})}, 1);
    }
    out_transcriptions.push_back(SegmentTranscription(pred_tokens, segment_index, tokenizer));
}

void beamsearch_decode_segment(const at::Tensor &spectrogram,
                               TranscriptionTask task,
                               const uint32_t language_id,
                               const uint32_t segment_index,
                               std::shared_ptr<Whisper> model,
                               const Tokenizer &tokenizer,
                               std::vector<SegmentTranscription> &out_transcriptions)
{
    // BEAMSEARCH.
    const uint32_t n_beam = 3;
    // Embed audio and copy the embeddings `n_beam` times in the zeroth dimension.
    at::Tensor audio_features = model->embed_audio(spectrogram);
    audio_features = audio_features.repeat_interleave(n_beam, 0);

    // Prepare initial prompt sequence.
    at::Tensor ctx_tokens;
    const auto tokens_opts = at::TensorOptions(at::kLong);
    if (tokenizer.is_multilingual())
    {
        int task = (task == capgen::TranscriptionTask::Transcribe) ? tokenizer.transcribe() : tokenizer.translate();
        ctx_tokens = at::tensor({tokenizer.sot(), (int)language_id, task}, tokens_opts);
    }
    else
        ctx_tokens = at::tensor({tokenizer.sot()}, tokens_opts);
    ctx_tokens = ctx_tokens.unsqueeze(0);
    ctx_tokens = ctx_tokens.repeat_interleave(n_beam, 0);

    std::vector<float> sum_logprobs(n_beam, 0.0);

    std::vector<std::vector<uint32_t>> final_beams_tokens;
    final_beams_tokens.reserve(n_beam);

    std::vector<float> final_beams_logprobs;
    final_beams_logprobs.reserve(n_beam);

    // std::map<at::Tensor, double> scores;
    std::vector<std::pair<float, at::Tensor>> scores;
    auto scores_comp = [](const std::pair<float, at::Tensor> &lhs, const std::pair<float, at::Tensor> &rhs)
    {
        return lhs.first < rhs.first;
    };
    scores.reserve(n_beam * n_beam);

    uint32_t n_completed = 0;
    uint32_t cache_idx = segment_index + n_beam + 1;

    for (int i = 0; i < model->n_ctx(); ++i)
    {
        if (n_completed == n_beam)
            break;

        at::Tensor logits = model->logits(ctx_tokens, audio_features, cache_idx);
        logits = logits.index({at::indexing::Slice(NULL), at::indexing::Slice(-1)}).squeeze(1);
        suppress_forbidden(logits, ctx_tokens, tokenizer);
        at::Tensor logprobs = logits.log_softmax(-1);
        auto logprobs_topk = logprobs.topk(n_beam, -1, true, true);
        at::Tensor top_logprobs = std::get<0>(logprobs_topk);
        at::Tensor top_tokens =  std::get<1>(logprobs_topk);

        // Resets the score so we can compare new beams.
        scores.clear();
        // Rows represent number of beams remaining to be completed while columns reperesent the number of
        // tokens that we consider at each iteration from each individual beam.
        for (int row = 0; row < n_beam - n_completed; ++row)
            for (int col = 0; col < n_beam; ++col)
            {
                float cumulative_logprob = sum_logprobs[row] + top_logprobs.index({row, col}).item().toFloat();
                at::Tensor prefix = at::cat({ctx_tokens.index({row}).flatten(), top_tokens.index({row, col}).unsqueeze(0)}, 0);
                scores.push_back(std::make_pair(cumulative_logprob, prefix));
            }


        sum_logprobs.clear();
        // We don't sort on the first iteration because all the beams have the same predictions and
        // therefore we only need to pick top beams from a single beam.
        if (i > 0)
            std::sort(scores.rbegin(),scores.rend(), scores_comp);

        at::Tensor new_ctx_tokens = at::zeros({1, scores[0].second.size(0)}, tokens_opts);
        int n_remaining_beams = n_beam - n_completed;
        for (int beam_idx = 0; beam_idx < n_remaining_beams; ++beam_idx)
        {
            auto [logprob, prefix] = scores[beam_idx];
            int pred_token = prefix.index({-1}).item().toInt();
            if (pred_token == tokenizer.eot())
            {
                at::Tensor out_prefix = prefix.slice(0, tokenizer.prompt_length(), -1);
                std::vector out_prefix_vec = std::vector<uint32_t>(out_prefix.data_ptr<int64_t>(),
                                                                   out_prefix.data_ptr<int64_t>() + out_prefix.numel());
                final_beams_tokens.push_back(std::move(out_prefix_vec));
                final_beams_logprobs.push_back(logprob);
                n_completed += 1;

                audio_features = audio_features.index({at::indexing::Slice(0, n_beam - n_completed)});
                // Resets the cache so that the model recompute the cache on the next iteration.
                // This is important because we just sliced the audio features and therefore the
                // cache of the correct dimensions must be recomputed. Also, we reset by adding two
                // to avoid the next segment from using cache from the current segment.
                cache_idx += 2;
            }
            else
            {
                new_ctx_tokens = at::cat({new_ctx_tokens, prefix.unsqueeze(0)}, 0);
                sum_logprobs.push_back(logprob);
            }
        }

        // Slicing gets rid of the first row of zeros that was created with the tensor.
        if (new_ctx_tokens.size(0) > 1)
            ctx_tokens = new_ctx_tokens.index({at::indexing::Slice(1, (n_beam - n_completed)+1)});
    }

    if (!final_beams_tokens.empty()) {
        // Normalize the log probabilities and pick the transcription with the highest log
        // probability.
        float max_logprob = -INFINITY;
        int max_logprob_idx = 0;
        for (int j = 0; j < final_beams_logprobs.size(); j++)
        {
            float normalised_logprob = final_beams_logprobs[j] / final_beams_tokens[j].size();
            if (normalised_logprob > max_logprob)
            {
                max_logprob = normalised_logprob;
                max_logprob_idx = j;
            }
        }

        out_transcriptions.push_back(SegmentTranscription(final_beams_tokens[max_logprob_idx], segment_index, tokenizer));
    }
    else
    {
        uint32_t start_time = (uint32_t)tokenizer.timestamp_begin();
        uint32_t end_time = (uint32_t)tokenizer.timestamp_end();
        // Put three dots to indicate failure. In the future, shall be replaced by other
        // methods such as repetition dection methods.
        std::vector<uint32_t> failed_tokens({start_time, 13, 13, 13, end_time});

        out_transcriptions.push_back(SegmentTranscription(failed_tokens, segment_index, tokenizer));
    }
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
    if (!outfile)
    {
        CG_LOG_ERROR("Failed to create captions srt file at %s", filename.c_str());
        throw std::exception();
    }
    float time_offset = 0.0f;
    int segment_index = 1;
    for (const auto &segment_trx : transcription)
    {
        for (const auto &timestamped_trx : segment_trx.sub_segments)
        {
            std::fprintf(outfile, "%d\n", segment_index);
            segment_index += 1;
            write_time(time_offset + timestamped_trx.m_start_time, outfile, true);
            write_time(time_offset + timestamped_trx.m_end_time, outfile, false);
            for (auto &token : timestamped_trx.m_text_tokens)
            std::fprintf(outfile, "%s", tokenizer.decode_token(token));
            std::fprintf(outfile, "\n\n");
        }
        time_offset += segment_trx.m_end_time;
    }
    std::fclose(outfile);
}

} // namespace capgen
