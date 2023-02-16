#include <torch/script.h>
#include <chrono>
#include <cstdlib>
#include <cmath>
#include <string>
#include <vector>
#include <filesystem>
#include <functional>

#include "audio.h"
#include "decoder.h"
#include "model.h"
#include "tokenizer.h"
#include "transcribe.h"
#include "utils.h"


void capgen::transcribe(std::string &path, capgen::Whisper &whisper, std::function<void(int)> update_callback) {
  std::filesystem::path infilepath(path);
  // Load audio and tokenizer.
  const capgen::AudioDecoder audio_decoder = capgen::AudioDecoder();
  const torch::Tensor spectrogram = audio_decoder.get_audio_spectrogram(infilepath.c_str());
  const capgen::Tokenizer tokenizer = capgen::Tokenizer();
  // Here we predict the number of iterations that the model will take to transcribe the audio.
  // At first, it may seem that the number of iterations is simply the number of 30-second 
  // chunks in the audio spectrogram. But that is not exact because the model does not usually
  // transcribe a 30-second audio chunk fully. In many cases, the model transcribes up to 26th
  // or 28th second(this info is in the predicted timestamps). Therefore, the next segment to be
  // fed to the model has to begin where the model previously left. For a short audio of length
  // 20min, a naive calculation predicts about 60 iterations but we end up performing 65 instead.
  // So, in order to reserve enough memory and update the callback function properly, we need to
  // take into account that longer audio require extra iterations to complete transcription. The
  // current strategy is to assume for that every 10 30-second chunk segments, we require an extra
  // iteration. This should work fine in most cases but in the future, a better method can be
  // obtained by performing a series of transcriptions while recording statictics.
  int n_segments = capgen::exact_div(spectrogram.size(-1), 3000);
  // We also add a 1 to account for very short videos.
  n_segments += (int)std::round((float)n_segments / 10.0f) + 1;
  std::vector<capgen::SegmentTranscription> transcriptions;
  transcriptions.reserve(n_segments);
  int index_offset = 0;
  int segment_index = 0;
  const float max_percentage = 95.0f;
  while (index_offset + 3000 < spectrogram.size(-1)) {
    torch::Tensor segment_spec = spectrogram.index_select(-1, torch::arange(index_offset, index_offset + 3000));
    capgen::greedy_decode_segment(whisper, tokenizer, segment_spec, transcriptions, segment_index);
    index_offset += transcriptions[segment_index].end_time * 100;
    segment_index += 1;
    // This will start for 1% to max percentage.
    int prog_percentage = ((float)segment_index / (float)n_segments) * max_percentage;
    std::cout << "[CG]: Prog - " << prog_percentage << std::endl;
    // Sometimes, we may have under-approximated but we must not exceed max percentage.
    if (prog_percentage <= max_percentage)
      update_callback(prog_percentage);
  }
  std::filesystem::path outfilepath = infilepath.replace_extension("srt");
  capgen::save_to_srt(transcriptions, tokenizer, outfilepath.string());
  update_callback(100);
  std::cout << "[CG]: Transcription Complete" << std::endl;
}

void capgen::transcribe_debug(std::string &path) {
  std::filesystem::path infilepath(path);
  //Load model
  capgen::Whisper whisper = capgen::Whisper();
  // Load audio and tokenizer.
  const capgen::AudioDecoder audio_decoder = capgen::AudioDecoder(true);
  const torch::Tensor spectrogram = audio_decoder.get_audio_spectrogram(infilepath.c_str());
  const capgen::Tokenizer tokenizer = capgen::Tokenizer();
  const int n_segments = capgen::exact_div(spectrogram.size(-1), 3000);
  const capgen::TranscribingTimer timer{n_segments};
  int index_offset = 0;
  int segment_index = 0;
  while (index_offset + 3000 < spectrogram.size(-1)) {
    torch::Tensor segment_spec = spectrogram.index_select(-1, torch::arange(index_offset, index_offset + 3000));
    capgen::greedy_decode_segment_realtime(whisper, tokenizer, segment_spec, segment_index);
    index_offset += 3000;
    segment_index += 1;
  }
  timer.stop();
}
