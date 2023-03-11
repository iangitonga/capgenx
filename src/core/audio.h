#pragma once

#include "audio/caudio.h"

#include <ATen/ATen.h>


namespace capgen {

class AudioDecoder {
public:
    AudioDecodeOutput *decode_output;
    AudioDecoder(bool enable_logging = false);
    ~AudioDecoder();
    at::Tensor get_audio_spectrogram(const char *infilepath) const;

private:
    // TODO: Does forward slash work on Windows.
    const char *m_mel_filters_path = "./assets/mel_80";

    at::Tensor get_audio_tensor() const;
    at::Tensor get_mel_filters() const;
    at::Tensor get_audio_spectrogram(const at::Tensor& audio) const;
};

}