#include "audio.h"
#include "decoder.h"
#include "log.h"
#include "model.h"
#include "tokenizer.h"
#include "transcribe.h"
#include "utils.h"

#include <torch/script.h>

#include <cstdlib>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>



static at::Tensor pad_or_trim(at::Tensor &spectrogram, int start_frame_pos)
{
    int end_frame_pos = start_frame_pos + 3000;
    const int64_t total_frames = spectrogram.size(-1);
    if (total_frames - start_frame_pos >= 3000)
        return spectrogram.index_select(-1, at::arange(start_frame_pos, end_frame_pos));
    else
    {
        // Add the required number of frames.
        const int needed_extra_frames = 3000 - (total_frames - start_frame_pos);
        at::Tensor padded_spectrogram = at::pad(spectrogram, {0, needed_extra_frames, 0, 0});
        return padded_spectrogram.index_select(-1, at::arange(start_frame_pos, end_frame_pos));;
    }
}


void capgen::transcribe(std::filesystem::path media_filepath,
                        std::shared_ptr<Whisper> whisper,
                        TranscriptionTask task,
                        std::function<void()> trx_start_callback,
                        std::function<void(float)> trx_update_callback)
{
    CG_LOG_INFO("Transcription process started for file: %s", media_filepath.c_str());

    // Load audio and tokenizer.
    const capgen::AudioPreprocessor audio_preprocessor;
    at::Tensor spectrogram = audio_preprocessor.get_audio_spectrogram(media_filepath.c_str());
    capgen::Tokenizer tokenizer(capgen::TokenizerType::Tk_English);

    // Detect language spoken in the audio.
    int language_id;
    if (whisper->is_multilingual())
    {
        tokenizer = std::move(capgen::Tokenizer(capgen::TokenizerType::Tk_Multilingual));
        language_id = capgen::detect_language(spectrogram, whisper, tokenizer);
        const char *language_id_str = tokenizer.decode_token(language_id);
        CG_LOG_INFO("Detected language: code=%s,  id=%d", language_id_str, language_id);
    }
    else {
        language_id = capgen::Tokenizer::s_english_token;
        CG_LOG_MINFO("Using English model");
    }

    int n_segments = capgen::exact_div(spectrogram.size(-1), 3000);
    // Contains the transcription for every segment.
    std::vector<capgen::SegmentTranscription> transcriptions;
    transcriptions.reserve(n_segments);

    const float total_frames = spectrogram.size(-1);
    float frames_transcribed = 0;
    uint32_t segment_idx = 0;
    // seek is the position of the frame of the segment we should transcribe next.
    uint32_t seek = 0;
    const float max_percentage = 99.0f;
    const capgen::TranscribingTimer timer;

    trx_start_callback();
    while (seek < spectrogram.size(-1))
    {
        at::Tensor segment_spec = pad_or_trim(spectrogram, seek);

        capgen::beamsearch_decode_segment(segment_spec, task, language_id, segment_idx, whisper, tokenizer, transcriptions);
        // capgen::greedy_decode_segment(segment_spec, task, language_id, segment_idx, whisper, tokenizer, transcriptions);
        frames_transcribed += transcriptions[segment_idx].m_end_time * 100;
        seek += (int)(transcriptions[segment_idx].m_end_time * 100);
        segment_idx += 1;

        float prog_percentage = (frames_transcribed / total_frames) * max_percentage;
        // We must not exceed max percentage.
        if (prog_percentage <= max_percentage)
            trx_update_callback(prog_percentage);
        CG_LOG_DEBUG("Transcription progress: (%d%)", (int)prog_percentage);
    }

    std::filesystem::path outfilepath = media_filepath.replace_extension("srt");
    capgen::save_to_srt(transcriptions, tokenizer, outfilepath.string());
    trx_update_callback(100.0f);
    CG_LOG_MINFO("Transcription Complete");
    timer.stop(segment_idx);
}