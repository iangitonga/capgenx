#pragma once

#include <torch/torch.h>
#include <string>
#include "caudio.h"


namespace capgen {

class AudioDecoder {
private:
  // TODO: May not work on Windows.
  const std::string mel_filters_path_ = "./assets/mel_80";

  const torch::Tensor get_audio_tensor_() const;
  const torch::Tensor get_mel_filters_() const;
  const torch::Tensor get_audio_spectrogram_(const torch::Tensor& audio) const;
  // Pads the spectrogram to ensure the last segment has 3000 frames.
  const torch::Tensor pad_audio_spectrogram_(const torch::Tensor& spectrogram) const;

public:
  AudioDecodeOutput *decode_output;
  AudioDecoder(bool enable_logging = false);
  ~AudioDecoder();
  const torch::Tensor get_audio_spectrogram(const char *infilepath) const;
};

}