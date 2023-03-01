#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include "utils.h"


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
  std::cout << "[TIMING]\n---------------------------" << std::endl;
  std::cout << "Audio length      : " << (int)(num_segments * 0.5) << " mins" << std::endl;
  std::cout << "Number of segments: " << num_segments << std::endl;
  std::cout << "Total time elapsed: " << duration_min << " mins" << std::endl;
  std::cout << "Avg time/segment  : " << duration_per_segment << " secs" << std::endl;
  std::cout << "---------------------------\n" << std::endl;
}
}
