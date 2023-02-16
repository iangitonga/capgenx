#pragma once

#include <torch/torch.h>
#include <array>
#include <vector>
#include "model.h"
#include "tokenizer.h"

 
namespace capgen {

class SubSegmentTranscription {
public:
  std::vector<int> text_tokens;
  float start_time, end_time;

  SubSegmentTranscription();
};


class SegmentTranscription {
public:
  std::vector<SubSegmentTranscription> sub_segments;
  int segment_index;
  float end_time;

  SegmentTranscription(std::array<int, 256>& _tokens, unsigned int _n_tokens,
                       int _segment_index, const Tokenizer& tokenizer);
};


void greedy_decode_segment(Whisper& model, const Tokenizer& tokenizer,
                           const torch::Tensor& spectrogram,
                           std::vector<SegmentTranscription>& out_transcriptions,
                           const int segment_index);


void greedy_decode_segment_realtime(Whisper& model, const Tokenizer& tokenizer,
                                    const torch::Tensor& spectrogram, const int segment_index);


void save_to_srt(const std::vector<SegmentTranscription>& transcription,
                 const Tokenizer& tokenizer, const std::string& filename);

} // namespace capgen

