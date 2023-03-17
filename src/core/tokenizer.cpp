#include "tokenizer.h"
#include "log.h"


namespace capgen {

Tokenizer::Tokenizer(TokenizerType tokenizer_type)
{
    m_tokenizer_type = tokenizer_type;
    if (m_tokenizer_type == TokenizerType::English)
        init_english_tokenizer();
    else
        init_multilingual_tokenizer();
    // Vocab init.
    m_vocab = new char[m_vocab_bufsize];
    load_vocabulary();
}


Tokenizer &capgen::Tokenizer::operator=(Tokenizer &&other) noexcept
{
    if (this != &other)
    {
        m_tokenizer_type = other.m_tokenizer_type;

        m_sot = other.m_sot;
        m_eot = other.m_eot;
        m_transcribe = other.m_transcribe;
        m_translate = other.m_translate;
        m_no_speech = other.m_no_speech;
        m_no_timestamps = other.m_no_timestamps;
        m_timestamp_begin = other.m_timestamp_begin;
        m_timestamp_end = other.m_timestamp_end;

        m_vocab_filepath = other.m_vocab_filepath;
        m_prompt_length = other.m_prompt_length;
        m_vocab_size = other.m_vocab_size;
        m_vocab_bufsize = other.m_vocab_bufsize;

        delete[] m_vocab;
        // Move the other vocab here.
        m_vocab = other.m_vocab;
        other.m_vocab = nullptr;
    }
    return *this;
}

Tokenizer::~Tokenizer()
{
  delete[] m_vocab;
}

void Tokenizer::init_english_tokenizer()
{
    m_sot = 50257;
    m_eot = 50256;
    m_transcribe = 50358;
    m_translate = 50357;
    m_no_speech = 50361;
    m_no_timestamps = 50362;
    m_timestamp_begin = 50363;
    m_timestamp_end = 51863;

    m_prompt_length = 1;
    m_vocab_filepath = "./assets/english_vocab";
    m_vocab_size = 50256;
    m_vocab_bufsize = m_vocab_size * m_word_size;
}


void Tokenizer::init_multilingual_tokenizer()
{
    m_sot = 50258;
    m_eot = 50257;
    m_transcribe = 50359;
    m_translate = 50358;
    m_no_speech = 50362;
    m_no_timestamps = 50363;
    m_timestamp_begin = 50364;
    m_timestamp_end = 51864;

    m_prompt_length = 3;
    m_vocab_filepath = "./assets/multilingual_vocab";
    m_vocab_size = 50364;
    m_vocab_bufsize = m_vocab_size * m_word_size;
}

bool Tokenizer::is_multilingual() const
{
    return m_tokenizer_type == TokenizerType::Multilingual;
}

const char* const Tokenizer::decode_token(int token) const
{
    validate_token_range(token);
    // `vocab_` is the pointer to the buffer start pos, `token_size_` is
    // the offset and `token` is the index.
    return m_vocab + (token * m_word_size);
}

float Tokenizer::decode_timestamp_token(int token) const
{
    if (token > m_timestamp_end || token < m_timestamp_begin)
    {
        CG_LOG_DEBUG("Token %d out of range", token);
        throw std::exception();
    }
    return (token - m_timestamp_begin) * 0.02f;
}

bool Tokenizer::is_timestamp(int token) const
{
  validate_token_range(token);
  return token >= m_timestamp_begin;
}

void Tokenizer::load_vocabulary()
{
    FILE* vocab_file = std::fopen(m_vocab_filepath.c_str(), "r");
    if (!vocab_file) {
        CG_LOG_ERROR("Failed to open vocab file: %s", m_vocab_filepath.c_str());
        throw std::exception();
    }

    uint8_t char_hold;
    for (int token_offset = 0; token_offset < m_vocab_bufsize; token_offset += m_word_size)
    {
        uint32_t char_idx = 0;
        while((char_hold = std::fgetc(vocab_file)) != EOF && char_hold != '\n' && char_idx < m_word_size - 1)
        {
          m_vocab[token_offset + char_idx] = char_hold;
          char_idx += 1;
        }
        m_vocab[token_offset + char_idx] = '\0';
    }
    fclose(vocab_file);
}

void Tokenizer::validate_token_range(int token) const
{
    if (token > m_timestamp_end) {
        CG_LOG_DEBUG("Token %d out of range", token);
        throw std::exception();
    }
}

} // namespace capgen.

