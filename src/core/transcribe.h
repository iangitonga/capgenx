#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <functional>

#include "audio/audio.h"
#include "decoder.h"
#include "model.h"
#include "tokenizer.h"
#include "utils.h"

namespace capgen {

/// @brief Transcribe the media file in the given path.
/// @param path path to the media file.
/// @param update_callback A function to call with a progress value in percentage rounded off to the nearest int.
void transcribe(std::string &path, capgen::Whisper &whisper, std::function<void(int)> update_callback);

// Transcribes the media file in the given path while printing the transcription
// in real-time to the console. It also performs transcription timing.
void transcribe_debug(std::string &path);

}; // namespace capgen
