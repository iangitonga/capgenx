#pragma once

#include <ATen/ATen.h>

#include <string>


namespace capgen {

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

    int prompt_length() const { return m_prompt_length; }
    int sot() const { return m_sot; }
    int eot() const { return m_eot; }
    int transcribe() const { return m_transcribe; }
    int translate() const { return m_translate; }
    int no_speech() const { return m_no_speech; }
    int no_timestamps() const { return m_no_timestamps; }
    int timestamp_begin() const { return m_timestamp_begin; }
    int timestamp_end() const { return m_timestamp_end; }

private:
    TokenizerType m_tokenizer_type;

    // Special tokens required for transcription. They are declared as ints rather than
    // uint because libtorch does not currently support uint types as inputs to models.
    int m_sot;
    int m_eot;
    int m_transcribe;
    int m_translate;
    int m_no_speech;
    int m_no_timestamps;
    int m_timestamp_begin;
    int m_timestamp_end;

    // Path to the vocabulary file.
    std::string m_vocab_filepath;

    // Length of prompt/begin sequence.
    uint32_t m_prompt_length;

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

} // namespace capgen.