#pragma once

#include "audio/caudio.h"

#include <ATen/ATen.h>


namespace capgen {

// Decodes audio from a given media filepath. The only reason it exists as a class
// is to delete the audio signal buffer upon destruction.
class AudioPreprocessor {
public:
    AudioPreprocessor(bool enable_logging = false);
    ~AudioPreprocessor();
    at::Tensor get_audio_spectrogram(const char *infilepath) const;

private:
    // TODO: Does forward slash work on Windows.
    const char * const m_mel_filters_path = "./assets/mel_80";
    AudioDecodeOutput *m_decode_output;

    at::Tensor get_audio_tensor() const;
    at::Tensor get_mel_filters() const;
    at::Tensor get_audio_spectrogram(const at::Tensor& audio) const;
};

}