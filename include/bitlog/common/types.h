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