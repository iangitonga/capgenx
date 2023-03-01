#pragma once

#include <torch/script.h>
#include <string>
#include <map>

namespace capgen {

class Whisper {
public:
  // Maximum number of context tokens.
  const int n_ctx = 224;
  std::string m_name;

  Whisper(const std::string &name);
  const at::Tensor embed_audio(const at::Tensor& spectrogram);
  const at::Tensor logits(const at::Tensor& tokens,
                          const at::Tensor& audio_features,
                          const int cache_index);

private:
  torch::jit::script::Module m_encoder;
  torch::jit::script::Module m_decoder;
};

struct ModelInfo {
  const char *name;
  uint32_t disk_size_mb;
  uint32_t dl_size_mb; // download size (compressed).
  uint32_t mem_usage_mb;
  const char* url;
};

static std::map<std::string, ModelInfo> MODELS = {
  {"tiny", {"tiny", 152, 84, 500, "https://huggingface.co/iangitonga/capgen_tiny/resolve/main/tiny.zip"}},
  // {"base", {"base", 292, 161, 800, "https://huggingface.co/iangitonga/capgen_tiny/resolve/main/base.zip"}}
};

} // namespace capgen
