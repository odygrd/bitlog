/**
 * Copyright(c) 2024-present, Odysseas Georgoudis & Bitlog contributors.
 * Distributed under the MIT License (http://opensource.org/licenses/MIT)
 */

#pragma once

// Include always common first as it defines FMTBITLOG_HEADER_ONLY
#include "bitlog/common/common.h"

#include <chrono>
#include <cstddef>
#include <utility>
#include <vector>

#include "bitlog/bundled/fmt/format.h"

namespace bitlog::detail
{
/**
 * @brief Structure to store metadata information for a log statement.
 */
class LogStatementMetadata
{
public:
  LogStatementMetadata(std::string full_source_path, std::string source_line,
                       std::string caller_function, std::string message_format, LogLevel log_level,
                       std::vector<TypeDescriptorName> type_descriptors)
    : _full_source_path(std::move(full_source_path)),
      _source_line(std::move(source_line)),
      _caller_function(std::move(caller_function)),
      _message_format(std::move(message_format)),
      _source_location(_extract_source_location(_full_source_path, _source_line)),
      _source_file(_extract_source_file(_full_source_path)),
      _type_descriptors(std::move(type_descriptors)),
      _log_level(log_level)
  {
  }

  [[nodiscard]] std::string const& full_source_path() const noexcept { return _full_source_path; }

  [[nodiscard]] std::string const& source_line() const noexcept { return _source_line; }

  [[nodiscard]] std::string const& caller_function() const noexcept { return _caller_function; }

  [[nodiscard]] std::string const& message_format() const noexcept { return _message_format; }

  [[nodiscard]] std::string const& source_location() const noexcept { return _source_location; }

  [[nodiscard]] std::string const& source_file() const noexcept { return _source_file; }

  [[nodiscard]] std::vector<TypeDescriptorName> const& type_descriptors() const noexcept
  {
    return _type_descriptors;
  }

  [[nodiscard]] LogLevel log_level() const noexcept { return _log_level; }

  [[nodiscard]] std::string_view log_level_id() const noexcept
  {
    return log_level_to_id_string(_log_level);
  }

private:
  [[nodiscard]] static std::string _extract_source_file(std::string const& pathname) noexcept
  {
    std::filesystem::path path(pathname);
    return path.filename().string();
  }

  [[nodiscard]] static std::string _extract_source_location(std::string const& file_path,
                                                            std::string const& line) noexcept
  {
    return _extract_source_file(file_path) + ":" + std::string(line);
  }

private:
  std::string _full_source_path;
  std::string _source_line;
  std::string _caller_function;
  std::string _message_format;
  std::string _source_location;
  std::string _source_file;
  std::vector<TypeDescriptorName> _type_descriptors;
  LogLevel _log_level;
};

/**
 * @brief Structure to store metadata information for a logger.
 */
class LoggerMetadata
{
public:
  LoggerMetadata(std::string logger_name, std::string log_record_pattern, std::string timestamp_pattern,
                 Timezone timezone, SinkType sink_type, std::string output_file_path,
                 uint64_t rotation_max_file_size, uint64_t rotation_time_interval,
                 std::pair<std::chrono::hours, std::chrono::minutes> rotation_daily_at_time,
                 uint32_t rotation_max_backup_files, FileOpenMode output_file_open_mode,
                 FileRotationFrequency rotation_time_frequency, FileSuffix output_file_suffix,
                 bool rotation_overwrite_oldest_files)
    : _logger_name(std::move(logger_name)),
      _log_record_pattern(std::move(log_record_pattern)),
      _timestamp_pattern(std::move(timestamp_pattern)),
      _output_file_path(std::move(output_file_path)),
      _rotation_daily_at_time(rotation_daily_at_time),
      _rotation_max_file_size(rotation_max_file_size),
      _rotation_time_interval(rotation_time_interval),
      _rotation_max_backup_files(rotation_max_backup_files),
      _timezone(timezone),
      _sink_type(sink_type),
      _output_file_open_mode(output_file_open_mode),
      _rotation_time_frequency(rotation_time_frequency),
      _output_file_suffix(output_file_suffix),
      _rotation_overwrite_oldest_files(rotation_overwrite_oldest_files)
  {
  }

  [[nodiscard]] std::string const& logger_name() const noexcept { return _logger_name; }
  [[nodiscard]] std::string const& log_record_pattern() const noexcept
  {
    return _log_record_pattern;
  }
  [[nodiscard]] std::string const& timestamp_pattern() const noexcept { return _timestamp_pattern; }
  [[nodiscard]] std::string const& output_file_path() const noexcept { return _output_file_path; }

  [[nodiscard]] std::pair<std::chrono::hours, std::chrono::minutes> rotation_daily_at_time() const noexcept
  {
    return _rotation_daily_at_time;
  }
  [[nodiscard]] uint64_t rotation_max_file_size() const noexcept { return _rotation_max_file_size; }
  [[nodiscard]] uint64_t rotation_time_interval() const noexcept { return _rotation_time_interval; }
  [[nodiscard]] uint32_t rotation_max_backup_files() const noexcept
  {
    return _rotation_max_backup_files;
  }

  [[nodiscard]] Timezone timezone() const noexcept { return _timezone; }
  [[nodiscard]] SinkType sink_type() const noexcept { return _sink_type; }
  [[nodiscard]] FileOpenMode output_file_open_mode() const noexcept
  {
    return _output_file_open_mode;
  }
  [[nodiscard]] FileRotationFrequency rotation_time_frequency() const noexcept
  {
    return _rotation_time_frequency;
  }
  [[nodiscard]] FileSuffix output_file_suffix() const noexcept { return _output_file_suffix; }
  [[nodiscard]] bool rotation_overwrite_oldest_files() const noexcept
  {
    return _rotation_overwrite_oldest_files;
  }

private:
  std::string _logger_name;
  std::string _log_record_pattern;
  std::string _timestamp_pattern;
  std::string _output_file_path;

  std::pair<std::chrono::hours, std::chrono::minutes> _rotation_daily_at_time;
  uint64_t _rotation_max_file_size;
  uint64_t _rotation_time_interval;
  uint32_t _rotation_max_backup_files;

  Timezone _timezone;
  SinkType _sink_type;
  FileOpenMode _output_file_open_mode;
  FileRotationFrequency _rotation_time_frequency;
  FileSuffix _output_file_suffix;
  bool _rotation_overwrite_oldest_files;
};
} // namespace bitlog::detail