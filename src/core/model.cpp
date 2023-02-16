#include <torch/script.h>
#include <vector>
#include "model.h"

namespace capgen {

Whisper::Whisper() {
  try {
		encoder_ = torch::jit::load(encoder_path_);
		decoder_ = torch::jit::load(decoder_path_);
	} catch (const c10::Error &e) {
		std::cerr << "Error loading modules!!" << std::endl;
		throw e;
	}
}

const torch::Tensor Whisper::embed_audio(const torch::Tensor& spectrogram) {
  torch::NoGradGuard no_grad;  // No gradients.
  const std::vector<torch::jit::IValue> encoder_inputs = {spectrogram};
  const torch::Tensor audio_features = encoder_.forward(encoder_inputs).toTensor();
  return audio_features;
}

const torch::Tensor Whisper::logits(const torch::Tensor& tokens, 
                                    const torch::Tensor& audio_features,
                                    const int cache_index) {
  torch::NoGradGuard no_grad;  // No gradients.
  const std::vector<torch::jit::IValue> decoder_inputs = {tokens, audio_features, torch::tensor({cache_index})};
  const torch::Tensor logits = decoder_.forward(decoder_inputs).toTensor();
  return logits;
}

}