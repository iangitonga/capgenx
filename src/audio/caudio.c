
#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>

#include <math.h>

#include "caudio.h"

#ifdef __cplusplus
}
#endif

// Performance:
// 1HR audio - 125MB memory  - 10sec decoding.
// 3HR audio - 375MB memory - 30sec decoding.


#define OUT_SAMPLE_RATE 16000UL
#define OUT_SAMPLE_SIZE sizeof(int16_t)
// Signed 16-bit planar format.
#define OUT_SAMPLE_FMT AV_SAMPLE_FMT_S16P
#define OUT_CHANNEL_LAYOUT AV_CHANNEL_LAYOUT_MONO


void capi_disable_logging() {
  av_log_set_level(AV_LOG_QUIET);
}

void capi_enable_logging() {
  av_log_set_level(AV_LOG_INFO);
}

uint64_t bytes_to_mb(uint64_t bytes) {
  uint64_t out = (uint64_t)ceil((double_t)bytes / (double_t)1000000);
  return out;
}


AudioDecodeOutput *capi_alloc_audio_decode_output() {
  AudioDecodeOutput *dec_out = (AudioDecodeOutput *)malloc(sizeof(AudioDecodeOutput));
  if (!dec_out)
    return dec_out;
  // Default buffer size is the number of bytes needed to hold 30min-long(1800secs) audio samples.
  uint64_t default_buf_size = OUT_SAMPLE_SIZE * OUT_SAMPLE_RATE * 1800UL;
  dec_out->buf = (uint8_t *)malloc(default_buf_size);
  if (!(dec_out->buf)) {
    free(dec_out);
    fprintf(stderr,
           "[ERROR]: Could not allocate %ldMB for initial audio decoding buffer.\n",
           bytes_to_mb(default_buf_size)
    );
    return NULL;
  }
  fprintf(stdout,
          "[INFO]: Allocated %ldMB for initial audio decoding buffer.\n",
          bytes_to_mb(default_buf_size)
  );
  dec_out->tot_buf_size = default_buf_size;
  dec_out->used_buf_size = 0;
  dec_out->num_samples = 0;
  return dec_out;
}

void capi_free_audio_decode_output(AudioDecodeOutput **dec_out) {
  // Checks ensure that correct results are attained even if this function is called
  // twice on the same object.
  if (!(*dec_out))
    return;
  if (!((*dec_out)->buf)) {
    free(*dec_out);
    *dec_out = NULL;
    return;
  }
  uint64_t buf_size = (*dec_out)->tot_buf_size;
  free((*dec_out)->buf);
  free(*(dec_out));
  *dec_out = NULL;
  fprintf(stdout, "[INFO]: Freed %ldMB audio decoding buffer.\n", bytes_to_mb(buf_size));
}

static int _shrink_decode_output_to_fit(AudioDecodeOutput *dec_out);
static int _decode_audio(const char *infilepath, AudioDecodeOutput *dec_out);

int capi_get_audio_signal(const char *infilepath, AudioDecodeOutput *dec_out) {
  if (!dec_out)
    return -1;
  int ret = _decode_audio(infilepath, dec_out);
  if (ret < 0)
    return -1;
  ret = _shrink_decode_output_to_fit(dec_out);
  if (ret < 0) 
    return -1;
  fprintf(stdout, "[INFO]: Decoded %ldMB successfully.\n", bytes_to_mb(dec_out->used_buf_size));
  return 0;
}


int capi_dump_signal_to_file(const char *infilepath, AudioDecodeOutput *dec_out) {
  if (!dec_out)
    return -1;
  FILE *outfile = fopen(infilepath, "wb");
  if (!outfile)
    return -1;
  fwrite(dec_out->buf, OUT_SAMPLE_SIZE, dec_out->num_samples, outfile);
  fclose(outfile);
}


