#include "bundled/doctest.h"

#include "bitlog/backend/timestamp_formatter.h"

using namespace bitlog;
using namespace bitlog::detail;

TEST_SUITE_BEGIN("TimestampFormatter");

/***/
TEST_CASE("simple_format_string")
{
  // valid simple string
  REQUIRE_NOTHROW(TimestampFormatter ts_formatter{"%I:%M%p%S%Qns z"});
}

/***/
TEST_CASE("format_string_no_additional_specifier")
{
  std::chrono::nanoseconds constexpr timestamp{1587161887987654321};

  // simple formats without any ms/us/ns specifiers
  {
    TimestampFormatter ts_formatter{"%H:%M:%S", Timezone::GmtTime};

    auto const& result = ts_formatter.format_timestamp(timestamp);
    REQUIRE_EQ(std::string_view{result.data()}, "22:18:07");
  }

  {
    TimestampFormatter ts_formatter{"%F %H:%M:%S", Timezone::GmtTime};

    auto const& result = ts_formatter.format_timestamp(timestamp);
    REQUIRE_EQ(std::string_view{result.data()}, "2020-04-17 22:18:07");
  }

  // large simple string to cause reallocation
  {
    TimestampFormatter ts_formatter{"%A %B %d %T %Y %F", Timezone::GmtTime};

    auto const& result = ts_formatter.format_timestamp(timestamp);
    REQUIRE_EQ(std::string_view{result.data()}, "Friday April 17 22:18:07 2020 2020-04-17");
  }
}

/***/
TEST_CASE("format_string_with_millisecond_precision")
{
  // simple
  {
    std::chrono::nanoseconds constexpr timestamp{1587161887987654321};
    TimestampFormatter ts_formatter{"%H:%M:%S.%Qms", Timezone::GmtTime};

    auto const& result = ts_formatter.format_timestamp(timestamp);
    REQUIRE_EQ(std::string_view{result.data()}, "22:18:07.987");
  }

  // with double formatting
  {
    std::chrono::nanoseconds constexpr timestamp{1587161887803654321};
    TimestampFormatter ts_formatter{"%H:%M:%S.%Qms %D", Timezone::GmtTime};

    auto const& result = ts_formatter.format_timestamp(timestamp);
    REQUIRE_EQ(std::string_view{result.data()}, "22:18:07.803 04/17/20");
  }

  // with double formatting 2
  {
    std::chrono::nanoseconds constexpr timestamp{1587161887023654321};
    TimestampFormatter ts_formatter{"%H:%M:%S.%Qms-%G", Timezone::GmtTime};

    auto const& result = ts_formatter.format_timestamp(timestamp);
    REQUIRE_EQ(std::string_view{result.data()}, "22:18:07.023-2020");
  }

  // with zeros
  {
    std::chrono::nanoseconds constexpr timestamp{1587161887009654321};
    TimestampFormatter ts_formatter{"%H:%M:%S.%Qms", Timezone::GmtTime};

    auto const& result = ts_formatter.format_timestamp(timestamp);
    REQUIRE_EQ(std::string_view{result.data()}, "22:18:07.009");
  }
}

/***/
TEST_CASE("format_string_with_microsecond_precision")
{
  // simple
  {
    std::chrono::nanoseconds constexpr timestamp{1587161887987654321};
    TimestampFormatter ts_formatter{"%H:%M:%S.%Qus", Timezone::GmtTime};

    auto const& result = ts_formatter.format_timestamp(timestamp);
    REQUIRE_EQ(std::string_view{result.data()}, "22:18:07.987654");
  }

  // with double formatting
  {
    std::chrono::nanoseconds constexpr timestamp{1587161887803654321};
    TimestampFormatter ts_formatter{"%H:%M:%S.%Qus %D", Timezone::GmtTime};

    auto const& result = ts_formatter.format_timestamp(timestamp);
    REQUIRE_EQ(std::string_view{result.data()}, "22:18:07.803654 04/17/20");
  }

  // with double formatting 2
  {
    std::chrono::nanoseconds constexpr timestamp{1587161887010654321};
    TimestampFormatter ts_formatter{"%H:%M:%S.%Qus-%G", Timezone::GmtTime};

    auto const& result = ts_formatter.format_timestamp(timestamp);
    REQUIRE_EQ(std::string_view{result.data()}, "22:18:07.010654-2020");
  }

  // with zeros
  {
    std::chrono::nanoseconds constexpr timestamp{1587161887000004321};
    TimestampFormatter ts_formatter{"%H:%M:%S.%Qus", Timezone::GmtTime};

    auto const& result = ts_formatter.format_timestamp(timestamp);
    REQUIRE_EQ(std::string_view{result.data()}, "22:18:07.000004");
  }
}

/***/
TEST_CASE("format_string_with_nanosecond_precision")
{
  // simple
  {
    std::chrono::nanoseconds constexpr timestamp{1587161887987654321};
    TimestampFormatter ts_formatter{"%H:%M:%S.%Qns", Timezone::GmtTime};

    auto const& result = ts_formatter.format_timestamp(timestamp);
    REQUIRE_EQ(std::string_view{result.data()}, "22:18:07.987654321");
  }

  // with double formatting
  {
    std::chrono::nanoseconds constexpr timestamp{1587161887803654320};
    TimestampFormatter ts_formatter{"%H:%M:%S.%Qns %D", Timezone::GmtTime};

    auto const& result = ts_formatter.format_timestamp(timestamp);
    REQUIRE_EQ(std::string_view{result.data()}, "22:18:07.803654320 04/17/20");
  }

  // with double formatting 2
  {
    std::chrono::nanoseconds constexpr timestamp{1587161887000654321};
    TimestampFormatter ts_formatter{"%H:%M:%S.%Qns-%G", Timezone::GmtTime};

    auto const& result = ts_formatter.format_timestamp(timestamp);
    REQUIRE_EQ(std::string_view{result.data()}, "22:18:07.000654321-2020");
  }

  // with zeros
  {
    std::chrono::nanoseconds constexpr timestamp{1587161887000000009};
    TimestampFormatter ts_formatter{"%H:%M:%S.%Qns", Timezone::GmtTime};

    auto const& result = ts_formatter.format_timestamp(timestamp);
    REQUIRE_EQ(std::string_view{result.data()}, "22:18:07.000000009");
  }

  // with max
  {
    std::chrono::nanoseconds constexpr timestamp{1587161887999999999};
    TimestampFormatter ts_formatter{"%H:%M:%S.%Qns", Timezone::GmtTime};

    auto const& result = ts_formatter.format_timestamp(timestamp);
    REQUIRE_EQ(std::string_view{result.data()}, "22:18:07.999999999");
  }
}

TEST_SUITE_END();