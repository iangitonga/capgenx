#include <vector>
#include "model.h"
#include "log.h"


namespace capgen {

Whisper::Whisper(const std::string &name, ModelType model_type)
  : m_name(name), m_model_type(model_type)
{
  CG_LOG_INFO("Loading model: %s", name.c_str());
  try {
    std::string encoder_path;
    std::string decoder_path;
    if (m_model_type == ModelType::English) {
      encoder_path = std::string("./assets/models/") + m_name + std::string("/encoder.en.pt");
      decoder_path = std::string("./assets/models/") + m_name + std::string("/decoder.en.pt");
    } else {
      encoder_path = std::string("./assets/models/") + m_name + std::string("/encoder.pt");
      decoder_path = std::string("./assets/models/") + m_name + std::string("/decoder.pt");
    }
    CG_LOG_INFO("Model encoder path: %s", encoder_path.c_str());
    CG_LOG_INFO("Model decoder path: %s", decoder_path.c_str());
		m_encoder = torch::jit::load(encoder_path);
		m_decoder = torch::jit::load(decoder_path);
	} catch (const c10::Error &e) {
    CG_LOG_MERROR("Failed to load models");
		throw;
	}
  CG_LOG_MINFO("Model loading complete");
}

at::Tensor Whisper::embed_audio(const at::Tensor& spectrogram) {
  at::NoGradGuard no_grad;  // No gradients.
  const std::vector<torch::jit::IValue> encoder_inputs = {spectrogram};
  const at::Tensor audio_features = m_encoder.forward(encoder_inputs).toTensor();
  return audio_features;
}

bool Whisper::is_multilingual() {
  return m_model_type == ModelType::Multilingual;
}

at::Tensor Whisper::logits(const at::Tensor& tokens, 
                                 const at::Tensor& audio_features,
                                 const int cache_index)
{
  at::NoGradGuard no_grad;  // No gradients.
  const std::vector<torch::jit::IValue> decoder_inputs = {tokens, audio_features, at::tensor({cache_index})};
  const at::Tensor logits = m_decoder.forward(decoder_inputs).toTensor();
  return logits;
}

}  // namespace capgen