static int _shrink_decode_output_to_fit(AudioDecodeOutput *dec_out) {
  if (dec_out->used_buf_size == 0)
    return -1;
  if (dec_out->used_buf_size == dec_out->tot_buf_size)
    return 0;
  uint8_t *new_buf = (uint8_t *)realloc(dec_out->buf, dec_out->used_buf_size);
  if (!new_buf) {
    fprintf(stderr,
           "[ERROR]: Failed to shrink from %ldMB to %ldMB.\n",
           bytes_to_mb(dec_out->tot_buf_size),
           bytes_to_mb(dec_out->used_buf_size)
    );
    return -1;
  }
  fprintf(stdout,
          "[INFO]: Shrunk audio buffer from %ldMB to %ldMB.\n",
          bytes_to_mb(dec_out->tot_buf_size),
          bytes_to_mb(dec_out->used_buf_size)
  );
  dec_out->buf = new_buf;
  dec_out->tot_buf_size = dec_out->used_buf_size;
  return 0;
}


///  Copies data from the `inbuf` to the decode output structure.
static int _write_internal(const uint8_t *inbuf,
                           int num_samples,
                           AudioDecodeOutput *dec_out)
{
  size_t inbuf_size = num_samples * OUT_SAMPLE_SIZE;
  uint64_t available_size = dec_out->tot_buf_size - dec_out->used_buf_size;
  if (inbuf_size > available_size) {
    // The reallocation strategy works as follows: We begin with a buffer that can hold up to 30
    // mins of audio data. If it fills up, we double the size to 1hr, then to 2hrs and from there
    // we increase size to hold 1 more hour of content each time it fills up. Since most audio length
    // is between 30min and 3hrs, this strategy should work fine.
    int64_t old_tot_secs = dec_out->tot_buf_size / (OUT_SAMPLE_RATE * OUT_SAMPLE_SIZE);
    int64_t new_tot_secs;
    if (old_tot_secs < 10800)
      new_tot_secs = old_tot_secs * 2;
    else
      new_tot_secs = old_tot_secs + 60;
    size_t new_tot_size = new_tot_secs * OUT_SAMPLE_RATE * OUT_SAMPLE_SIZE;
    uint8_t *new_buf = (uint8_t *)realloc(dec_out->buf, new_tot_size);
    if (!new_buf) {
      fprintf(stderr,
              "[ERROR]: Failed to expand signal buf to %ldMB.\n",
              bytes_to_mb(new_tot_size)
      );
      return -1;
    }
    fprintf(stdout, "[INFO]: Reallocated from %ldMB to %ldMB.\n", bytes_to_mb(dec_out->tot_buf_size), bytes_to_mb(new_tot_size));
    dec_out->buf = new_buf;
    dec_out->tot_buf_size = new_tot_size;
  }
  uint64_t start_offset = dec_out->used_buf_size;
  memcpy(dec_out->buf + start_offset, inbuf, inbuf_size);
  dec_out->used_buf_size = dec_out->used_buf_size + inbuf_size;
  dec_out->num_samples = dec_out->num_samples + num_samples;
  return 0;
}


/// Loops through all the frames in the given packet while saving them to the output structure.
static int _write_frames_to_output(SwrContext *sampler_ctx,
                                   AVCodecContext *codec_ctx,
                                   AVPacket *packet,
                                   AVFrame *frame,
                                   AudioDecodeOutput *dec_out) 
{
  // Decode the coded frame from the packet.
  int ret = avcodec_send_packet(codec_ctx, packet);
  if (ret < 0) {
    fprintf(stderr, "[ERROR]: Failed to decode sent packet.\n");
    return -1;
  }

  while (ret >= 0) {
    // Fetch decoded output data from the decoder into the frame.
    ret = avcodec_receive_frame(codec_ctx, frame);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
      break;
    } else if (ret < 0) {
      fprintf(stderr, "[ERROR]: Failed to fetch sent packet into the frame.\n");
      return -1;
    }

    // Number of samples contained in this frame.
    int n_in_samples = frame->nb_samples;
    // An upper bound on the number of output samples we require given the input samples.
    int64_t n_max_out_samples = av_rescale_rnd(n_in_samples, OUT_SAMPLE_RATE, frame->sample_rate, AV_ROUND_UP);
    size_t tot_out_bytes = n_max_out_samples * OUT_SAMPLE_SIZE;
    uint8_t *outbuf = (uint8_t*)malloc(tot_out_bytes);
    if (!outbuf) {
      fprintf(stderr, "[ERROR]: Failed to allocate %ldMB for resampling.\n", bytes_to_mb(tot_out_bytes));
      return -1;
    }
    // `data[0]` is a pointer to the first channel data.
    const uint8_t **inbuf = (const uint8_t**)&(frame->data[0]);
    /// TODO: Could we use the `dec_out` buffer directly instead of intermediate buffer?
    int n_output_samples = swr_convert(sampler_ctx, &outbuf, n_max_out_samples, inbuf, n_in_samples);
    if (n_output_samples < 0) {
      fprintf(stderr, "[ERROR]: Failed to resample.\n");
      free(outbuf);
      return -1;
    }
    ret = _write_internal(outbuf, n_output_samples, dec_out);
    if (ret < 0) {
      free(outbuf);
      return -1;
    }
    free(outbuf);
  }
  return 0;
}


