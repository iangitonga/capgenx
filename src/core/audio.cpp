#include "audio/caudio.h"
#include "audio.h"
#include "utils.h"


namespace capgen {

AudioDecoder::AudioDecoder(bool enable_logging) {
  if (enable_logging)
    capi_enable_logging();
  else
    capi_disable_logging();
  decode_output = capi_alloc_audio_decode_output();
  if (!decode_output)
    throw std::exception();
}

AudioDecoder::~AudioDecoder() {
  capi_free_audio_decode_output(&decode_output);
}

const at::Tensor AudioDecoder::get_audio_spectrogram(const char *infilepath) const {
  int ret = capi_get_audio_signal(infilepath, decode_output);
  if (ret < 0)
    throw std::exception();
  const at::Tensor audio = get_audio_tensor();
  const at::Tensor spectrogram = get_audio_spectrogram(audio);
  const at::Tensor padded_spectrogram = pad_audio_spectrogram(spectrogram);
  return padded_spectrogram;
}

const at::Tensor AudioDecoder::get_audio_tensor() const {
  auto audio_ref = at::makeArrayRef((int16_t *)(decode_output->buf), decode_output->num_samples);
  at::Tensor audio = at::tensor(audio_ref);
  audio = audio / audio.max();
  audio = audio.to(at::kFloat);
  return audio;
}

const at::Tensor AudioDecoder::get_mel_filters() const {
  FILE *spec_file = fopen(m_mel_filters_path, "rb");
  if (!spec_file)
    throw std::exception();
  int filtersize = 80 * 201;
  // A temporary data to hold the filter data before it is copied to a tensor.
  float buf[filtersize];
  // TODO: Use C++ streams.
  std::fread(buf, sizeof(float_t), filtersize, spec_file);
  std::fclose(spec_file);
  auto filter_ref = at::makeArrayRef(buf, filtersize);
  const at::Tensor filter = at::tensor(filter_ref);
  return filter;
}

const at::Tensor AudioDecoder::get_audio_spectrogram(const at::Tensor& audio) const {
  const at::Tensor window = at::hann_window(400);
  const at::Tensor stft = at::stft(audio, 400, 160, at::nullopt, window, false, true, true);
  const at::Tensor magnitudes = stft.index(
    {at::indexing::Slice(NULL), at::indexing::Slice(NULL, -1)}
  ).abs().square();
  const at::Tensor filters = get_mel_filters();
  const at::Tensor mel_spec = at::mm(filters.view({80, 201}), magnitudes);
  const at::Tensor log_spec = at::clamp(mel_spec, 1e-10).log10();
  const at::Tensor log_spec_max = at::maximum(log_spec, log_spec.max() - 8.0);
  const at::Tensor out_spec = (log_spec_max + 4.0) / 4.0;
  return out_spec.view({1, 80, -1});
}

const at::Tensor AudioDecoder::pad_audio_spectrogram(const at::Tensor& spectrogram) const {
  const int64_t n_audio_frames = spectrogram.size(2);
  if ((n_audio_frames % 3000) == 0)
    return spectrogram;
  const int quotient = exact_div(n_audio_frames, 3000);
  const int64_t pad_diff = ((quotient * 3000) + 3000) - n_audio_frames;
  const at::Tensor padded_spectrogram = at::pad(spectrogram, {0, pad_diff, 0, 0});
  return padded_spectrogram;
}

} // namespace capgen.
