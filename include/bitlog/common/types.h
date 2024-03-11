#pragma once

#include <cstdint>

namespace bitlog
{
enum MemoryPageSize : uint32_t
{
  RegularPage = 0,
  HugePage2MB = 2 * 1024 * 1024,
  HugePage1GB = 1024 * 1024 * 1024
};

enum class QueueTypeOption : uint8_t
{
  Default,
  X86Optimised
};

/**
 * Enum to select a timezone
 */
enum class Timezone : uint8_t
{
  LocalTime,
  GmtTime
};

[[nodiscard]] inline std::string get_timezone_string(Timezone timezone) noexcept
{
  switch (timezone)
  {
  case Timezone::LocalTime:
    return "LocalTime";
  case Timezone::GmtTime:
    return "GmtTime";
  default:
    return "Unknown";
  }
}

[[nodiscard]] inline Timezone get_timezone_enum(std::string timezone_str)
{
  static std::unordered_map<std::string, Timezone> const timezone_map = {
    {"localtime", Timezone::LocalTime}, {"gmttime", Timezone::GmtTime}};

  std::transform(timezone_str.begin(), timezone_str.end(), timezone_str.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

  auto const search = timezone_map.find(timezone_str);

  if (search == timezone_map.cend()) [[unlikely]]
  {
    // TODO:: Handle Error ?
    std::abort();
  }

  return search->second;
}

/**
 * @brief Enumerates different options for suffixes in output file names.
 */
enum class FileSuffix : uint8_t
{
  StartDate,
  StartDateTime,
  None
};

[[nodiscard]] inline std::string get_file_suffix_string(FileSuffix suffix) noexcept
{
  switch (suffix)
  {
  case FileSuffix::StartDate:
    return "StartDate";
  case FileSuffix::StartDateTime:
    return "StartDateTime";
  case FileSuffix::None:
    return "None";
  default:
    return "Unknown";
  }
}

[[nodiscard]] inline FileSuffix get_file_suffix_enum(std::string file_suffix_str)
{
  static std::unordered_map<std::string, FileSuffix> const file_suffix_map = {
    {"startdate", FileSuffix::StartDate}, {"startdatetime", FileSuffix::StartDateTime}, {"none", FileSuffix::None}};

  std::transform(std::begin(file_suffix_str), std::end(file_suffix_str), std::begin(file_suffix_str),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

  auto const search = file_suffix_map.find(file_suffix_str);

  if (search == std::cend(file_suffix_map)) [[unlikely]]
  {
    // TODO:: Handle Error ?
    std::abort();
  }

  return search->second;
}

enum class FileOpenMode : uint8_t
{
  Append,
  Write
};

[[nodiscard]] inline std::string get_file_open_mode_string(FileOpenMode mode) noexcept
{
  switch (mode)
  {
  case FileOpenMode::Append:
    return "Append";
  case FileOpenMode::Write:
    return "Write";
  default:
    return "Unknown";
  }
}

[[nodiscard]] inline FileOpenMode get_file_open_mode_enum(std::string file_open_mode_str)
{
  static std::unordered_map<std::string, FileOpenMode> const file_open_mode_map = {
    {"append", FileOpenMode::Append}, {"write", FileOpenMode::Write}};

  std::transform(std::begin(file_open_mode_str), std::end(file_open_mode_str), std::begin(file_open_mode_str),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

  auto const search = file_open_mode_map.find(file_open_mode_str);

  if (search == std::cend(file_open_mode_map)) [[unlikely]]
  {
    // TODO:: Handle Error ?
    std::abort();
  }

  return search->second;
}

enum class FileRotationFrequency : uint8_t
{
  Disabled,
  Daily,
  Hourly,
  Minutely
};

[[nodiscard]] inline std::string get_file_rotation_frequency_string(FileRotationFrequency frequency) noexcept
{
  switch (frequency)
  {
  case FileRotationFrequency::Disabled:
    return "Disabled";
  case FileRotationFrequency::Daily:
    return "Daily";
  case FileRotationFrequency::Hourly:
    return "Hourly";
  case FileRotationFrequency::Minutely:
    return "Minutely";
  default:
    return "Unknown";
  }
}

[[nodiscard]] inline FileRotationFrequency get_file_rotation_frequency_enum(std::string file_rotation_freq_str)
{
  static std::unordered_map<std::string, FileRotationFrequency> const file_rotation_freq_map = {
    {"disabled", FileRotationFrequency::Disabled},
    {"daily", FileRotationFrequency::Daily},
    {"hourly", FileRotationFrequency::Hourly},
    {"minutely", FileRotationFrequency::Minutely}};

  std::transform(std::begin(file_rotation_freq_str), std::end(file_rotation_freq_str),
                 std::begin(file_rotation_freq_str),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

  auto const search = file_rotation_freq_map.find(file_rotation_freq_str);

  if (search == std::cend(file_rotation_freq_map)) [[unlikely]]
  {
    // TODO:: Handle Error ?
    std::abort();
  }

  return search->second;
}

enum class SinkType : uint8_t
{
  File,
  Console
};

[[nodiscard]] inline std::string get_sink_type_string(SinkType sink_type) noexcept
{
  switch (sink_type)
  {
  case SinkType::File:
    return "File";
  case SinkType::Console:
    return "Console";
  default:
    return "Unknown";
  }
}

[[nodiscard]] inline SinkType get_sink_type_enum(std::string sink_type_str)
{
  static std::unordered_map<std::string, SinkType> const sink_type_map = {
    {"file", SinkType::File}, {"console", SinkType::Console}};

  std::transform(std::begin(sink_type_str), std::end(sink_type_str), std::begin(sink_type_str),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

  auto const search = sink_type_map.find(sink_type_str);

  if (search == std::cend(sink_type_map)) [[unlikely]]
  {
    // TODO:: Handle Error ?
    std::abort();
  }

  return search->second;
}

/**
 * Represents log levels
 */
enum class LogLevel : uint8_t
{
  TraceL3,
  TraceL2,
  TraceL1,
  Debug,
  Info,
  Warning,
  Error,
  Critical,
  None,
};

namespace detail
{
/**
 * Enum defining different type descriptors
 */
enum class TypeDescriptorName : uint8_t
{
  None = 0,
  Char,
  SignedChar,
  UnsignedChar,
  ShortInt,
  UnsignedShortInt,
  Int,
  UnsignedInt,
  LongInt,
  UnsignedLongInt,
  LongLongInt,
  UnsignedLongLongInt,
  Float,
  Double,
  CString,
  CStringArray,
  StdString
};
} // namespace detail
} // namespace bitlog