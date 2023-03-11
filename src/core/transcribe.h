#pragma once

#include "model.h"

#include <string>
#include <functional>
#include <filesystem>
#include <memory>

namespace capgen {

enum TranscriptionTask {
    Transcribe,
    Translate
};

/// @brief Transcribe the media file in the given path.
/// @param path path to the media file.
/// @param update_callback A function to call with a progress value in percentage rounded
///   off to the nearest int.
void transcribe(std::filesystem::path media_filepath,
                std::shared_ptr<Whisper> whisper,
                TranscriptionTask task,
                std::function<void(float)> update_callback);

}; // namespace capgen
