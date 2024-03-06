#include "bundled/doctest.h"

#include "bitlog/backend/pattern_formatter.h"

using namespace bitlog;
using namespace bitlog::detail;

static constexpr char const* thread_name = "test_thread";
static constexpr char const* process_id = "123";
static constexpr std::chrono::nanoseconds ts{1579815761020123000};
static constexpr char const* thread_id_str = "31341";
static constexpr char const* logger_name = "test_logger";

TEST_SUITE_BEGIN("PatternFormatter");

TEST_CASE("default_pattern_formatter")
{
  LogStatementMetadata const lsm{__FILE__,       std::to_string(__LINE__),
                                 __func__,       "This the {} formatter {}",
                                 LogLevel::Info, std::vector<TypeDescriptorName>{}};

  // Format to a buffer
  PatternFormatter::fmt_buffer_t log_message;
  fmtbitlog::format_to(std::back_inserter(log_message), fmtbitlog::runtime(lsm.message_format()),
                       "pattern", 1234);

  PatternFormatter default_pattern_formatter{};
  std::string_view const formatted_log_statement =
    default_pattern_formatter.format(lsm, ts, thread_id_str, thread_name, process_id, logger_name,
                                     std::string_view{log_message.data(), log_message.size()});

  // Default pattern formatter is using local time to convert the timestamp to timezone, in this test we ignore the timestamp
  std::string const expected =
    "[31341] test_pattern_formatter.cpp:18 LOG_INFO      test_logger  This the pattern formatter "
    "1234\n";

  auto const found_expected = formatted_log_statement.find(expected);
  REQUIRE_NE(found_expected, std::string::npos);
}

TEST_CASE("custom_pattern_message_only")
{
  LogStatementMetadata const lsm{__FILE__,        std::to_string(__LINE__),
                                 __func__,        "This the {} formatter {}",
                                 LogLevel::Debug, std::vector<TypeDescriptorName>{}};

  // Format to a buffer
  PatternFormatter::fmt_buffer_t log_message;
  fmtbitlog::format_to(std::back_inserter(log_message), fmtbitlog::runtime(lsm.message_format()),
                       "pattern", 12.34);

  PatternFormatter custom_pattern_formatter{"%(log_level_id) %(log_message)", "%H:%M:%S.%Qns", Timezone::GmtTime};
  std::string_view const formatted_log_statement =
    custom_pattern_formatter.format(lsm, ts, thread_id_str, thread_name, process_id, logger_name,
                                    std::string_view{log_message.data(), log_message.size()});

  std::string const expected_string = "D This the pattern formatter 12.34\n";

  REQUIRE_EQ(formatted_log_statement, expected_string);
}

TEST_CASE("custom_pattern_timestamp_precision_nanoseconds")
{
  LogStatementMetadata const lsm{__FILE__,        std::to_string(__LINE__),
                                 __func__,        "This the {} formatter {}",
                                 LogLevel::Debug, std::vector<TypeDescriptorName>{}};

  // Format to a buffer
  PatternFormatter::fmt_buffer_t log_message;
  fmtbitlog::format_to(std::back_inserter(log_message), fmtbitlog::runtime(lsm.message_format()),
                       "pattern", 1234);

  // Custom pattern with part 1 and part 3
  PatternFormatter custom_pattern_formatter{
    "%(creation_time) [%(thread_id)] %(source_file):%(source_line) LOG_%(log_level) %(logger) "
    "%(log_message) [%(caller_function)]",
    "%m-%d-%Y %H:%M:%S.%Qns", Timezone::GmtTime};
  std::string_view const formatted_log_statement =
    custom_pattern_formatter.format(lsm, ts, thread_id_str, thread_name, process_id, logger_name,
                                    std::string_view{log_message.data(), log_message.size()});

  std::string const expected_string =
    "01-23-2020 21:42:41.020123000 [31341] test_pattern_formatter.cpp:64 LOG_DEBUG test_logger "
    "This the pattern formatter 1234 [DOCTEST_ANON_FUNC_7]\n";

  REQUIRE_EQ(formatted_log_statement, expected_string);
}

