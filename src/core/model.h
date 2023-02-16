#pragma once

#include <torch/torch.h>
#include <string>


namespace capgen {

class Whisper {
private:
  const std::string encoder_path_ = "../assets/models/traced_encoder.pt";
  const std::string decoder_path_ = "../assets/models/traced_decoder.pt";
  torch::jit::script::Module encoder_;
  torch::jit::script::Module decoder_;

public:
  // Maximum number of context tokens.
  const int n_ctx = 224;

  Whisper();
  const torch::Tensor embed_audio(const torch::Tensor& spectrogram);
  const torch::Tensor logits(const torch::Tensor& tokens,
                             const torch::Tensor& audio_features,
                             const int cache_index);
};

} // namespace capgen
