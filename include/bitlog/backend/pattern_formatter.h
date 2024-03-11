/**
 * Copyright(c) 2024-present, Odysseas Georgoudis & Bitlog contributors.
 * Distributed under the MIT License (http://opensource.org/licenses/MIT)
 */

#pragma once

// Include always common first as it defines FMTBITLOG_HEADER_ONLY
#include "bitlog/backend/backend_types.h"

#include <array>
#include <bitset>
#include <chrono>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "bitlog/backend/timestamp_formatter.h"
#include "bitlog/bundled/fmt/format.h"

namespace bitlog::detail
{
class PatternFormatter
{
  /** Public classes **/
public:
  using fmt_buffer_t = fmtbitlog::basic_memory_buffer<char, 2048>;

  /**
   * Stores the precision of the timestamp
   */
  enum class TimestampPrecision : uint8_t
  {
    None,
    MilliSeconds,
    MicroSeconds,
    NanoSeconds
  };

  enum Attribute : uint32_t
  {
    CreationTime = 0,
    SourceFile,
    CallerFunction,
    LogLevel,
    LogLevelId,
    SourceLine,
    Logger,
    FullSourcePath,
    ThreadId,
    ThreadName,
    ProcessId,
    SourceLocation,
    LogMessage,
    ATTR_NR_ITEMS
  };

  /** Main PatternFormatter class **/
public:
  /**
   * Constructor for a PatterFormatter with a custom format
   * @param format_pattern format_pattern a format string. The same attribute can not be used
   *                       twice in the same format pattern
   *
   *     The following attribute names can be used with the corresponding placeholder in a %-style format string.
   *     %(creation_time)    - Human-readable time when the LogRecord was created
   *     %(full_source_path) - Source file where the logging call was issued
   *     %(source_file)      - Full source file where the logging call was issued
   *     %(caller_function)  - Name of function containing the logging call
   *     %(log_level)        - Text logging level for the log_messageText logging level for the log_message
   *     %(log_level_id)     - Single letter id
   *     %(source_line)      - Source line number where the logging call was issued
   *     %(logger)           - Name of the logger used to log the call.
   *     %(log_message)      - The logged message
   *     %(thread_id)        - Thread ID
   *     %(thread_name)      - Thread Name if set
   *     %(process_id)       - Process ID
   *
   * @param timestamp_format The for format of the date. Same as strftime() format with extra specifiers `%Qms` `%Qus` `Qns`
   * @param timezone The timezone of the timestamp, local_time or gmt_time
   */
  PatternFormatter(std::string format_pattern =
                     "%(creation_time) [%(thread_id)] %(source_location:<28) LOG_%(log_level:<9) "
                     "%(logger:<12) %(log_message)",
                   std::string timestamp_format = "%H:%M:%S.%Qns", Timezone timezone = Timezone::LocalTime)
    : _format_pattern(std::move(format_pattern)), _timestamp_formatter(std::move(timestamp_format), timezone)
  {
    _set_pattern(_format_pattern);
  }

  /**
   * Destructor
   */
  ~PatternFormatter() = default;

