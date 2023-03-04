#pragma once

#include <torch/script.h>
#include <string>
#include <map>

namespace capgen {

enum ModelType {
  English,
  Multilingual
};

class Whisper {
public:
  // Maximum number of context tokens.
  const int n_ctx = 224;
  std::string m_name;
  ModelType m_model_type;

  Whisper(const std::string &name, ModelType model_type);
  at::Tensor embed_audio(const at::Tensor& spectrogram);
  bool is_multilingual();
  at::Tensor logits(const at::Tensor& tokens,
                          const at::Tensor& audio_features,
                          const int cache_index);

private:
  torch::jit::script::Module m_encoder;
  torch::jit::script::Module m_decoder;
};

struct ModelInfo {
  const char *name;
  uint32_t dl_size_mb; // download size (compressed).
  uint32_t mem_usage_mb;
  const char* url;
};

static std::map<std::string, ModelInfo> MODELS = {
  {"tiny",  {"tiny",   178,   500, "https://huggingface.co/iangitonga/capgen_models/resolve/main/tiny.zip"}},
  {"base",  {"base",   338,   800, "https://huggingface.co/iangitonga/capgen_models/resolve/main/base.zip"}},
  {"small", {"small", 1140,  1000, "https://huggingface.co/iangitonga/capgen_models/resolve/main/small.zip"}}
};

} // namespace capgen
