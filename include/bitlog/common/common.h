#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <mutex>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <utility>

#include <sys/file.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "bitlog/common/types.h"

#define FMTBITLOG_HEADER_ONLY

#if defined(__x86_64__) || defined(_M_X64)
  #define BITLOG_X86_ARCHITECTURE_ENABLED
#endif

// Attributes and Macros
#ifdef NDEBUG // Check if not in debug mode
  #define BITLOG_ALWAYS_INLINE [[gnu::always_inline]]
#else
  #define BITLOG_ALWAYS_INLINE
#endif

namespace bitlog::detail
{
/** Constants **/
static constexpr size_t CACHE_LINE_SIZE_BYTES{64u};
static constexpr size_t CACHE_LINE_ALIGNED{2 * CACHE_LINE_SIZE_BYTES};
static constexpr char const* LOG_STATEMENTS_METADATA_FILENAME{"log-statements-metadata.yaml"};
static constexpr char const* LOGGERS_METADATA_FILENAME{"loggers-metadata.yaml"};
static constexpr char const* APP_RUNNING_FILENAME{"running.app-lock"};
static constexpr char const* APP_READY_FILENAME{"init.app-ready"};

/**
 * @brief Converts LogLevel to its full string representation.
 *
 * @param log_level The log level to convert.
 * @return A string view containing the full log level representation.
 */
[[nodiscard]] inline std::string_view log_level_to_string(LogLevel log_level)
{
  static constexpr std::array<std::string_view, 11> log_level_strings = {
    {"TRACE_L3", "TRACE_L2", "TRACE_L1", "DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL", "NONE"}};

  using log_lvl_t = std::underlying_type_t<LogLevel>;

  auto const log_level_index = static_cast<log_lvl_t>(log_level);
  if (log_level_index > (log_level_strings.size() - 1)) [[unlikely]]
  {
    // TODO:: Handle Error ?
    std::abort();
  }

  return log_level_strings[log_level_index];
}

/**
 * @brief Converts LogLevel to its short identifier string.
 *
 * @param log_level The log level to convert.
 * @return A string view containing the short identifier of the log level.
 */
[[nodiscard]] inline std::string_view log_level_to_id_string(LogLevel log_level)
{
  static constexpr std::array<std::string_view, 11> log_level_id_strings = {
    {"T3", "T2", "T1", "D", "I", "W", "E", "C", "N"}};

  using log_lvl_t = std::underlying_type_t<LogLevel>;

  auto const log_level_index = static_cast<log_lvl_t>(log_level);
  if (log_level_index > (log_level_id_strings.size() - 1)) [[unlikely]]
  {
    // TODO:: Handle Error ?
    std::abort();
  }

  return log_level_id_strings[log_level_index];
}

/**
 * @brief Converts string to LogLevel.
 *
 * @param log_level The string representation of the log level.
 * @return The corresponding LogLevel.
 */
[[nodiscard]] inline LogLevel log_level_from_string(std::string log_level)
{
  static std::unordered_map<std::string, LogLevel> const log_level_map = {
    {"tracel3", LogLevel::TraceL3}, {"trace_l3", LogLevel::TraceL3},
    {"tracel2", LogLevel::TraceL2}, {"trace_l2", LogLevel::TraceL2},
    {"tracel1", LogLevel::TraceL1}, {"trace_l1", LogLevel::TraceL1},
    {"debug", LogLevel::Debug},     {"info", LogLevel::Info},
    {"warning", LogLevel::Warning}, {"error", LogLevel::Error}};

  // parse log level
  std::transform(log_level.begin(), log_level.end(), log_level.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

  auto const search = log_level_map.find(log_level);

  if (search == log_level_map.cend()) [[unlikely]]
  {
    // TODO:: Handle Error ?
    std::abort();
  }

  return search->second;
}

/**
 * Round up a value to the nearest multiple of a specified size.
 * @tparam T Type of value to round.
 * @param value The value to be rounded up.
 * @param round_to The size to which the value should be rounded.
 * @return The rounded-up value.
 */
template <typename T>
[[nodiscard]] T round_up_to_nearest(T value, T round_to) noexcept
{
  return ((value + round_to - 1) / round_to) * round_to;
}

/**
 * @brief Retrieves the ID of the current thread.
 *
 * Retrieves the ID of the current thread using system-specific calls.
 *
 * @return The ID of the current thread.
 */
[[nodiscard]] inline uint32_t thread_id() noexcept
{
  thread_local uint32_t thread_id{0};
  if (!thread_id)
  {
    thread_id = static_cast<uint32_t>(::syscall(SYS_gettid));
  }
  return thread_id;
}

/**
 * @brief Retrieves the system's page size or returns the specified memory page size.
 *
 * Retrieves the system's page size
 *
 * @param memory_page_size The specified memory page size to use
 * @return The size of the system's page or the specified memory page size.
 */
[[nodiscard]] inline size_t page_size() noexcept
{
  thread_local uint32_t page_size{0};
  if (!page_size)
  {
    page_size = static_cast<uint32_t>(sysconf(_SC_PAGESIZE));
  }
  return page_size;
}

/**
 * Literal class type that wraps a constant expression string.
 */
template <size_t N>
struct StringLiteral
{
  constexpr StringLiteral(char const (&str)[N]) { std::copy_n(str, N, value); }
  char value[N];
};

/**
 * @brief Attempts to lock a file descriptor in an exclusive, non-blocking manner.
 *
 * @param fd The file descriptor to lock.
 * @param ec Reference to a std::error_code object that will be set in case of failure.
 * @return True if the file was successfully locked, false otherwise.
 */
inline bool lock_file(int fd, std::error_code& ec) noexcept
{
  if (::flock(fd, LOCK_EX | LOCK_NB) == -1)
  {
    ec = std::error_code{errno, std::generic_category()};
    return false;
  }
  return true;
}

/**
 * @brief Attempts to unlock a file descriptor in an exclusive, non-blocking manner.
 *
 * @param fd The file descriptor unlock lock.
 * @param ec Reference to a std::error_code object that will be set in case of failure.
 * @return True if the file was successfully unlocked, false otherwise.
 */
inline bool unlock_file(int fd, std::error_code& ec) noexcept
{
  if (::flock(fd, LOCK_UN | LOCK_NB) == -1)
  {
    ec = std::error_code{errno, std::generic_category()};
    return false;
  }
  return true;
}

/**
 * @brief The MetadataFile class provides functionality to read/write data to a file.
 */
class MetadataFile
{
public:
  ~MetadataFile()
  {
    if (_file)
    {
      std::fflush(_file);

      std::error_code ec;
      unlock_file(fileno(_file), ec);

      std::fclose(_file);
    }
  }

  [[nodiscard]] bool init_writer(std::filesystem::path const& path) { return _init(path, "a"); }

  [[nodiscard]] bool init_reader(std::filesystem::path const& path)
  {
    _line.resize(2048);
    return _init(path, "r");
  }

  [[nodiscard]] bool write(void const* buffer, size_t count, std::error_code& ec)
  {
    assert(_file);

    size_t const written = std::fwrite(buffer, sizeof(char), count, _file);

    if (written != count)
    {
      ec = std::error_code{errno, std::generic_category()};
      return false;
    }

    return true;
  }

  [[nodiscard]] FILE* file_ptr() noexcept { return _file; }

private:
  [[nodiscard]] bool _init(std::filesystem::path const& path, char const* mode)
  {
    _file = std::fopen(path.c_str(), mode);

    if (!_file)
    {
      return false;
    }

    std::error_code ec;
    bool locked;
    do
    {
      locked = lock_file(fileno(_file), ec);
    } while (ec.value() == EWOULDBLOCK);

    if (!locked)
    {
      return false;
    }

    return true;
  }

private:
  std::string _line;
  std::FILE* _file{nullptr};
};

[[nodiscard]] inline std::filesystem::path resolve_base_dir(std::error_code& ec,
                                                            std::string_view base_dir = std::string_view{}) noexcept
{
  return base_dir.empty() ? (std::filesystem::exists("/dev/shm", ec) ? "/dev/shm/bitlog" : "/tmp/bitlog")
                          : base_dir;
}
} // namespace bitlog::detail