#pragma once

#include <cstddef>
#include <cstdint>
#include <algorithm>

#include <sys/syscall.h>
#include <unistd.h>

#if defined(__x86_64__) || defined(_M_X64)
  #define BITLOG_X86_ARCHITECTURE_ENABLED
#endif

// Attributes and Macros
#ifdef NDEBUG // Check if not in debug mode
  #define BITLOG_ALWAYS_INLINE [[gnu::always_inline]]
#else
  #define BITLOG_ALWAYS_INLINE
#endif

namespace bitlog
{
enum MemoryPageSize : uint32_t
{
  RegularPage = 0,
  HugePage2MB = 2 * 1024 * 1024,
  HugePage1GB = 1024 * 1024 * 1024
};

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
}

namespace bitlog::detail
{
/** Constants **/
static constexpr size_t CACHE_LINE_SIZE_BYTES{64u};
static constexpr size_t CACHE_LINE_ALIGNED{2 * CACHE_LINE_SIZE_BYTES};

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
[[nodiscard]] inline uint32_t get_thread_id() noexcept
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
[[nodiscard]] inline size_t get_page_size() noexcept
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
 * Enum defining different type descriptors
 */
enum class TypeDescriptorName : uint8_t
{
  None = 0,
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
  Double
};
} // namespace bitlog::detail