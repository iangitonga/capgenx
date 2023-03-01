#include <iostream>
#include "tokenizer.h"


namespace capgen {

Tokenizer::Tokenizer() {
  m_vocab = new char[m_vocab_bufsize];
  if (!m_vocab) {
    std::cerr << "[ERROR]: Failed to allocate 2MB vocab buffer.\n";
    throw std::exception();
  }
  load_tokenizer();
}

Tokenizer::~Tokenizer() {
  delete[] m_vocab;
}

const char* const Tokenizer::decode_token(int token) const {
  validate_token_range(token);
  // `vocab_` is the pointer to the buffer start pos, `token_size_` is
  // the offset and `token` is the index.
  return m_vocab + (token * m_token_size);
}

const char* const Tokenizer::decode_token_without_ts(int token) const {
  validate_token_range(token);
  if (token < timestamp_begin)
    return m_vocab + (token * m_token_size);
  return empty_string;
}

float Tokenizer::time_from_token(int token) const {
  if (token >= m_vocab_size || token < timestamp_begin) {
    std::cerr << "Token out of range: " << token << std::endl;
    throw std::exception();
  }
  return (token - timestamp_begin) * 0.02f;
}

bool Tokenizer::is_timestamp(int token) const {
  validate_token_range(token);
  return token >= timestamp_begin;
}

void Tokenizer::load_tokenizer() const {
   FILE* vocab_file = fopen(vocab_filename_.c_str(), "r");
  if (!vocab_file)
    throw std::exception();
  char char_hold;
  for (int token_offset = 0; token_offset < m_vocab_bufsize; token_offset += m_token_size) {
    int char_idx = 0;
    while((char_hold = fgetc(vocab_file)) != EOF && char_hold != '\n' && char_idx < m_token_size - 1) {
      m_vocab[token_offset + char_idx] = char_hold;
      char_idx += 1;
    }
    m_vocab[token_offset + char_idx] = '\0';
  }
  fclose(vocab_file);
}

void Tokenizer::validate_token_range(int token) const {
  if (token >= m_vocab_size) {
    std::cerr << "[ERROR]: Token out of vocab range: " << token << std::endl;
    throw std::exception();
  }
}

} // namespace capgen.

