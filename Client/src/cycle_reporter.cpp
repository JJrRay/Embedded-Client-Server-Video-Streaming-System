/// @file cycle_reporter.cpp
#include "cycle_reporter.h"

#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace Metrics::Interne {

std::string nowString() {
  const auto now =
      std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  std::tm local{};
  localtime_r(&now, &local);
  std::ostringstream stream;
  stream << std::put_time(&local, "%Y-%m-%d %H:%M:%S");
  return stream.str();
}

} // namespace Metrics::Interne

namespace Metrics {

void CycleReporter::beginCycle() { _cycleStart = Clock::now(); }

void CycleReporter::reset() {
  _lastReport = Clock::now();
  _cyclesSinceReport = 0;
  _accumulatedMs = 0.0;
}

void CycleReporter::endCycle() {
  const auto now = Clock::now();
  _accumulatedMs +=
      std::chrono::duration<double, std::milli>(now - _cycleStart).count();
  ++_cyclesSinceReport;

  const double sinceReportMs =
      std::chrono::duration<double, std::milli>(now - _lastReport).count();
  if (sinceReportMs < 1000.0) {
    return;
  }

  const double cyclesPerSecond = _cyclesSinceReport * 1000.0 / sinceReportMs;
  const double averageCycleMs = _accumulatedMs / _cyclesSinceReport;
  std::cout << Interne::nowString() << " | cycles/s: " << std::fixed
            << std::setprecision(1) << cyclesPerSecond
            << " | cycle moyen: " << averageCycleMs << " ms" << std::endl;

  reset();
}

} // namespace Metrics