  /**
   * @brief Formats a log message based on the specified format and log metadata.
   *
   * This function formats a log message using the provided format string and the metadata
   * from the log statement. It supports various placeholders for attributes like creation time,
   * source file, caller function, log level, thread information, process ID, and the log message itself.
   *
   * @param log_statement_metadata The metadata of the log statement.
   * @param timestamp_ns The timestamp of the log statement in nanoseconds since epoch.
   * @param thread_id The ID of the thread that logged the message.
   * @param thread_name The name of the thread that logged the message.
   * @param process_id The ID of the process that logged the message.
   * @param logger The name of the logger used for the log statement.
   * @param log_message The actual log message to be included in the formatted output.
   * @return A string view containing the formatted log message.
   *
   * @note The function clears any existing buffer before formatting a new log message.
   */
  [[nodiscard]] std::string_view format(LogStatementMetadata const& log_statement_metadata,
                                        uint64_t timestamp_ns, uint32_t thread_id,
                                        thread_name_array_t const& thread_name, std::string const& process_id,
                                        std::string const& logger, std::string_view log_message)
  {
    // clear out existing buffer
    _formatted_log_log_message.clear();

    if (_fmt_format_pattern.empty())
    {
      // nothing to format when the given format is empty. This is useful e.g. in the
      // JsonFileHandler if we want to skip formatting the main log_message
      return std::string_view{};
    }

    if (_is_set_in_pattern[Attribute::CreationTime])
    {
      _set_arg_val<Attribute::CreationTime>(_timestamp_formatter.format_timestamp(timestamp_ns));
    }

    if (_is_set_in_pattern[Attribute::SourceFile])
    {
      _set_arg_val<Attribute::SourceFile>(log_statement_metadata.source_file());
    }

    if (_is_set_in_pattern[Attribute::CallerFunction])
    {
      _set_arg_val<Attribute::CallerFunction>(log_statement_metadata.caller_function());
    }

    if (_is_set_in_pattern[Attribute::LogLevel])
    {
      _set_arg_val<Attribute::LogLevel>(log_level_to_string(log_statement_metadata.log_level()));
    }

    if (_is_set_in_pattern[Attribute::LogLevelId])
    {
      _set_arg_val<Attribute::LogLevelId>(log_statement_metadata.log_level_id());
    }

    if (_is_set_in_pattern[Attribute::SourceLine])
    {
      _set_arg_val<Attribute::SourceLine>(log_statement_metadata.source_line());
    }

    if (_is_set_in_pattern[Attribute::Logger])
    {
      _set_arg_val<Attribute::Logger>(logger);
    }

    if (_is_set_in_pattern[Attribute::FullSourcePath])
    {
      _set_arg_val<Attribute::FullSourcePath>(log_statement_metadata.full_source_path());
    }

    if (_is_set_in_pattern[Attribute::ThreadId])
    {
      _set_arg_val<Attribute::ThreadId>(fmtbitlog::to_string(thread_id));
    }

    if (_is_set_in_pattern[Attribute::ThreadName])
    {
      _set_arg_val<Attribute::ThreadName>(std::string_view{thread_name.data()});
    }

    if (_is_set_in_pattern[Attribute::ProcessId])
    {
      _set_arg_val<Attribute::ProcessId>(process_id);
    }

    if (_is_set_in_pattern[Attribute::SourceLocation])
    {
      _set_arg_val<Attribute::SourceLocation>(log_statement_metadata.source_location());
    }

    _set_arg_val<Attribute::LogMessage>(log_message);

    fmtbitlog::vformat_to(std::back_inserter(_formatted_log_log_message), _fmt_format_pattern,
                          fmtbitlog::basic_format_args(_args.data(), static_cast<int>(_args.size())));

    return std::string_view{_formatted_log_log_message.data(), _formatted_log_log_message.size()};
  }

  [[nodiscard]] std::string const& format_pattern() const noexcept { return _format_pattern; }
  [[nodiscard]] TimestampFormatter const& timestamp_formatter() const noexcept
  {
    return _timestamp_formatter;
  }

private:
  /**
   * Sets a pattern to the formatter.
   * The given pattern is broken down into three parts : part_1 - %(log_message) - part_2
   *
   */
  void _set_pattern(std::string format_pattern)
  {
    format_pattern += "\n";

    // the order we pass the arguments here must match with the order of Attribute enum
    using namespace fmtbitlog::literals;
    std::tie(_fmt_format_pattern, _order_index) = _generate_fmt_format_string(
      _is_set_in_pattern, format_pattern, "creation_time"_a = "", "source_file"_a = "",
      "caller_function"_a = "", "log_level"_a = "", "log_level_id"_a = "", "source_line"_a = "",
      "logger"_a = "", "full_source_path"_a = "", "thread_id"_a = "", "thread_name"_a = "",
      "process_id"_a = "", "source_location"_a = "", "log_message"_a = "");

    _set_arg<Attribute::CreationTime>(std::string_view("creation_time"));
    _set_arg<Attribute::SourceFile>(std::string_view("source_file"));
    _set_arg<Attribute::CallerFunction>(std::string_view("caller_function"));
    _set_arg<Attribute::LogLevel>(std::string_view("log_level"));
    _set_arg<Attribute::LogLevelId>(std::string_view("log_level_id"));
    _set_arg<Attribute::SourceLine>(std::string_view("source_line"));
    _set_arg<Attribute::Logger>(std::string_view("logger"));
    _set_arg<Attribute::FullSourcePath>(std::string_view("full_source_path"));
    _set_arg<Attribute::ThreadId>(std::string_view("thread_id"));
    _set_arg<Attribute::ThreadName>(std::string_view("thread_name"));
    _set_arg<Attribute::ProcessId>(std::string_view("process_id"));
    _set_arg<Attribute::SourceLocation>(std::string_view("source_location"));
    _set_arg<Attribute::LogMessage>(std::string_view("log_message"));
  }

