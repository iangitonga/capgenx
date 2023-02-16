#pragma once

#include <chrono>


namespace capgen{

const int exact_div(const long a, const long b);    

class TranscribingTimer {
private:
  int64_t num_segments_;
  std::chrono::time_point<std::chrono::high_resolution_clock> start_time_;

public:
  TranscribingTimer(int64_t num_segments);
  void stop() const;
};

}

