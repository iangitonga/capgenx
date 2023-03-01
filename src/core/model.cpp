#include <vector>
#include "model.h"


namespace capgen {

Whisper::Whisper(const std::string &name)
  : m_name(name)
{
  std::cout << "[INFO]: Loading models\n";
  try {
    std::string encoder_path = std::string("./assets/models/") + m_name + std::string("/traced_encoder.pt");
    std::string decoder_path = std::string("./assets/models/") + m_name + std::string("/traced_decoder.pt");
		m_encoder = torch::jit::load(encoder_path);
		m_decoder = torch::jit::load(decoder_path);
	} catch (const c10::Error &e) {
		std::cerr << "[ERROR]: Failed to load modules\n";
		throw;
	}
  m_encoder.eval();
  m_decoder.eval();
  std::cout << "[INFO]: Models loaded.\n";
}

const at::Tensor Whisper::embed_audio(const at::Tensor& spectrogram) {
  at::NoGradGuard no_grad;  // No gradients.
  const std::vector<torch::jit::IValue> encoder_inputs = {spectrogram};
  const at::Tensor audio_features = m_encoder.forward(encoder_inputs).toTensor();
  return audio_features;
}

const at::Tensor Whisper::logits(const at::Tensor& tokens, 
                                 const at::Tensor& audio_features,
                                 const int cache_index)
{
  at::NoGradGuard no_grad;  // No gradients.
  const std::vector<torch::jit::IValue> decoder_inputs = {tokens, audio_features, at::tensor({cache_index})};
  const at::Tensor logits = m_decoder.forward(decoder_inputs).toTensor();
  return logits;
}

}  // namespace capgen