#pragma once

#include <ATen/ATen.h>
#include "audio/caudio.h"


namespace capgen {

class AudioDecoder {
public:
  AudioDecodeOutput *decode_output;
  AudioDecoder(bool enable_logging = false);
  ~AudioDecoder();
  const at::Tensor get_audio_spectrogram(const char *infilepath) const;

private:
  // TODO: Does forward slash work on Windows.
  const char *m_mel_filters_path = "./assets/mel_80";

  const at::Tensor get_audio_tensor() const;
  const at::Tensor get_mel_filters() const;
  const at::Tensor get_audio_spectrogram(const at::Tensor& audio) const;
  // Pads the spectrogram to ensure the last segment has 3000 frames.
  // Only necessary for realtime transcription.
  const at::Tensor pad_audio_spectrogram(const at::Tensor& spectrogram) const;
};

}