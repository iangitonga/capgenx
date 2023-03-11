#pragma once

#include "model.h"
#include "tokenizer.h"
#include "transcribe.h"

#include <ATen/ATen.h>

#include <vector>

 
namespace capgen {

int detect_language(const at::Tensor &spectrogram,
                    std::shared_ptr<Whisper>,
                    const Tokenizer &tokenizer);

// Represents a transcription with a start time, text and end time. For instance,
// <0.00>The quick brown fox jumped over the lazy dog<2.00>
class TimestampedTranscription {
public:
    std::vector<uint32_t> m_text_tokens;
    float m_start_time, m_end_time;

    TimestampedTranscription();
};


// Represents full transcription of a 30-second segment. For instance, 
//<0.00>The quick brown fox jumped over the lazy dog<2.00><2.00>Some other content
// <3.12>...<28.12>Final transcription.<30.00>.
// NOTE: It does not necessarily need to end at <30.00>. In fact, it almost always
// end at a lower timestamp.
class SegmentTranscription {
public:
    std::vector<TimestampedTranscription> sub_segments;
    uint32_t m_segment_index;
    float m_end_time;

    SegmentTranscription(std::vector<uint32_t>& tokens,
                       uint32_t segment_index,
                       const Tokenizer& tokenizer);
};


void greedy_decode_segment(const at::Tensor& spectrogram,
                           TranscriptionTask task,
                           const uint32_t language_id,
                           const uint32_t segment_index,
                           std::shared_ptr<Whisper>,
                           const Tokenizer& tokenizer,
                           std::vector<SegmentTranscription>& out_transcriptions);


void beamsearch_decode_segment(const at::Tensor& spectrogram,
                           TranscriptionTask task,
                           const uint32_t language_id,
                           const uint32_t segment_index,
                           std::shared_ptr<Whisper>,
                           const Tokenizer& tokenizer,
                           std::vector<SegmentTranscription>& out_transcriptions);

// TODO: Should probably in utils.h
void save_to_srt(const std::vector<SegmentTranscription>& transcription,
                 const Tokenizer& tokenizer,
                 const std::string& filename);

} // namespace capgen

