#pragma once

// Include always common first as it defines FMTBITLOG_HEADER_ONLY
#include "bitlog/common/common.h"

#include <string_view>
#include <vector>

#include "bitlog/bundled/fmt/core.h"

namespace bitlog::detail
{
/**
 * @brief Decodes an argument from a buffer and adds it to the vector of format arguments.
 *
 * @param buffer Pointer to the buffer containing the encoded argument.
 * @param fmt_args Vector of format arguments to which the decoded argument will be added.
 */
template <typename T>
inline void decode_arg(uint8_t const*& buffer,
                       std::vector<fmtbitlog::basic_format_arg<fmtbitlog::format_context>>& fmt_args) noexcept
{
  T value;
  std::memcpy(&value, buffer, sizeof(value));
  fmt_args.push_back(fmtbitlog::detail::make_arg<fmtbitlog::format_context>(value));
  buffer += sizeof(value);
}

/**
 * @brief Decodes a string argument from a buffer and adds it to the vector of format arguments.
 *
 * @param length The length of the string.
 * @param buffer Pointer to the buffer containing the encoded string.
 * @param fmt_args Vector of format arguments to which the decoded string will be added.
 */
inline void decode_string_arg(uint32_t length, uint8_t const*& buffer,
                              std::vector<fmtbitlog::basic_format_arg<fmtbitlog::format_context>>& fmt_args) noexcept
{
  std::string_view const value{reinterpret_cast<char const*>(buffer), length};
  fmt_args.push_back(fmtbitlog::detail::make_arg<fmtbitlog::format_context>(value));
  buffer += value.size();
}

/**
 * @brief Decodes multiple arguments from a buffer based on type descriptors and adds them to the vector of format arguments.
 *
 * @param buffer Pointer to the buffer containing the encoded arguments.
 * @param type_descriptors Vector of type descriptors specifying the types of arguments to decode.
 * @param fmt_args Vector of format arguments to which the decoded arguments will be added.
 */
inline void decode(uint8_t const*& buffer, std::vector<TypeDescriptorName> const& type_descriptors,
                   std::vector<fmtbitlog::basic_format_arg<fmtbitlog::format_context>>& fmt_args) noexcept
{
  fmt_args.clear();

  for (auto type_descriptor : type_descriptors)
  {
    switch (type_descriptor)
    {
    case TypeDescriptorName::Char:
      decode_arg<char>(buffer, fmt_args);
      break;
    case TypeDescriptorName::SignedChar:
      decode_arg<signed char>(buffer, fmt_args);
      break;
    case TypeDescriptorName::UnsignedChar:
      decode_arg<unsigned char>(buffer, fmt_args);
      break;
    case TypeDescriptorName::ShortInt:
      decode_arg<short int>(buffer, fmt_args);
      break;
    case TypeDescriptorName::UnsignedShortInt:
      decode_arg<unsigned short int>(buffer, fmt_args);
      break;
    case TypeDescriptorName::Int:
      decode_arg<int>(buffer, fmt_args);
      break;
    case TypeDescriptorName::UnsignedInt:
      decode_arg<unsigned int>(buffer, fmt_args);
      break;
    case TypeDescriptorName::LongInt:
      decode_arg<long int>(buffer, fmt_args);
      break;
    case TypeDescriptorName::UnsignedLongInt:
      decode_arg<unsigned long int>(buffer, fmt_args);
      break;
    case TypeDescriptorName::LongLongInt:
      decode_arg<long long int>(buffer, fmt_args);
      break;
    case TypeDescriptorName::UnsignedLongLongInt:
      decode_arg<unsigned long long int>(buffer, fmt_args);
      break;
    case TypeDescriptorName::Float:
      decode_arg<float>(buffer, fmt_args);
      break;
    case TypeDescriptorName::Double:
      decode_arg<double>(buffer, fmt_args);
      break;
    case TypeDescriptorName::CString:
    {
      decode_string_arg(strlen(reinterpret_cast<char const*>(buffer)), buffer, fmt_args);
      buffer += 1; // Also include the size of the null-terminated character
      break;
    }
    case TypeDescriptorName::CStringArray:
    case TypeDescriptorName::StdString:
    {
      uint32_t len;
      std::memcpy(&len, buffer, sizeof(len));
      buffer += sizeof(len);

      decode_string_arg(len, buffer, fmt_args);
      // There is no null-terminated character to append like in the CString case
      break;
    }
    default:
      // TODO:: error, unsupported type ?
      break;
    }
  }
}
} // namespace bitlog::detail