/// Sets the output values in the given sampler context and initializes it.
static int _set_up_sampler(SwrContext *sampler_ctx,
                          AVCodecContext *codec_ctx,
                          AVCodecParameters *codec_params) 
{
  av_opt_set_chlayout(sampler_ctx, "in_chlayout", &(codec_params->ch_layout), 0);
  av_opt_set_int(sampler_ctx, "in_sample_rate", codec_params->sample_rate, 0);
  av_opt_set_sample_fmt(sampler_ctx, "in_sample_fmt", codec_ctx->sample_fmt, 0);

  AVChannelLayout out_ch_layout = OUT_CHANNEL_LAYOUT;
  av_opt_set_chlayout(sampler_ctx, "out_chlayout", &out_ch_layout, 0);
  av_channel_layout_uninit(&out_ch_layout);
  av_opt_set_int(sampler_ctx, "out_sample_rate", OUT_SAMPLE_RATE, 0);
  av_opt_set_sample_fmt(sampler_ctx, "out_sample_fmt", OUT_SAMPLE_FMT, 0);
  if (swr_init(sampler_ctx) < 0) {
    fprintf(stderr, "[ERROR]: Failed to initialise sampler context.\n");
    return -1;
  }
  return 0;
}


static int _decode_audio_internal(AVFormatContext *fmt_ctx,
                                 AVCodecParameters *codec_params,
                                 int stream_idx,
                                 AudioDecodeOutput *dec_out)
{
  if (codec_params->codec_type != AVMEDIA_TYPE_AUDIO) {
    fprintf(stderr, "[ERROR]: Codec parameters not for audio.\n");
    return -1;
  }
  // Finds a registered decoder with the given codec ID.
  const AVCodec *codec = avcodec_find_decoder(codec_params->codec_id);
  if (!codec) {
    fprintf(stderr, "[ERROR]: Failed to find a registered codec decoder.\n");
    return -1;
  }
  // av_log(NULL, AV_LOG_INFO,
  //  "~Ch=%d, sr=%d, bitrate=%ld\n",
  //   codec_params->ch_layout.nb_channels, codec_params->sample_rate, codec_params->bit_rate
  // );
  // Holds data and information for the decoder.
  AVCodecContext *codec_ctx = avcodec_alloc_context3(codec);
  if (!codec_ctx) {
    fprintf(stderr, "[ERROR]: Codec context alloc failed.\n");
    return -1;
  }
  // Fills the codec context based on the values from the supplied codec parameters.
  int ret = avcodec_parameters_to_context(codec_ctx, codec_params);
  if (ret < 0) {
    fprintf(stderr, "[ERROR]: Failed to fill codec context with codec params.\n");
    goto error_1;
  }
  av_log(NULL, AV_LOG_INFO, "~Raw audio format: %s\n", av_get_sample_fmt_name(codec_ctx->sample_fmt));
  // Initialize the codec context to use the given codec.
  if (avcodec_open2(codec_ctx, codec, NULL) < 0) {
    fprintf(stderr, "[ERROR]: Failed to initialize codec context.\n");
    goto error_1;
  }
  // Holds compressed packet data.
  AVPacket *packet = av_packet_alloc();
  if (!packet) {
    fprintf(stderr, "[ERROR]: Failed to allocate a packet.\n");
    goto error_1;
  }
  // Holds uncompressed(raw) data.
  AVFrame *frame = av_frame_alloc();
  if (!frame) {
    fprintf(stderr, "[ERROR]: Failed to allocate a frame.\n");
    goto error_2;
  }

  // Sets up the resampler;
  SwrContext *sampler_ctx = swr_alloc();
  if (!sampler_ctx) {
    fprintf(stderr, "[ERROR]: Failed to allocate a sampler context.\n");
    goto error_3;
  }
  ret = _set_up_sampler(sampler_ctx, codec_ctx, codec_params);
  if (ret < 0) {
    fprintf(stderr, "[ERROR]: Failed to set sampler values.\n");
    goto error_4;
  }

  // Fetch the next coded frame of the stream into the packet. 
  while(av_read_frame(fmt_ctx, packet) >= 0) {
    // Skip all the packets that do not belong to the audio stream.
    /// TODO: Can we avoid this by going direct to audio stream packets?
    if(packet->stream_index != stream_idx) {
        av_packet_unref(packet);
        continue;
    }
    int ret = _write_frames_to_output(sampler_ctx, codec_ctx, packet, frame, dec_out);
    if (ret < 0)
      goto error_4;
    av_packet_unref(packet);
  }

  // Cleanup
  swr_free(&sampler_ctx);
  av_frame_free(&frame);
  av_packet_free(&packet);
  avcodec_free_context(&codec_ctx);
  return 0;

error_1:
  avcodec_free_context(&codec_ctx);
  return -1;
error_2:
  av_packet_free(&packet);
  avcodec_free_context(&codec_ctx);
  return -1;
error_3:
  av_frame_free(&frame);
  av_packet_free(&packet);
  avcodec_free_context(&codec_ctx);
  return -1;
error_4:
  swr_free(&sampler_ctx);
  av_frame_free(&frame);
  av_packet_free(&packet);
  avcodec_free_context(&codec_ctx);
  return -1;
}


