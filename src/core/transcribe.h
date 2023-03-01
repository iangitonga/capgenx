#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <functional>

#include "audio.h"
#include "decoder.h"
#include "model.h"
#include "tokenizer.h"
#include "utils.h"

namespace capgen {

/// @brief Transcribe the media file in the given path.
/// @param path path to the media file.
/// @param update_callback A function to call with a progress value in percentage rounded
///   off to the nearest int.
void transcribe(std::string &path,
                std::shared_ptr<Whisper> whisper,
                int task,
                std::function<void(float)> update_callback);

}; // namespace capgen