TEST_CASE("custom_pattern_timestamp_precision_microseconds")
{
  LogStatementMetadata const lsm{__FILE__,       std::to_string(__LINE__),
                                 __func__,       "This the {} formatter {}",
                                 LogLevel::Info, std::vector<TypeDescriptorName>{}};

  // Format to a buffer
  PatternFormatter::fmt_buffer_t log_message;
  fmtbitlog::format_to(std::back_inserter(log_message), fmtbitlog::runtime(lsm.message_format()),
                       "pattern", 1234);

  PatternFormatter custom_pattern_formatter{
    "%(creation_time) [%(thread_id)] %(source_file):%(source_line) LOG_%(log_level) %(logger) "
    "%(log_message) [%(caller_function)]",
    "%m-%d-%Y %H:%M:%S.%Qus", Timezone::GmtTime};
  std::string_view const formatted_log_statement =
    custom_pattern_formatter.format(lsm, ts, thread_id_str, thread_name, process_id, logger_name,
                                    std::string_view{log_message.data(), log_message.size()});

  std::string const expected_string =
    "01-23-2020 21:42:41.020123 [31341] test_pattern_formatter.cpp:91 LOG_INFO test_logger "
    "This the pattern formatter 1234 [DOCTEST_ANON_FUNC_9]\n";

  REQUIRE_EQ(formatted_log_statement, expected_string);
}

TEST_CASE("custom_pattern_timestamp_precision_milliseconds")
{
  LogStatementMetadata const lsm{__FILE__,       std::to_string(__LINE__),
                                 __func__,       "This the {} formatter {}",
                                 LogLevel::Info, std::vector<TypeDescriptorName>{}};

  // Format to a buffer
  PatternFormatter::fmt_buffer_t log_message;
  fmtbitlog::format_to(std::back_inserter(log_message), fmtbitlog::runtime(lsm.message_format()),
                       "pattern", 1234);

  PatternFormatter custom_pattern_formatter{
    "%(creation_time) [%(thread_id)] %(source_file):%(source_line) LOG_%(log_level) %(logger) "
    "%(log_message) [%(caller_function)]",
    "%m-%d-%Y %H:%M:%S.%Qms", Timezone::GmtTime};
  std::string_view const formatted_log_statement =
    custom_pattern_formatter.format(lsm, ts, thread_id_str, thread_name, process_id, logger_name,
                                    std::string_view{log_message.data(), log_message.size()});

  std::string const expected_string =
    "01-23-2020 21:42:41.020 [31341] test_pattern_formatter.cpp:117 LOG_INFO test_logger "
    "This the pattern formatter 1234 [DOCTEST_ANON_FUNC_11]\n";

  REQUIRE_EQ(formatted_log_statement, expected_string);
}

TEST_CASE("custom_pattern_timestamp_precision_none")
{
  LogStatementMetadata const lsm{__FILE__,       std::to_string(__LINE__),
                                 __func__,       "This the {} formatter {}",
                                 LogLevel::Info, std::vector<TypeDescriptorName>{}};

  // Format to a buffer
  PatternFormatter::fmt_buffer_t log_message;
  fmtbitlog::format_to(std::back_inserter(log_message), fmtbitlog::runtime(lsm.message_format()),
                       "pattern", 1234);

  PatternFormatter custom_pattern_formatter{
    "%(creation_time) [%(thread_id)] %(source_file):%(source_line) LOG_%(log_level) %(logger) "
    "%(log_message) [%(caller_function)]",
    "%m-%d-%Y %H:%M:%S", Timezone::GmtTime};
  std::string_view const formatted_log_statement =
    custom_pattern_formatter.format(lsm, ts, thread_id_str, thread_name, process_id, logger_name,
                                    std::string_view{log_message.data(), log_message.size()});

  std::string const expected_string =
    "01-23-2020 21:42:41 [31341] test_pattern_formatter.cpp:143 LOG_INFO test_logger "
    "This the pattern formatter 1234 [DOCTEST_ANON_FUNC_13]\n";

  REQUIRE_EQ(formatted_log_statement, expected_string);
}

TEST_CASE("custom_pattern_timestamp_strftime_reallocation_on_format_string_2")
{
  LogStatementMetadata const lsm{__FILE__,        std::to_string(__LINE__),
                                 __func__,        "This the {} formatter {}",
                                 LogLevel::Debug, std::vector<TypeDescriptorName>{}};

  // set a timestamp_format that will cause timestamp _formatted_date to re-allocate.
  PatternFormatter custom_pattern_formatter{
    "%(creation_time) [%(thread_id)] %(source_file):%(source_line) LOG_%(log_level) %(logger) "
    "%(log_message) [%(caller_function)]",
    "%FT%T.%Qus%FT%T", Timezone::GmtTime};

  for (size_t i = 0; i < 5; ++i)
  {
    // Format to a buffer
    PatternFormatter::fmt_buffer_t log_message;
    fmtbitlog::format_to(std::back_inserter(log_message), fmtbitlog::runtime(lsm.message_format()),
                         "pattern", 1234);

    std::string_view const formatted_log_statement =
      custom_pattern_formatter.format(lsm, ts, thread_id_str, thread_name, process_id, logger_name,
                                      std::string_view{log_message.data(), log_message.size()});

    std::string const expected_string =
      "2020-01-23T21:42:41.0201232020-01-23T21:42:41 [31341] test_pattern_formatter.cpp:169 "
      "LOG_DEBUG test_logger This the pattern formatter 1234 [DOCTEST_ANON_FUNC_15]\n";

    REQUIRE_EQ(formatted_log_statement, expected_string);
  }
}

