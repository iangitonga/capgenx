#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include "utils.h"
#include "log.h"


namespace capgen {

const int exact_div(const long a, const long b) {
  std::ldiv_t result = std::ldiv(a, b);
  return result.quot;
}

TranscribingTimer::TranscribingTimer() {
  start_time_ = std::chrono::high_resolution_clock::now();
}

void TranscribingTimer::stop(int64_t num_segments) const {
  auto end_time = std::chrono::high_resolution_clock::now();
  auto start = std::chrono::time_point_cast<std::chrono::seconds>(start_time_).time_since_epoch().count();
  auto end = std::chrono::time_point_cast<std::chrono::seconds>(end_time).time_since_epoch().count();
  auto duration = end - start;
  auto duration_per_segment = duration / num_segments;
  int64_t duration_min = duration / 60;
  CG_LOG_MDEBUG("TIMING INFO");
  CG_LOG_MDEBUG("---------------------------");
  CG_LOG_DEBUG("Number of segments: %d", num_segments);
  CG_LOG_DEBUG("Audio Length      : %d mins", (int)(num_segments * 0.5));
  CG_LOG_DEBUG("Total time elapsed: %d mins", duration_min);
  CG_LOG_DEBUG("Avg time/segment  : %d secs", duration_per_segment);
  CG_LOG_MDEBUG("---------------------------");
}
}
