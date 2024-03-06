/**
 * Copyright(c) 2024-present, Odysseas Georgoudis & Bitlog contributors.
 * Distributed under the MIT License (http://opensource.org/licenses/MIT)
 */

#pragma once

#include <ctime>

namespace bitlog::detail
{

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