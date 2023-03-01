#pragma once

#include <chrono>


namespace capgen{

const int exact_div(const long a, const long b);    

class TranscribingTimer {
private:
  std::chrono::time_point<std::chrono::high_resolution_clock> start_time_;

public:
  TranscribingTimer();
  void stop(int64_t num_segments) const;
};

}

