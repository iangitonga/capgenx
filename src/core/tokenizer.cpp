#include <iostream>
#include "tokenizer.h"


namespace capgen {

Tokenizer::Tokenizer() {
  vocab_ = new char[vocab_bufsize_];
  if (!vocab_)
    throw std::exception();
  load_tokenizer_();
}

Tokenizer::~Tokenizer() {
  delete[] vocab_;
}

const char* const Tokenizer::decode_token(int token) const {
  if (token >= vocab_size_) {
    std::cerr << "Token out of vocab range: " << token << std::endl;
    throw std::exception();
  }
  // `vocab_` is the pointer to the buffer start pos, `token_size_` is
  // the offset and `token` is the index.
  return vocab_ + (token * token_size_);
}

const char* const Tokenizer::decode_token_without_ts(int token) const {
  if (token >= vocab_size_)
    throw std::exception();
  if (token < timestamp_begin)
    return vocab_ + (token * token_size_);
  return empty_string;
}

float Tokenizer::time_from_token(int token) const {
  if (token >= vocab_size_ || token < timestamp_begin) {
    std::cerr << "Token out of range: " << token << std::endl;
    throw std::exception();
  }
  return (token - timestamp_begin) * 0.02f;
}

bool Tokenizer::is_timestamp(int token) const {
  if (token >= vocab_size_)
    throw std::exception();
  return token >= timestamp_begin;
}

void Tokenizer::load_tokenizer_() const {
   FILE* vocab_file = fopen(vocab_filename_.c_str(), "r");
  if (!vocab_file)
    throw std::exception();
  char char_hold;
  for (int token_offset = 0; token_offset < vocab_bufsize_; token_offset += token_size_) {
    int char_idx = 0;
    while((char_hold = fgetc(vocab_file)) != EOF && char_hold != '\n' && char_idx < token_size_ - 1) {
      vocab_[token_offset + char_idx] = char_hold;
      char_idx += 1;
    }
    vocab_[token_offset + char_idx] = '\0';
  }
  fclose(vocab_file);
}

} // namespace tokenizer.