TEST_CASE("custom_pattern_timestamp_strftime_reallocation_when_adding_fractional_seconds")
{
  LogStatementMetadata const lsm{__FILE__,        std::to_string(__LINE__),
                                 __func__,        "This the {} formatter {}",
                                 LogLevel::Debug, std::vector<TypeDescriptorName>{}};

  // set a timestamp_format that will cause timestamp _formatted_date to re-allocate.
  PatternFormatter custom_pattern_formatter{
    "%(creation_time) [%(thread_id)] %(source_file):%(source_line) LOG_%(log_level) %(logger) "
    "%(log_message) [%(caller_function)]",
    "%FT%T.%T.%Qus%FT%T", Timezone::GmtTime};

  for (size_t i = 0; i < 5; ++i)
  {
    // Format to a buffer
    PatternFormatter::fmt_buffer_t log_message;
    fmtbitlog::format_to(std::back_inserter(log_message), fmtbitlog::runtime(lsm.message_format()),
                         "pattern", 1234);

    std::string_view const formatted_log_statement =
      custom_pattern_formatter.format(lsm, ts, thread_id_str, thread_name, process_id, logger_name,
                                      std::string_view{log_message.data(), log_message.size()});

    std::string const expected_string =
      "2020-01-23T21:42:41.21:42:41.0201232020-01-23T21:42:41 [31341] "
      "test_pattern_formatter.cpp:200 LOG_DEBUG test_logger This the pattern formatter 1234 "
      "[DOCTEST_ANON_FUNC_17]\n";

    REQUIRE_EQ(formatted_log_statement, expected_string);
  }
}

TEST_CASE("custom_pattern")
{
  LogStatementMetadata const lsm{__FILE__,        std::to_string(__LINE__),
                                 __func__,        "This the {} formatter {}",
                                 LogLevel::Debug, std::vector<TypeDescriptorName>{}};

  // Format to a buffer
  PatternFormatter::fmt_buffer_t log_message;
  fmtbitlog::format_to(std::back_inserter(log_message), fmtbitlog::runtime(lsm.message_format()),
                       "pattern", 1234);

  // Custom pattern with part 1 and part 2
  PatternFormatter custom_pattern_formatter{
    "%(creation_time) [%(thread_id)] %(source_file):%(source_line) LOG_%(log_level) %(logger) "
    "%(log_message)",
    "%m-%d-%Y %H:%M:%S.%Qns", Timezone::GmtTime};
  std::string_view const formatted_log_statement =
    custom_pattern_formatter.format(lsm, ts, thread_id_str, thread_name, process_id, logger_name,
                                    std::string_view{log_message.data(), log_message.size()});

  std::string const expected_string =
    "01-23-2020 21:42:41.020123000 [31341] test_pattern_formatter.cpp:232 LOG_DEBUG test_logger "
    "This the pattern formatter 1234\n";

  REQUIRE_EQ(formatted_log_statement, expected_string);
}

TEST_CASE("custom_pattern_part_3_no_format_specifiers")
{
  LogStatementMetadata const lsm{__FILE__,        std::to_string(__LINE__),
                                 __func__,        "This the {} formatter {}",
                                 LogLevel::Debug, std::vector<TypeDescriptorName>{}};

  // Format to a buffer
  PatternFormatter::fmt_buffer_t log_message;
  fmtbitlog::format_to(std::back_inserter(log_message), fmtbitlog::runtime(lsm.message_format()),
                       "pattern", 1234);

  // Custom pattern with a part 3 that has no format specifiers:
  //   Part 1 - "|{}|{}|"
  //   Part 3 - "|EOM|"
  PatternFormatter custom_pattern_formatter{"|LOG_%(log_level)|%(logger)|%(log_message)|EOM|",
                                            "%H:%M:%S", Timezone::GmtTime};
  std::string_view const formatted_log_statement =
    custom_pattern_formatter.format(lsm, ts, thread_id_str, thread_name, process_id, logger_name,
                                    std::string_view{log_message.data(), log_message.size()});

  std::string const expected_string =
    "|LOG_DEBUG|test_logger|This the pattern formatter 1234|EOM|\n";

  REQUIRE_EQ(formatted_log_statement, expected_string);
}

TEST_CASE("invalid_pattern")
{
  // TODO:: ADD test with invalid pattern
}
TEST_SUITE_END();
