#include "audio/caudio.h"
#include "audio.h"
#include "log.h"
#include "utils.h"


namespace capgen {

AudioDecoder::AudioDecoder(bool enable_logging)
{
    if (enable_logging)
        capi_enable_logging();
    else
        capi_disable_logging();
    decode_output = capi_alloc_audio_decode_output();
    if (!decode_output)
        throw std::exception();
}

AudioDecoder::~AudioDecoder()
{
    capi_free_audio_decode_output(&decode_output);
}

at::Tensor AudioDecoder::get_audio_spectrogram(const char *infilepath) const
{
    int ret = capi_get_audio_signal(infilepath, decode_output);
    if (ret < 0)
        throw std::exception();
    const at::Tensor audio = get_audio_tensor();
    const at::Tensor spectrogram = get_audio_spectrogram(audio);
    return spectrogram;
}

at::Tensor AudioDecoder::get_audio_tensor() const
{
    auto audio_ref = at::makeArrayRef((int16_t *)(decode_output->buf), decode_output->num_samples);
    at::Tensor audio = at::tensor(audio_ref);
    audio = audio / audio.max();
    audio = audio.to(at::kFloat);
    return audio;
}

at::Tensor AudioDecoder::get_mel_filters() const
{
    FILE *spec_file = fopen(m_mel_filters_path, "rb");
    if (!spec_file)
    {
        CG_LOG_ERROR("Failed to open file %s", m_mel_filters_path);
        throw std::exception();
    }
    int filtersize = 80 * 201;
    // A temporary data to hold the filter data before it is copied to a tensor.
    float buf[filtersize];
    // TODO: Use C++ streams.
    int read_bytes = std::fread(buf, sizeof(float_t), filtersize, spec_file);
    if (read_bytes < filtersize)
        CG_LOG_ERROR("Read mel filter data is less than expected: expected=%d, read=%d", filtersize, read_bytes);
    std::fclose(spec_file);
    auto filter_ref = at::makeArrayRef(buf, filtersize);
    at::Tensor filter = at::tensor(filter_ref);
    return filter;
}

at::Tensor AudioDecoder::get_audio_spectrogram(const at::Tensor& audio) const
{
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

} // namespace capgen.
