#include <cstdio>


#define CG_LOG_RESET_COLOR "\x1B[0m"
#define CG_LOG_INFO_COLOR "\x1B[0m"
#define CG_LOG_DEBUG_COLOR "\x1B[1;34m"
#define CG_LOG_WARNING_COLOR "\x1B[1;33m"
#define CG_LOG_ERROR_COLOR "\x1B[1;31m"


#define CG_LOG_INFO(f, ...) capgen::log("INFO", CG_LOG_INFO_COLOR, f, __VA_ARGS__)
#define CG_LOG_MINFO(...) capgen::log("INFO", CG_LOG_INFO_COLOR, "%s", __VA_ARGS__)
#define CG_LOG_DEBUG(f, ...) capgen::log("DEBUG", CG_LOG_DEBUG_COLOR, f, __VA_ARGS__)
#define CG_LOG_MDEBUG(...) capgen::log("DEBUG", CG_LOG_DEBUG_COLOR, "%s", __VA_ARGS__)
#define CG_LOG_WARNING(f, ...) capgen::log("WARNING", CG_LOG_WARNING_COLOR, f, __VA_ARGS__)
#define CG_LOG_MWARNING(...) capgen::log("WARNING", CG_LOG_WARNING_COLOR, "%s", __VA_ARGS__)
#define CG_LOG_ERROR(f, ...) capgen::log("ERROR", CG_LOG_ERROR_COLOR, f, __VA_ARGS__)
#define CG_LOG_MERROR(...) capgen::log("ERROR", CG_LOG_ERROR_COLOR, "%s", __VA_ARGS__)


namespace capgen {

template<typename ... Args>
void log(const char *type, const char *color, const char *s, Args ... args)
{
    std::printf("%s[%s]: ", color, type);
    std::printf(s, args...);
    std::printf("%s\n", CG_LOG_RESET_COLOR);
}

}