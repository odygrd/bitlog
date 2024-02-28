#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <mutex>
#include <string>
#include <string_view>
#include <system_error>
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

/**
 * Initialises once, intended for use with templated classes.
 * This function ensures that the initialization is performed only once,
 * even when instantiated with different template parameters.
 * @return true if initialization is performed, false otherwise.
 */
[[nodiscard]] bool initialise_frontend_once()
{
  static std::once_flag once_flag;
  bool init_called{false};
  std::call_once(once_flag, [&init_called]() mutable { init_called = true; });
  return init_called;
}
} // namespace bitlog::detail