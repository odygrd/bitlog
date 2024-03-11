/**
 * Copyright(c) 2024-present, Odysseas Georgoudis & Bitlog contributors.
 * Distributed under the MIT License (http://opensource.org/licenses/MIT)
 */

#pragma once

// Include always common first as it defines FMTBITLOG_HEADER_ONLY
#include "bitlog/backend/backend_types.h"

#include <chrono>
#include <ctime>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

#include "bitlog/backend/backend_utilities.h"
#include "bitlog/bundled/fmt/format.h"

namespace bitlog::detail
{
class SinkBase
{
public:
  SinkBase() = default;

  /**
   * Logs a formatted log message to the handler
   * @note: Accessor for backend processing
   * @param formatted_log_message input log message to write
   * @param log_event transit event
   */
  virtual void write(std::string_view formatted_log_message, LogStatementMetadata const& lsm) = 0;

  /**
   * Flush the handler synchronising the associated handler with its controlled output sequence.
   */
  virtual void flush() noexcept = 0;

  virtual ~SinkBase() = default;

  [[nodiscard]] SinkType type() const noexcept { return _type; }
  [[nodiscard]] std::string const& name() const noexcept { return _name; }

protected:
  std::string _name;
  SinkType _type;
};

class ConsoleSink : public SinkBase
{
public:
  ConsoleSink() { _type = SinkType::Console; }

  void write(std::string_view formatted_log_message, LogStatementMetadata const&) override
  {
    std::error_code ec;
    if (!detail::fwrite_fully(formatted_log_message.data(), formatted_log_message.size(), _file, ec))
    {
      // TODO:: Handle Error
    }
  }

  void flush() noexcept override { fflush(_file); }

private:
  FILE* _file{stdout};
};

class FileSink : public SinkBase
{
public:
  FileSink() { _type = SinkType::File; }

  [[nodiscard]] bool init(std::filesystem::path const& output_file_path, FileOpenMode open_mode)
  {
    // first attempt to create any non-existing directories
    std::error_code ec;

    _name = _get_output_file_full_path(output_file_path, ec).string();

    if (open_mode == FileOpenMode::Write)
    {
      _file = std::fopen(name().c_str(), "w");
    }
    else if (open_mode == FileOpenMode::Append)
    {
      _file = std::fopen(name().c_str(), "a");
    }

    return _file;
  }

  void write(std::string_view formatted_log_message, LogStatementMetadata const&) override
  {
    std::error_code ec;
    if (!detail::fwrite_fully(formatted_log_message.data(), formatted_log_message.size(), _file, ec))
    {
      // TODO:: Handle Error
    }
  }

  void flush() noexcept override { fflush(_file); }

  ~FileSink() override
  {
    if (_file)
    {
      std::fclose(_file);
    }
  }

private:
  [[nodiscard]] std::filesystem::path _get_output_file_full_path(std::filesystem::path const& output_file_path,
                                                                 std::error_code& ec)
  {
    // First attempt to create any non-existing directories
    std::filesystem::path parent_path;

    if (!output_file_path.parent_path().empty())
    {
      parent_path = output_file_path.parent_path();
      std::filesystem::create_directories(parent_path, ec);
    }
    else
    {
      parent_path = std::filesystem::current_path(ec);
    }

    if (ec) [[unlikely]]
    {
      // TODO: Handle Error
      return std::filesystem::path{};
    }

    // Convert the parent path to an absolute path
    std::filesystem::path const canonical_path = std::filesystem::canonical(parent_path, ec);

    if (ec) [[unlikely]]
    {
      // TODO: Handle Error
      return std::filesystem::path{};
    }

    // Finally, replace the given filename's parent_path with the equivalent canonical path
    return canonical_path / output_file_path.filename();
  }

  /**
   * @brief Get a formatted date and time string based on a timestamp and timezone.
   *
   * Converts the given timestamp in nanoseconds to a formatted date and time string.
   *
   * @param timestamp_ns The timestamp in nanoseconds.
   * @param timezone The timezone to use (GmtTime or Local).
   * @param include_time Flag indicating whether to include the time part in the string.
   * @return Formatted date and time string.
   */
  [[nodiscard]] std::string _get_datetime_string(uint64_t timestamp_ns, Timezone timezone, bool include_time)
  {
    // convert to seconds
    auto const seconds_since_epoch = static_cast<time_t>(timestamp_ns / 1000000000);
    tm time_info;

    if (timezone == Timezone::GmtTime)
    {
      gmtime_rs(&seconds_since_epoch, &time_info);
    }
    else
    {
      localtime_rs(&seconds_since_epoch, &time_info);
    }

    // Construct the string
    std::stringstream ss;
    if (include_time)
    {
      ss << std::setfill('0') << std::setw(4) << time_info.tm_year + 1900 << std::setw(2)
         << time_info.tm_mon + 1 << std::setw(2) << time_info.tm_mday << "_" << std::setw(2)
         << time_info.tm_hour << std::setw(2) << time_info.tm_min << std::setw(2) << time_info.tm_sec;
    }
    else
    {
      ss << std::setfill('0') << std::setw(4) << time_info.tm_year + 1900 << std::setw(2)
         << time_info.tm_mon + 1 << std::setw(2) << time_info.tm_mday;
    }

    return ss.str();
  }

  /**
   * @brief Extract the stem and extension from a filename.
   *
   * Extracts the stem and extension from the given filename.
   *
   * @param filename The input filename.
   * @return A pair containing the stem and extension strings.
   */
  [[nodiscard]] std::pair<std::string, std::string> _extract_stem_and_extension(std::filesystem::path const& filename) noexcept
  {
    // filename and extension
    return std::make_pair((filename.parent_path() / filename.stem()).string(), filename.extension().string());
  }

  /**
   * @brief Append a formatted date and time to a filename.
   *
   * Appends a formatted date and time string to the given filename, based on the provided current time.
   *
   * @param filename The original filename.
   * @param include_time Flag indicating whether to include the time part in the appended string.
   * @param timezone The timezone to use (GmtTime or Local).
   * @param current_time The current time (optional, defaults to the current system time).
   * @return A new filename with the appended date and time.
   */
  [[nodiscard]] std::filesystem::path _append_datetime_to_filename(std::filesystem::path const& filename,
                                                                   bool include_time, Timezone timezone,
                                                                   std::chrono::system_clock::time_point current_time) noexcept
  {
    // Get base file and extension
    auto const [stem, extension] = _extract_stem_and_extension(filename);

    // Get the time now as tm from user or default to now
    std::chrono::system_clock::time_point const ts_now =
      (current_time == std::chrono::system_clock::time_point{}) ? std::chrono::system_clock::now() : current_time;

    uint64_t const timestamp_ns = static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(ts_now.time_since_epoch()).count());

    // Construct a filename
    return fmtbitlog::format("{}_{}{}", stem,
                             _get_datetime_string(timestamp_ns, timezone, include_time), extension);
  }

private:
  FILE* _file{nullptr};
};
} // namespace bitlog::detail