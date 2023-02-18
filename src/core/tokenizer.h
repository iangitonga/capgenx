#pragma once

#include <string>

namespace capgen {

class Tokenizer {

private:
  const std::string vocab_filename_ = "./assets/multilingual_vocab";
  // Max number of characters a token can have. This value is large enough to contain the
  // longest token.
  const int token_size_ = 40;
  // Number of tokens in the vocab.
  const int vocab_size_ = 51865;
  // Max number of chars that the vocab can contain.
  const int vocab_bufsize_ = 51865 * 40;
  // The buffer (2MB size) which holds all the decoded tokens. Conceptually, it contains fixed-size
  // slots for each token in the vocabulary plus a null termination character. That size is chosen so
  // that it is large enough to hold the biggest token. When creating the vocabulary, we read each token
  // from a file, add it to its slot in the buffer and then add a null-termination character so that for
  // tokens which are smaller than the slots, we know where they end. The tokens are stored in the order
  // of their numeric value. So the first slot contains the token that when decoded, maps to numeric value
  // 0. That allows for very fast decoding because we can index a token directly by adding token offset to
  // to the buffer pointer. We could potentially use something like hash map to make it simpler but
  // we would have to allocate memory for each slot independently which would save memory but would lead to
  // memory fragmentation.
  char *vocab_;

  void load_tokenizer_() const;

public:
  int sot = 50258;
  int eot = 50257;
  int transcribe = 50359;
  int translate = 50358;
  int no_speech = 50362;
  int no_timestamps = 50363;
  int timestamp_begin = 50364;
  int timestamp_end = 51864;
  // Length of prompt/begin sequence. [sot, lang, task]
  int sample_begin = 3;
  // Used to mark end of a segment's transcription failure.
  int fail_marker = 7612;

  // used to decode without timestamps.
  const char* const empty_string = "\0";

  Tokenizer();
  ~Tokenizer();
  const char* const decode_token(int token) const;
  const char* const decode_token_without_ts(int token) const;
  float time_from_token(int token) const;
  bool is_timestamp(int token) const;
};

} // namespace capgen.