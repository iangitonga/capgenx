#pragma once


#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>


/// @brief This structure holds decoding output results. This struct is created by a call
/// to `alloc_audio_decode_output` function and freed by `free_audio_decode_output`.
typedef struct AudioDecodeOutput {
  // Pointer to the output buffer that must be freed by the caller. Also, although
  // it is a `uint8_t *` it should be interpreted as containing unsigned 16-bit data. uint8_t
  // is used because it allows for nice pointer arithmetic in-terms of bytes which is useful
  // for memory allocation, reallocation and copying.
  uint8_t *buf;
  // Buffer size, in bytes.
  uint64_t tot_buf_size;
  // Buffer size that is currently used, in bytes. This is used during decoding process and
  // when decoding process is done the buffer is properly resized so that the total buf
  // size is equal to used buf size.
  uint64_t used_buf_size;

  // The number of audio samples available.
  int64_t num_samples;
} AudioDecodeOutput;


/// @brief Allocates an `AudioDecodeOutput` structure that is ready to be used. A structure
///  created by this function should be freed by a call to `free_audio_decode_output`.
/// @return A pointer to the created structure or a NULL pointer if alloc failed.
AudioDecodeOutput *capi_alloc_audio_decode_output();


/// @brief Frees the given output structure and sets its pointer to NULL.
/// @param dec_out a double-pointer to the structure to free. A double-pointer allows us
/// to also set the structure pointer to NULL so that it is easier to check in the future,
/// if a given pointer to the structure is freed.
void capi_free_audio_decode_output(AudioDecodeOutput **dec_out);


/// @brief Decodes the audio stream and writes the samples to the decoder output structure. The
///  decoded audio signal is mono-channel, has sample rate of 16000 and its format is pcm_s16le.
/// @param infilepath Path to the media file from which to decode the audio stream.
/// @param dec_out A tructure to hold the output data.
/// @return 0 if the process was successful and -1 if decoding could not be done.
int capi_get_audio_signal(const char *infilepath, AudioDecodeOutput *dec_out);

/// @brief Writes the signal from the given decoder output to the given filepaths
/// as binary. Helpful for debugging and inspection.
/// @return 0 if successful and -1 if something went wrong.
int capi_dump_signal_to_file(const char *infilepath, AudioDecodeOutput *dec_out);


/// Allow/disallow logging info to be shown on the console.
void capi_enable_logging();
void capi_disable_logging();

#ifdef __cplusplus
}
#endif
