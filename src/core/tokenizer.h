#pragma once

#include <ATen/ATen.h>
#include <string>


namespace capgen {

struct TokenizerSpecialTokens {
  int sot;
  int eot;
  int transcribe;
  int translate;
  int no_speech;
  int no_timestamps;
  int timestamp_begin;
  int timestamp_end;
};

/// We only have two tokenizer types, namely, English tokenizer whose vocabulary
/// contains english words only and multilingual conterpart whose vocabulary contains
/// english and other languages words.
enum TokenizerType {
  Tk_English,
  Tk_Multilingual
};



class Tokenizer {
public:
  static const int s_english_token = 50258;

  TokenizerSpecialTokens m_special_tokens;
  // Length of prompt/begin sequence.
  int m_prompt_length;

  Tokenizer(TokenizerType tokenizer_type);
  // Copy constructors and are explicitly deleted to prevent the compiler from generating copy
  // constructors that copy the internal pointers. We also don't need them currently but that
  // is subject to change.
  Tokenizer(const Tokenizer&) = delete;
  Tokenizer &operator=(const Tokenizer &other) = delete;
  Tokenizer(const Tokenizer &&other) = delete;  // Also not needed.
  Tokenizer &operator=(Tokenizer &&other) noexcept;
  ~Tokenizer();
  const char* const decode_token(int token) const;
  float decode_timestamp_token(int token) const;
  bool is_timestamp(int token) const;
  bool is_multilingual() const;

private:
  TokenizerType m_tokenizer_type;

  // Path to the vocabulary file.
  std::string m_vocab_filepath;

  // Memory size, in bytes, of allocated for each word.
  const int m_word_size = 50;

  // Total number of words in the vocabulary. 
  int m_vocab_size;

  // Size of the vocab buffer, in bytes.
  int m_vocab_bufsize;

  // The buffer (2MB size) which holds all the vocab words. Conceptually, it contains
  // fixed-size slots for each word in the vocabulary. The slot size is large enough to 
  // hold the longest word.
  // When creating the vocabulary, we read each word from a file, add it to its slot in
  // the buffer and then add a null-termination character so that for words smaller than
  // the slots, we know where they end. The words are stored in the order of their
  // corresponding token value so the first slot contains the word that when decoded,
  // maps to token value 0. That allows for very fast decoding because we can index a
  // word directly by adding an offset to to the buffer pointer. We could potentially
  // use something like hash map to make it much simpler but we would have to allocate 
  // memory for each slot independently which would lead to awful memory fragmentation
  // plus the overhead of creating the map structure.
  // TODO: Can we embed the whole dictionary in the source.
  char *m_vocab;

  void init_english_tokenizer();
  void init_multilingual_tokenizer();
  void load_vocabulary();
  void validate_token_range(int token) const;
};


// All non-language tokens that should be suppressed during decoding.
static const at::Tensor s_ENGLISH_TOKENIZER_BAD_TOKENS = at::tensor(
  {
    1, 2, 7, 8, 9, 10, 14, 25, 26, 27, 28, 29, 31, 58, 59, 60, 61, 62, 63, 90, 91, 92,
    93, 357, 366, 438, 532, 685, 705, 796, 930, 1058, 1220, 1267, 1279, 1303, 1343, 1377,
    1391, 1635, 1782, 1875, 2162, 2361, 2488, 3467, 3880, 4008, 4211, 4600, 4808, 5299,
    5855, 6329, 7203, 8864, 9609, 9959, 10221, 10563, 10786, 11420, 11709, 11907, 13163,
    13697, 13700, 14808, 15306, 16410, 16791, 17174, 17992, 19203, 19510, 20368, 20724,
    22305, 22935, 23090, 27007, 29113, 30109, 30420, 30906, 33409, 34949, 40283, 40493, 
    40549, 41906, 46111, 47282, 49146, 49704, 50361
  },
  at::TensorOptions(at::kLong)
);

static const at::Tensor s_MULTILINGUAL_TOKENIZER_BAD_TOKENS = at::tensor(
  {
    1, 2, 7, 8, 9, 10, 14, 25, 26, 27, 28, 29, 31, 58, 59, 60, 61, 62, 63, 90, 91, 92,
    93, 220, 359, 503, 522, 542, 873, 893, 902, 918, 922, 931, 1350, 1853, 1982, 2460, 
    2627, 3246, 3253, 3268, 3536, 3846, 3961, 4183, 4667, 6585, 6647, 7273, 9061, 9383,
    10428, 10929, 11938, 12033, 12331, 12562, 13793, 14157, 14635, 15265, 15618, 16553,
    16604, 18362, 18956, 20075, 21675, 22520, 26130, 26161, 26435, 28279, 29464, 31650,
    32302, 32470, 36865, 42863, 47425, 49870, 50254, 50258, 50360, 50361, 50362
  },
  at::TensorOptions(at::kLong)
);

} // namespace capgen.