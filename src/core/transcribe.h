#pragma once

#include "model.h"

#include <string>
#include <functional>
#include <filesystem>
#include <memory>

namespace capgen {

enum class TranscriptionTask { Transcribe, Translate };

enum TranscriptionDecoder { Greedy, BeamSearch };

/// @brief Transcribe the media file in the given path.
/// @param path path to the media file.
/// @param update_callback A function to call with a progress value in percentage rounded
///   off to the nearest int.
void transcribe(std::filesystem::path media_filepath,
                std::shared_ptr<Whisper> whisper,
                TranscriptionTask task,
                TranscriptionDecoder decoder,
                std::function<void()> trx_start_callback,
                std::function<void(float)> trx_update_callback);

}; // namespace capgen
