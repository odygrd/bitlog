/**
 * Copyright(c) 2024-present, Odysseas Georgoudis & Bitlog contributors.
 * Distributed under the MIT License (http://opensource.org/licenses/MIT)
 */

#pragma once

// Include always common first as it defines FMTBITLOG_HEADER_ONLY
#include "bitlog/common/common.h"

#include <cstddef>
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
  LogStatementMetadata(std::string_view full_source_path, std::string_view source_line,
                       std::string_view caller_function, std::string_view message_format,
                       LogLevel log_level, std::vector<TypeDescriptorName> type_descriptors)
    : _full_source_path(full_source_path),
      _source_line(source_line),
      _caller_function(caller_function),
      _message_format(message_format),
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
struct LoggerMetadata
{
  std::string name;
};

/**
 * Enum to select a timezone
 */
enum class Timezone : uint8_t
{
  LocalTime,
  GmtTime
};
} // namespace bitlog::detail