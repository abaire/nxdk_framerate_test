#ifndef NXDK_FRAMERATE_TEST_PROFILER_H
#define NXDK_FRAMERATE_TEST_PROFILER_H

#include <windows.h>

class Profiler {
 public:
  Profiler() { QueryPerformanceFrequency(&perf_freq_); }

  void Start() { QueryPerformanceCounter(&start_counter_); }

  [[nodiscard]] int64_t DeltaTicks() const {
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);

    return now.QuadPart - start_counter_.QuadPart;
  }

  [[nodiscard]] float DeltaMilliseconds() const {
    return static_cast<float>(DeltaTicks()) * 1000.f / static_cast<float>(Frequency());
  }

  template <typename T>
  [[nodiscard]] float DeltaItemsPerSecond(T items) const {
    return static_cast<float>(items) * static_cast<float>(Frequency()) / static_cast<float>(DeltaTicks());
  }

  [[nodiscard]] int64_t Frequency() const { return perf_freq_.QuadPart; }

 private:
  LARGE_INTEGER perf_freq_{};
  LARGE_INTEGER start_counter_{};
};

#endif  // NXDK_FRAMERATE_TEST_PROFILER_H
