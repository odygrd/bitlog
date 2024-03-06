#include "bundled/doctest.h"

#include "bitlog/backend/cached_timestamp_builder.h"

using namespace bitlog;
using namespace bitlog::detail;

TEST_SUITE_BEGIN("CachedTimestampBuilder");

/***/
TEST_CASE("localtime_format_time")
{
  std::string fmt2 = "%H:%M:%S";
  CachedTimestampBuilder string_from_time;

  string_from_time.init(fmt2, Timezone::LocalTime);

  // Get the timestamp now
  time_t raw_ts;
  std::time(&raw_ts);

  // Try for a few timestamps
  for (uint32_t i = 0; i <= 500'000; ++i)
  {
    // Get the time from string from time
    auto const& time_s1 = string_from_time.format_timestamp(raw_ts);

    // Get the time from strftime
    std::tm time_info{};
    localtime_rs(&raw_ts, &time_info);

    char buffer[256];
    std::strftime(buffer, 256, fmt2.data(), &time_info);
    auto const time_s2 = std::string{buffer};

    REQUIRE_EQ(std::string_view{time_s1.data()}, std::string_view{time_s2.data()});

    // Increment the timestamp for the next loop
    raw_ts += 1;
  }
}

/***/
TEST_CASE("localtime_format_I")
{
  std::string fmt2 = "%I:%M:%S%p";
  CachedTimestampBuilder string_from_time;

  string_from_time.init(fmt2, Timezone::LocalTime);

  // Get the timestamp now
  time_t raw_ts;
  std::time(&raw_ts);

  // Try for a few timestamps
  for (uint32_t i = 0; i <= 500'000; ++i)
  {
    // Get the time from string from time
    auto const& time_s1 = string_from_time.format_timestamp(raw_ts);

    // Get the time from strftime
    std::tm time_info{};
    localtime_rs(&raw_ts, &time_info);
    char buffer[256];
    std::strftime(buffer, 256, fmt2.data(), &time_info);
    auto const time_s2 = std::string{buffer};

    REQUIRE_EQ(std::string_view{time_s1.data()}, std::string_view{time_s2.data()});

    // Increment the timestamp for the next loop
    raw_ts += 1;
  }
}

/***/
TEST_CASE("localtime_fallback_to_strftime")
{
  // In this edge case we pass a timestamp that is back in time from our cached timestamp value.

  // Get the timestamp now
  time_t raw_ts;
  std::time(&raw_ts);

  // This will create the CachedTimestampBuilder class and pre-format a string for the timestamp now
  std::string fmt2 = "%Y-%m-%dT%H:%M:%SZ";
  CachedTimestampBuilder string_from_time;

  string_from_time.init(fmt2, Timezone::GmtTime);

  // Ask CachedTimestampBuilder to format timestamps in the past

  // Try for a few timestamps
  for (uint32_t i = 0; i <= 500'000; ++i)
  {
    // Get the time from string from time
    auto const& time_s1 = string_from_time.format_timestamp(raw_ts);

    // Get the time from strftime
    std::tm time_info{};
    gmtime_rs(&raw_ts, &time_info);
    char buffer[256];
    std::strftime(buffer, 256, fmt2.data(), &time_info);
    auto const time_s2 = std::string{buffer};

    REQUIRE_EQ(std::string_view{time_s1.data()}, std::string_view{time_s2.data()});

    // Decrement the timestamp for the next loop
    raw_ts -= 1;
  }
}

/***/
TEST_CASE("localtime_main_format")
{
  std::string fmt2 = "%Y-%m-%dT%H:%M:%SZ";
  CachedTimestampBuilder string_from_time;

  string_from_time.init(fmt2, Timezone::LocalTime);

  // Get the timestamp now
  time_t raw_ts;
  std::time(&raw_ts);

  // Try for a few timestamps
  for (uint32_t i = 0; i <= 600'000; ++i)
  {
    // Get the time from string from time
    auto const& time_s1 = string_from_time.format_timestamp(raw_ts);

    // Get the time from strftime
    std::tm time_info{};
    localtime_rs(&raw_ts, &time_info);
    char buffer[256];
    std::strftime(buffer, 256, fmt2.data(), &time_info);
    auto const time_s2 = std::string{buffer};

    REQUIRE_EQ(std::string_view{time_s1.data()}, std::string_view{time_s2.data()});

    // Increment the timestamp for the next loop
    raw_ts += 1;
  }
}

/***/
TEST_CASE("gmtime_main_format")
{
  std::string fmt2 = "%Y-%m-%dT%H:%M:%SZ";
  CachedTimestampBuilder string_from_time;

  string_from_time.init(fmt2, Timezone::GmtTime);

  // Get the timestamp now
  time_t raw_ts;
  std::time(&raw_ts);

  // Try for a few timestamps
  for (uint32_t i = 0; i <= 600'000; ++i)
  {
    // Get the time from string from time
    auto const& time_s1 = string_from_time.format_timestamp(raw_ts);

    // Get the time from strftime
    std::tm time_info{};
    gmtime_rs(&raw_ts, &time_info);
    char buffer[256];
    std::strftime(buffer, 256, fmt2.data(), &time_info);
    auto const time_s2 = std::string{buffer};

    REQUIRE_EQ(std::string_view{time_s1.data()}, std::string_view{time_s2.data()});

    // Increment the timestamp for the next loop
    raw_ts += 1;
  }
}

TEST_CASE("localtime_main_format_increment_ts")
{
  std::string fmt2 = "%Y-%m-%dT%H:%M:%SZ";
  CachedTimestampBuilder string_from_time;

  string_from_time.init(fmt2, Timezone::LocalTime);

  // Get the timestamp now
  time_t raw_ts;
  std::time(&raw_ts);

  // Try for a few timestamps
  for (uint32_t i = 0; i <= 10'000; ++i)
  {
    // Get the time from string from time
    auto const& time_s1 = string_from_time.format_timestamp(raw_ts);

    // Get the time from strftime
    std::tm time_info{};
    localtime_rs(&raw_ts, &time_info);
    char buffer[256];
    std::strftime(buffer, 256, fmt2.data(), &time_info);
    auto const time_s2 = std::string{buffer};

    REQUIRE_EQ(std::string_view{time_s1.data()}, std::string_view{time_s2.data()});

    // Increment the timestamp for the next loop
    // This test increments the ts by a huge amount trying to mimic e.g. system clock changes
    raw_ts += 7200;
  }
}

/***/
TEST_CASE("localtime_empty_cached_indexes")
{
  // try with a format that doesn't have hours, minutes, seconds
  std::string fmt2 = "%Y-%m-%d";
  CachedTimestampBuilder string_from_time;

  string_from_time.init(fmt2, Timezone::LocalTime);

  // Get the timestamp now
  time_t raw_ts;
  std::time(&raw_ts);

  // Try for a few timestamps
  for (uint32_t i = 0; i <= 500'000; ++i)
  {
    // Get the time from string from time
    auto const& time_s1 = string_from_time.format_timestamp(raw_ts);

    // Get the time from strftime
    std::tm time_info{};
    localtime_rs(&raw_ts, &time_info);
    char buffer[256];
    std::strftime(buffer, 256, fmt2.data(), &time_info);
    auto const time_s2 = std::string{buffer};

    REQUIRE_EQ(std::string_view{time_s1.data()}, std::string_view{time_s2.data()});

    // Increment the timestamp for the next loop
    raw_ts += 1;
  }
}

#if !defined(_WIN32)
// The following tests don't run on windows because the format identifiers are not supported.

/***/
TEST_CASE("localtime_format_l")
{
  std::string fmt2 = "%l:%M:%S%p";
  CachedTimestampBuilder string_from_time;

  string_from_time.init(fmt2, Timezone::LocalTime);

  // Get the timestamp now
  time_t raw_ts;
  std::time(&raw_ts);

  // Try for a few timestamps
  for (uint32_t i = 0; i <= 500'000; ++i)
  {
    // Get the time from string from time
    auto const& time_s1 = string_from_time.format_timestamp(raw_ts);

    // Get the time from strftime
    std::tm* time_info;
    time_info = std::localtime(&raw_ts);
    char buffer[256];
    std::strftime(buffer, 256, fmt2.data(), time_info);
    auto const time_s2 = std::string{buffer};

    REQUIRE_EQ(std::string_view{time_s1.data()}, std::string_view{time_s2.data()});

    // Increment the timestamp for the next loop
    raw_ts += 1;
  }
}

/***/
TEST_CASE("localtime_format_k")
{
  std::string fmt2 = "%k:%M:%S%p";
  CachedTimestampBuilder string_from_time;

  string_from_time.init(fmt2, Timezone::LocalTime);

  // Get the timestamp now
  time_t raw_ts;
  std::time(&raw_ts);

  // Try for a few timestamps
  for (uint32_t i = 0; i <= 500'000; ++i)
  {
    // Get the time from string from time
    auto const& time_s1 = string_from_time.format_timestamp(raw_ts);

    // Get the time from strftime
    std::tm* time_info;
    time_info = std::localtime(&raw_ts);
    char buffer[256];
    std::strftime(buffer, 256, fmt2.data(), time_info);
    auto const time_s2 = std::string{buffer};

    REQUIRE_EQ(std::string_view{time_s1.data()}, std::string_view{time_s2.data()});

    // Increment the timestamp for the next loop
    raw_ts += 1;
  }
}

/***/
TEST_CASE("localtime_format_s")
{
  std::string fmt2 = "%Y-%m-%d %s";
  CachedTimestampBuilder string_from_time;

  string_from_time.init(fmt2, Timezone::LocalTime);

  // Get the timestamp now
  time_t raw_ts;
  std::time(&raw_ts);

  // Try for a few timestamps
  for (uint32_t i = 0; i <= 100'000; ++i)
  {
    // Get the time from string from time
    auto const& time_s1 = string_from_time.format_timestamp(raw_ts);

    // Get the time from strftime
    std::tm* time_info;
    time_info = std::localtime(&raw_ts);
    char buffer[256];
    std::strftime(buffer, 256, fmt2.data(), time_info);
    auto const time_s2 = std::string{buffer};

    REQUIRE_EQ(std::string_view{time_s1.data()}, std::string_view{time_s2.data()});

    // Increment the timestamp for the next loop
    raw_ts += 1;
  }
}
#endif

TEST_SUITE_END();