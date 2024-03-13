/**
 * Copyright(c) 2024-present, Odysseas Georgoudis & Bitlog contributors.
 * Distributed under the MIT License (http://opensource.org/licenses/MIT)
 */

#pragma once

#include <ctime>

#include <sched.h>

namespace bitlog::detail
{

/***/
void set_cpu_affinity(uint16_t cpu_id)
{
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(cpu_id, &cpuset);

  auto const err = sched_setaffinity(0, sizeof(cpuset), &cpuset);

  if (err == -1)
  {
    // TODO:: Handle error
  }
}

/***/
inline tm* gmtime_rs(time_t const* timer, tm* buf) noexcept
{
  tm* res = gmtime_r(timer, buf);
  if (!res) [[unlikely]]
  {
    // TODO:: Handle error
  }
  return res;
}

/***/
inline tm* localtime_rs(time_t const* timer, tm* buf) noexcept
{
  tm* res = localtime_r(timer, buf);
  if (!res) [[unlikely]]
  {
    // TODO:: Handle error
  }
  return res;
}

/***/
inline time_t time_gm(tm* tm)
{
  time_t const ret_val = ::timegm(tm);

  if (ret_val == (time_t)-1) [[unlikely]]
  {
    // TODO:: Handle error
  }

  return ret_val;
}
} // namespace bitlog::detail