  /***/
  template <size_t I, typename T>
  void _set_arg(T const& arg)
  {
    _args[_order_index[I]] = fmtbitlog::detail::make_arg<fmtbitlog::format_context>(arg);
  }

  template <size_t I, typename T>
  void _set_arg_val(T const& arg)
  {
    fmtbitlog::detail::value<fmtbitlog::format_context>& value_ =
      *(reinterpret_cast<fmtbitlog::detail::value<fmtbitlog::format_context>*>(
        std::addressof(_args[_order_index[I]])));

    value_ = fmtbitlog::detail::arg_mapper<fmtbitlog::format_context>().map(arg);
  }

private:
  /***/
  [[nodiscard]] static PatternFormatter::Attribute _attribute_from_string(std::string const& attribute_name)
  {
    static std::unordered_map<std::string, PatternFormatter::Attribute> const attr_map = {
      {"creation_time", PatternFormatter::Attribute::CreationTime},
      {"source_file", PatternFormatter::Attribute::SourceFile},
      {"caller_function", PatternFormatter::Attribute::CallerFunction},
      {"log_level", PatternFormatter::Attribute::LogLevel},
      {"log_level_id", PatternFormatter::Attribute::LogLevelId},
      {"source_line", PatternFormatter::Attribute::SourceLine},
      {"logger", PatternFormatter::Attribute::Logger},
      {"full_source_path", PatternFormatter::Attribute::FullSourcePath},
      {"thread_id", PatternFormatter::Attribute::ThreadId},
      {"thread_name", PatternFormatter::Attribute::ThreadName},
      {"process_id", PatternFormatter::Attribute::ProcessId},
      {"source_location", PatternFormatter::Attribute::SourceLocation},
      {"log_message", PatternFormatter::Attribute::LogMessage}};

    auto const search = attr_map.find(attribute_name);

    if (search == attr_map.cend()) [[unlikely]]
    {
      // TODO::Handle error
      std::abort();
    }

    return search->second;
  }

  /***/
  template <size_t, size_t>
  static constexpr void _store_named_args(
    std::array<fmtbitlog::detail::named_arg_info<char>, PatternFormatter::Attribute::ATTR_NR_ITEMS>&)
  {
  }

  /***/
  template <size_t Idx, size_t NamedIdx, typename Arg, typename... Args>
  static constexpr void _store_named_args(
    std::array<fmtbitlog::detail::named_arg_info<char>, PatternFormatter::Attribute::ATTR_NR_ITEMS>& named_args_store,
    Arg const& arg, Args const&... args)
  {
    named_args_store[NamedIdx] = {arg.name, Idx};
    _store_named_args<Idx + 1, NamedIdx + 1>(named_args_store, args...);
  }

