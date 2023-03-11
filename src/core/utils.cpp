#include "utils.h"
#include "log.h"

#include <chrono>
#include <cmath>
#include <cstdlib>



namespace capgen {

const int exact_div(const long a, const long b)
{
    std::ldiv_t result = std::ldiv(a, b);
    return result.quot;
}

TranscribingTimer::TranscribingTimer()
{
    m_start_time = std::chrono::high_resolution_clock::now();
}

void TranscribingTimer::stop(int64_t num_segments) const
{
    auto end_time = std::chrono::high_resolution_clock::now();
    auto start = std::chrono::time_point_cast<std::chrono::seconds>(m_start_time).time_since_epoch().count();
    auto end = std::chrono::time_point_cast<std::chrono::seconds>(end_time).time_since_epoch().count();
    auto duration = end - start;
    auto duration_per_segment = duration / num_segments;
    int64_t duration_min = duration / 60;
    CG_LOG_MINFO("TIMING INFO");
    CG_LOG_MINFO("---------------------------");
    CG_LOG_INFO("Number of segments: %d", num_segments);
    CG_LOG_INFO("Audio Length      : %d mins", (int)(num_segments * 0.5));
    CG_LOG_INFO("Total time elapsed: %d mins", duration_min);
    CG_LOG_INFO("Avg time/segment  : %d secs", duration_per_segment);
    CG_LOG_MINFO("---------------------------");
}

}
