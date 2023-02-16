#include <torch/torch.h>
#include "caudio.h"
#include "audio.h"
#include "utils.h"


namespace capgen {

AudioDecoder::AudioDecoder(bool enable_logging) {
  if (enable_logging)
    capi_enable_logging();
  else
    capi_disable_logging();
  decode_output = capi_alloc_audio_decode_output();
  // TODO: Improve exception handling.
  if (!decode_output)
    throw std::exception();
}

AudioDecoder::~AudioDecoder() {
  capi_free_audio_decode_output(&decode_output);
}

const torch::Tensor AudioDecoder::get_audio_spectrogram(const char *infilepath) const {
  int ret = capi_get_audio_signal(infilepath, decode_output);
  if (ret < 0)
    throw std::exception();
  const torch::Tensor audio = get_audio_tensor_();
  const torch::Tensor spectrogram = get_audio_spectrogram_(audio);
  const torch::Tensor padded_spectrogram = pad_audio_spectrogram_(spectrogram);
  return padded_spectrogram;
}

const torch::Tensor AudioDecoder::get_audio_tensor_() const {
  auto audio_ref = torch::makeArrayRef((int16_t *)decode_output->buf, decode_output->num_samples);
  auto audio_opts = torch::TensorOptions(torch::kFloat32);
  torch::Tensor audio = torch::tensor(audio_ref, audio_opts);
  audio = audio / audio.max();
  return audio;
}

const torch::Tensor AudioDecoder::get_mel_filters_() const {
  FILE *spec_file = fopen(mel_filters_path_.c_str(), "rb");
  if (!spec_file)
    throw std::exception();
  int filtersize = 80 * 201;
  // A temporary data to hold the filter data before it is copied to a tensor.
  float buf[filtersize];
  // TODO: Use C++ streams.
  fread(buf, sizeof(float), filtersize, spec_file);
  fclose(spec_file);
  auto filter_opts = torch::TensorOptions(torch::kFloat32);
  auto filter_ref = torch::makeArrayRef(buf, filtersize);
  const torch::Tensor filter = torch::tensor(filter_ref, filter_opts);
  return filter;
}

const torch::Tensor AudioDecoder::get_audio_spectrogram_(const torch::Tensor& audio) const {
  const torch::Tensor window = torch::hann_window(400);
  const torch::Tensor stft = torch::stft(audio, 400, 160, torch::nullopt, window, false, true, true);
  const torch::Tensor magnitudes = stft.index(
    {torch::indexing::Slice(NULL), torch::indexing::Slice(NULL, -1)}
  ).abs().square();
  const torch::Tensor filters = get_mel_filters_();
  const torch::Tensor mel_spec = torch::mm(filters.view({80, 201}), magnitudes);
  const torch::Tensor log_spec = torch::clamp(mel_spec, 1e-10).log10();
  const torch::Tensor log_spec_max = torch::maximum(log_spec, log_spec.max() - 8.0);
  const torch::Tensor out_spec = (log_spec_max + 4.0) / 4.0;
  return out_spec.view({1, 80, -1});
}

const torch::Tensor AudioDecoder::pad_audio_spectrogram_(const torch::Tensor& spectrogram) const {
  const int64_t n_audio_frames = spectrogram.size(2);
  if ((n_audio_frames % 3000) == 0)
    return spectrogram;
  const int quotient = exact_div(n_audio_frames, 3000);
  const int64_t pad_diff = ((quotient * 3000) + 3000) - n_audio_frames;
  const torch::Tensor padded_spectrogram = torch::pad(spectrogram, {0, pad_diff, 0, 0});
  return padded_spectrogram;
}

} // namespace capgen.