  /**
   * Convert the pattern to fmt format string and also populate the order index array
   * e.g. given :
   *   "%(creation_time) [%(thread_id)] %(source_file):%(source_line) %(log_level:<12) %(logger) - "
   *
   * is changed to :
   *  {} [{}] {}:{} {:<12} {} -
   *
   *  with a order index of :
   *  i: 0 order idx[i] is: 0 - %(creation_time)
   *  i: 1 order idx[i] is: 2 - %(source_file)
   *  i: 2 order idx[i] is: 10 - empty
   *  i: 3 order idx[i] is: 4 - %(log_level)
   *  i: 4 order idx[i] is: 10 - empty
   *  i: 5 order idx[i] is: 3 - %(source_line)
   *  i: 6 order idx[i] is: 5 - %(logger)
   *  i: 7 order idx[i] is: 10 - empty
   *  i: 8 order idx[i] is: 1 - %(thread_id)
   *  i: 9 order idx[i] is: 10 - empty
   *  i: 10 order idx[i] is: 10 - empty
   * @tparam Args Args
   * @param is_set_in_pattern is set in pattern
   * @param pattern pattern
   * @param args args
   * @return processed pattern
   */
  template <typename... Args>
  [[nodiscard]] static std::pair<std::string, std::array<size_t, PatternFormatter::Attribute::ATTR_NR_ITEMS>> _generate_fmt_format_string(
    std::bitset<PatternFormatter::Attribute::ATTR_NR_ITEMS>& is_set_in_pattern, std::string pattern,
    Args const&... args)
  {
    // Attribute enum and the args we are passing here must be in sync
    static_assert(PatternFormatter::Attribute::ATTR_NR_ITEMS == sizeof...(Args));

    std::array<size_t, PatternFormatter::Attribute::ATTR_NR_ITEMS> order_index{};
    order_index.fill(PatternFormatter::Attribute::ATTR_NR_ITEMS - 1);

    std::array<fmtbitlog::detail::named_arg_info<char>, PatternFormatter::Attribute::ATTR_NR_ITEMS> named_args{};
    _store_named_args<0, 0>(named_args, args...);
    uint8_t arg_idx = 0;

    // we will replace all %(....) with {} to construct a string to pass to fmt library
    std::size_t arg_identifier_pos = pattern.find_first_of('%');
    while (arg_identifier_pos != std::string::npos)
    {
      if (std::size_t const open_paren_pos = pattern.find_first_of('(', arg_identifier_pos);
          open_paren_pos != std::string::npos && (open_paren_pos - arg_identifier_pos) == 1)
      {
        // if we found '%(' we have a matching pattern and we now need to get the closed paren
        std::size_t const closed_paren_pos = pattern.find_first_of(')', open_paren_pos);

        if (closed_paren_pos == std::string::npos)
        {
          // TODO:: Handle error
          std::abort();
        }

        // We have everything, get the substring, this time including '%( )'
        std::string attr = pattern.substr(arg_identifier_pos, (closed_paren_pos + 1) - arg_identifier_pos);

        // find any user format specifiers
        size_t const pos = attr.find(':');
        std::string attr_name;

        if (pos != std::string::npos)
        {
          // we found user format specifiers that we want to keep.
          // e.g. %(source_location:<32)

          // Get only the format specifier
          // e.g. :<32
          std::string custom_format_specifier = attr.substr(pos);
          custom_format_specifier.pop_back(); // remove the ")"

          // replace with the pattern with the correct value
          std::string value;
          value += "{";
          value += custom_format_specifier;
          value += "}";

          // e.g. {:<32}
          pattern.replace(arg_identifier_pos, attr.length(), value);

          // Get the part that is the named argument
          // e.g. source_location
          attr_name = attr.substr(2, pos - 2);
        }
        else
        {
          // Make the replacement.
          pattern.replace(arg_identifier_pos, attr.length(), "{}");

          // Get the part that is the named argument
          // e.g. source_location
          attr.pop_back(); // remove the ")"

          attr_name = attr.substr(2, attr.size());
        }

        // reorder
        int id = -1;

        for (size_t i = 0; i < PatternFormatter::Attribute::ATTR_NR_ITEMS; ++i)
        {
          if (named_args[i].name == attr_name)
          {
            id = named_args[i].id;
            break;
          }
        }

        if (id < 0)
        {
          // TODO:: Handle error
          std::abort();
        }

        order_index[static_cast<size_t>(id)] = arg_idx++;

        // Also set the value as used in the pattern in our bitset for lazy evaluation
        PatternFormatter::Attribute const attr_enum_value = _attribute_from_string(attr_name);
        is_set_in_pattern.set(attr_enum_value);

        // Look for the next pattern to replace
        arg_identifier_pos = pattern.find_first_of('%');
      }
      else
      {
        // search for the next '%'
        arg_identifier_pos = pattern.find_first_of('%', arg_identifier_pos + 1);
      }
    }

    return std::make_pair(pattern, order_index);
  }

private:
  std::string _format_pattern;
  std::string _fmt_format_pattern;

  /** Each named argument in the format_pattern is mapped in order to this array **/
  std::array<size_t, Attribute::ATTR_NR_ITEMS> _order_index{};
  std::array<fmtbitlog::basic_format_arg<fmtbitlog::format_context>, Attribute::ATTR_NR_ITEMS> _args{};
  std::bitset<Attribute::ATTR_NR_ITEMS> _is_set_in_pattern;

  /** class responsible for formatting the timestamp */
  TimestampFormatter _timestamp_formatter{"%H:%M:%S.%Qns", Timezone::LocalTime};

  /** The buffer where we store each formatted string, also stored as class member to avoid
   * re-allocations. This is mutable so we can have a format() const function **/
  fmt_buffer_t _formatted_log_log_message;
};
} // namespace bitlog::detail
