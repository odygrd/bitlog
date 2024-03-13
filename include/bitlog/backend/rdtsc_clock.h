/**
 * Copyright(c) 2024-present, Odysseas Georgoudis & Bitlog contributors.
 * Distributed under the MIT License (http://opensource.org/licenses/MIT)
 */

#pragma once

#include "bitlog/common/common.h"

#include <chrono>
#include <cstdint>

namespace bitlog::detail
{
/**
 * Converts tsc ticks to nanoseconds since epoch
 */
class RdtscClock
{
  /**
   * A static class that calculates the rdtsc ticks per second
   */
  class RdtscTicks
  {
  public:
    [[nodiscard]] static RdtscTicks& instance()
    {
      static RdtscTicks inst;
      return inst;
    }

    /***/
    [[nodiscard]] double ns_per_tick() const noexcept { return _ns_per_tick; }

  private:
    /**
     * Constructor
     */
    RdtscTicks()
    {
      // Convert rdtsc to wall time.
      // 1. Get real time and rdtsc current count
      // 2. Calculate how many rdtsc ticks can occur in one
      // calculate _ticks_per_ns as the median over a number of observations.
      constexpr std::chrono::milliseconds spin_duration = std::chrono::milliseconds{10};

      constexpr int trials = 13;
      std::array<double, trials> rates = {{0}};

      for (size_t i = 0; i < trials; ++i)
      {
        auto const beg_ts =
          std::chrono::nanoseconds{std::chrono::steady_clock::now().time_since_epoch().count()};
        uint64_t const beg_tsc = rdtsc();

        std::chrono::nanoseconds elapsed_ns;
        uint64_t end_tsc;
        do
        {
          auto const end_ts =
            std::chrono::nanoseconds{std::chrono::steady_clock::now().time_since_epoch().count()};
          end_tsc = rdtsc();

          elapsed_ns = end_ts - beg_ts;       // calculates ns between two timespecs
        } while (elapsed_ns < spin_duration); // busy spin for 10ms

        rates[i] = static_cast<double>(end_tsc - beg_tsc) / static_cast<double>(elapsed_ns.count());
      }

      std::nth_element(rates.begin(), rates.begin() + trials / 2, rates.end());

      double const ticks_per_ns = rates[trials / 2];
      _ns_per_tick = 1 / ticks_per_ns;
    }

    double _ns_per_tick{0};
  };

public:
  /**
   * Constructor
   * @param resync_interval the interval to resync the tsc clock with the real system wall clock
   */
  explicit RdtscClock(std::chrono::nanoseconds resync_interval)
    : _resync_interval_ticks(static_cast<std::int64_t>(static_cast<double>(resync_interval.count()) *
                                                       RdtscTicks::instance().ns_per_tick())),
      _resync_interval_original(_resync_interval_ticks),
      _ns_per_tick(RdtscTicks::instance().ns_per_tick())

  {
    if (!resync(2500))
    {
      // try to resync again with higher lag
      if (!resync(10000))
      {
        // TODO:: Handle Error "Failed to sync RdtscClock. Timestamps will be incorrect" << std::endl;
      }
    }
  }

  /**
   * Convert tsc cycles to nanoseconds
   * @param rdtsc_value the rdtsc timestamp to convert
   * @return the rdtsc timestamp converted to nanoseconds since epoch
   * @note should only get called by the backend logging thread
   */
  uint64_t time_since_epoch(uint64_t rdtsc_value) noexcept
  {
    // get rdtsc current value and compare the diff then add it to base wall time
    auto diff = static_cast<int64_t>(rdtsc_value - _base.base_tsc);

    // we need to sync after we calculated otherwise base_tsc value will be ahead of passed tsc value
    if (diff > _resync_interval_ticks)
    {
      resync(2500);
      diff = static_cast<int64_t>(rdtsc_value - _base.base_tsc);
    }

    return static_cast<uint64_t>(_base.base_time + static_cast<int64_t>(static_cast<double>(diff) * _ns_per_tick));
  }

  /**
   * Sync base wall time and base tsc.
   * @see static constexpr std::chrono::minutes resync_timer_{5};
   */
  bool resync(uint32_t lag) noexcept
  {
    // Sometimes we might get an interrupt and might never resync, so we will try again up to max_attempts
    constexpr uint8_t max_attempts{4};

    for (uint8_t attempt = 0; attempt < max_attempts; ++attempt)
    {
      uint64_t const beg = rdtsc();
      // we force convert to nanoseconds because the precision of system_clock::time-point is not portable across platforms.
      int64_t const wall_time =
        std::chrono::nanoseconds{std::chrono::system_clock::now().time_since_epoch()}.count();
      uint64_t const end = rdtsc();

      if (end - beg <= lag) [[likely]]
      {
        _base.base_time = wall_time;
        _base.base_tsc = _fast_average(beg, end);
        _resync_interval_ticks = _resync_interval_original;
        return true;
      }
    }

    // we failed to return earlier and we never resynced, but we don't really want to keep retrying on each call
    // to time_since_epoch() so we do non accurate resync we will increase the resync duration to resync later
    _resync_interval_ticks = _resync_interval_ticks * 2;
    return false;
  }

  /**
   * rdtsc ticks per nanosecond
   * @return the ticks per nanosecond
   */
  double nanoseconds_per_tick() const noexcept { return _ns_per_tick; }

private:
  /**
   * Calculates a fast average of two numbers
   */
  [[nodiscard]] static uint64_t _fast_average(uint64_t x, uint64_t y) noexcept
  {
    return (x & y) + ((x ^ y) >> 1);
  }

  struct BaseTimeTsc
  {
    BaseTimeTsc() = default;
    int64_t base_time{0}; /**< Get the initial base time in nanoseconds from epoch */
    uint64_t base_tsc{0}; /**< Get the initial base tsc time */
  };

private:
  mutable int64_t _resync_interval_ticks{0};
  int64_t _resync_interval_original{0}; /**< stores the initial interval value as as if we fail to resync we increase the timer */
  double _ns_per_tick{0};
  BaseTimeTsc _base;
};
} // namespace bitlog::detail