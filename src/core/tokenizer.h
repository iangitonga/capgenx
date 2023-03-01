#pragma once

#include <string>
#include <map>
#include <unordered_map>

namespace capgen {

class Tokenizer {
public:
  int sot = 50258;
  int eot = 50257;
  static const int transcribe = 50359;
  static const int translate = 50358;
  int no_speech = 50362;
  int no_timestamps = 50363;
  int timestamp_begin = 50364;
  int timestamp_end = 51864;
  int startoflm = 50360;
  int startofprev = 50361;
  // Length of prompt/begin sequence. [sot, lang, task]
  int sample_begin = 3;

  // used to decode without timestamps.
  const char* const empty_string = "\0";

  Tokenizer();
  ~Tokenizer();
  const char* const decode_token(int token) const;
  const char* const decode_token_without_ts(int token) const;
  float time_from_token(int token) const;
  bool is_timestamp(int token) const;

private:
  const std::string vocab_filename_ = "./assets/multilingual_vocab";
  // Max number of characters a token can have. This value is large enough to contain the
  // longest token.
  const int m_token_size = 40;
  // Number of tokens in the vocab.
  const int m_vocab_size = 51865;
  // Max number of chars that the vocab can contain.
  const int m_vocab_bufsize = 51865 * 40;
  // The buffer (2MB size) which holds all the decoded tokens. Conceptually, it contains
  // fixed-size slots for each token in the vocabulary plus a null termination character.
  // That size is chosen so that it is large enough to hold the biggest token.
  // When creating the vocabulary, we read each token from a file, add it to its slot in
  // the buffer and then add a null-termination character so that for tokens smaller than
  // the slots, we know where they end. The tokens are stored in the order of their
  // corresponding numeric value so the first slot contains the token that when decoded,
  // maps to numeric value 0. That allows for very fast decoding because we can index a
  // token directly by adding token offset to to the buffer pointer. We could potentially
  // use something like hash map to make it much simpler but we would have to allocate 
  // memory for each slot independently which would lead to awful memory fragmentation.
  // TODO: Can we embed the whole dictionary in the source and then use string views to
  // access it?
  char *m_vocab;

  void load_tokenizer() const;
  void validate_token_range(int token) const;
};

} // namespace capgen.