static int _decode_audio(const char *infilepath, AudioDecodeOutput *dec_out) {
  // Holds inforation about format(container) which is used to perform format I/O.
  AVFormatContext *fmt_ctx = avformat_alloc_context();
  if (!fmt_ctx) {
    fprintf(stderr, "[ERROR]: Failed to allocate format context.\n");
    return -1;
  }
  // Opens the input in the given path and reads header information to the
  // given format context.
  int ret = avformat_open_input(&fmt_ctx, infilepath, NULL, NULL);
  if (ret < 0) {
    avformat_free_context(fmt_ctx);
    fprintf(stderr, "[ERROR]: Failed to open format context input.\n");
    return -1;
  }
  // Reads packets of the file to obtain stream information.
  ret = avformat_find_stream_info(fmt_ctx, NULL);
  if (ret < 0) {
    avformat_free_context(fmt_ctx);
    fprintf(stderr, "[ERROR]: Failed to find media file stream information.\n");
    return -1;
  }

  // Decoding phase. Loop through all the streams until we find the first audio stream.
  /// TODO: Allow decoding all available audio streams?
  int found_audio = 0;
  for (int stream_idx = 0; stream_idx < fmt_ctx->nb_streams; ++stream_idx) {
    // Stores codec parameters associated with the corresponding stream.
    AVCodecParameters *codec_params = fmt_ctx->streams[stream_idx]->codecpar;
    // Check if this stream is audio stream. 2.1 - 3
    if (codec_params->codec_type != AVMEDIA_TYPE_AUDIO)
      continue;
    found_audio = 1;
    ret = _decode_audio_internal(fmt_ctx, codec_params, stream_idx, dec_out);
    if (ret < 0) {
      avformat_close_input(&fmt_ctx);
      avformat_free_context(fmt_ctx);
      return -1;
    }
  }
  if (!found_audio)
    fprintf(stdout, "[INFO]: Audio stream not found.\n");
  // Free input data.
  avformat_close_input(&fmt_ctx);
  // Free format context.
  avformat_free_context(fmt_ctx);
  return 0;
}
