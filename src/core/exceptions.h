#pragma once

#include <exception>


namespace capgen {

class MediaDecodingException : public std::exception
{
public:
    const char* what() const noexcept override { return "Failed to decode media."; };
};

}
