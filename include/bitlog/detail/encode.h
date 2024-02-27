#pragma once

#include "bitlog/detail/common.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <type_traits>

#include "bitlog/detail/rte_memcpy.h"

namespace bitlog::detail
{
/**
 * Primary template for unsupported types
 */
template <typename T>
struct GetTypeDescriptor
{
  static_assert(false, "Unsupported type");
};

// Specializations for supported types
#define BITLOG_SUPPORTED_TYPE_SPECIALIZATION(Type, Descriptor)                                     \
  template <>                                                                                      \
  struct GetTypeDescriptor<Type>                                                                   \
  {                                                                                                \
    static constexpr TypeDescriptorName value = Descriptor;                                        \
  };                                                                                               \
                                                                                                   \
  template <>                                                                                      \
  struct GetTypeDescriptor<Type const>                                                             \
  {                                                                                                \
    static constexpr TypeDescriptorName value = Descriptor;                                        \
  };                                                                                               \
                                                                                                   \
  template <>                                                                                      \
  struct GetTypeDescriptor<Type&>                                                                  \
  {                                                                                                \
    static constexpr TypeDescriptorName value = Descriptor;                                        \
  };                                                                                               \
                                                                                                   \
  template <>                                                                                      \
  struct GetTypeDescriptor<Type const&>                                                            \
  {                                                                                                \
    static constexpr TypeDescriptorName value = Descriptor;                                        \
  };

BITLOG_SUPPORTED_TYPE_SPECIALIZATION(char, TypeDescriptorName::Char)
BITLOG_SUPPORTED_TYPE_SPECIALIZATION(signed char, TypeDescriptorName::SignedChar)
BITLOG_SUPPORTED_TYPE_SPECIALIZATION(unsigned char, TypeDescriptorName::UnsignedChar)
BITLOG_SUPPORTED_TYPE_SPECIALIZATION(short int, TypeDescriptorName::ShortInt)
BITLOG_SUPPORTED_TYPE_SPECIALIZATION(unsigned short int, TypeDescriptorName::UnsignedShortInt)
BITLOG_SUPPORTED_TYPE_SPECIALIZATION(int, TypeDescriptorName::Int)
BITLOG_SUPPORTED_TYPE_SPECIALIZATION(unsigned int, TypeDescriptorName::UnsignedInt)
BITLOG_SUPPORTED_TYPE_SPECIALIZATION(long int, TypeDescriptorName::LongInt)
BITLOG_SUPPORTED_TYPE_SPECIALIZATION(unsigned long int, TypeDescriptorName::UnsignedLongInt)
BITLOG_SUPPORTED_TYPE_SPECIALIZATION(long long int, TypeDescriptorName::LongLongInt)
BITLOG_SUPPORTED_TYPE_SPECIALIZATION(unsigned long long int, TypeDescriptorName::UnsignedLongLongInt)
BITLOG_SUPPORTED_TYPE_SPECIALIZATION(float, TypeDescriptorName::Float)
BITLOG_SUPPORTED_TYPE_SPECIALIZATION(double, TypeDescriptorName::Double)
BITLOG_SUPPORTED_TYPE_SPECIALIZATION(char const*, TypeDescriptorName::CString)
BITLOG_SUPPORTED_TYPE_SPECIALIZATION(char*, TypeDescriptorName::CString)
BITLOG_SUPPORTED_TYPE_SPECIALIZATION(std::string, TypeDescriptorName::StdString)
BITLOG_SUPPORTED_TYPE_SPECIALIZATION(std::string_view, TypeDescriptorName::StdString)

#undef BITLOG_SUPPORTED_TYPE_SPECIALIZATION

template <std::size_t N>
struct GetTypeDescriptor<char[N]>
{
  static constexpr TypeDescriptorName value = TypeDescriptorName::CStringArray;
};

template <std::size_t N>
struct GetTypeDescriptor<const char[N]>
{
  static constexpr TypeDescriptorName value = TypeDescriptorName::CStringArray;
};

template <std::size_t N>
struct GetTypeDescriptor<char const (&)[N]>
{
  static constexpr TypeDescriptorName value = TypeDescriptorName::CStringArray;
};

template <std::size_t N>
struct GetTypeDescriptor<char (&)[N]>
{
  static constexpr TypeDescriptorName value = TypeDescriptorName::CStringArray;
};

/**
 * @brief Checks if the provided type is a C-style string (char const*, char*, or const char[]).
 * @return True if the type is a C-style string, false otherwise.
 */
template <typename T>
[[nodiscard]] constexpr bool is_c_style_string_type() noexcept
{
  using arg_type = std::decay_t<T>;
  return std::disjunction_v<std::is_same<arg_type, char const*>, std::is_same<arg_type, char*>>;
}

/**
 * @brief Checks if the provided type is a std::string or std::string_view type.
 * @return True if the type is a std::string or std::string_view type, false otherwise.
 */
template <typename T>
[[nodiscard]] constexpr bool is_std_string_type() noexcept
{
  using arg_type = std::decay_t<T>;
  return std::disjunction_v<std::is_same<arg_type, std::string>, std::is_same<arg_type, std::string_view>>;
}

/**
 * @brief Checks if the provided type is a char array (char[]).
 * @return True if the type is a char array, false otherwise.
 */
template <typename T>
[[nodiscard]] constexpr bool is_char_array_type() noexcept
{
  using arg_type = std::remove_cvref_t<T>;
  return std::conjunction_v<std::is_array<arg_type>, std::is_same<std::remove_cvref_t<std::remove_extent_t<arg_type>>, char>>;
}

/**
 * @brief Counts the number of C-style string arguments among the provided arguments.
 * @return The count of C-style string arguments.
 */
template <typename... Args>
[[nodiscard]] constexpr uint32_t count_c_style_strings() noexcept
{
  return (0u + ... + is_c_style_string_type<Args>());
}

/**
 * @brief Calculates the total size required to encode the provided arguments and populates the c_style_string_lengths array.
 * @tparam Args Variadic template for the function arguments.
 * @param c_style_string_lengths Array to store the c_style_string_lengths of C-style strings and char arrays.
 * @param args The arguments to be encoded.
 * @return The total size required to encode the arguments.
 */
template <typename... Args>
[[nodiscard]] constexpr uint32_t calculate_args_size_and_populate_string_lengths(uint32_t* c_style_string_lengths,
                                                                                 Args const&... args) noexcept
{
  uint32_t c_style_string_lengths_index{0};
  auto arg_size = []<typename T>(uint32_t* c_style_string_lengths,
                                 uint32_t& c_style_string_lengths_index, T const& arg)
  {
    if constexpr (is_char_array_type<T>())
    {
      c_style_string_lengths[c_style_string_lengths_index] = strnlen(arg, std::extent_v<T>);
      return static_cast<uint32_t>(sizeof(uint32_t) + c_style_string_lengths[c_style_string_lengths_index++]);
    }
    else if constexpr (is_c_style_string_type<T>())
    {
      // include one extra for the zero termination
      c_style_string_lengths[c_style_string_lengths_index] = strlen(arg) + 1u;
      return c_style_string_lengths[c_style_string_lengths_index++];
    }
    else if constexpr (is_std_string_type<T>())
    {
      return static_cast<uint32_t>(sizeof(uint32_t) + arg.length());
    }
    else
    {
      return static_cast<uint32_t>(sizeof(arg));
    }
  };

  return (0u + ... + arg_size(c_style_string_lengths, c_style_string_lengths_index, args));
}

/**
 * @brief Encodes an argument into a buffer.
 * @param buffer Pointer to the buffer for encoding.
 * @param c_style_string_lengths Array storing the c_style_string_lengths of C-style strings and char arrays.
 * @param c_style_string_lengths_index Index of the current string/array length in c_style_string_lengths.
 * @param arg The argument to be encoded.
 */
template <bool RteMemCpy, typename T>
inline void encode_arg(uint8_t*& buffer, uint32_t const* c_style_string_lengths,
                       uint32_t& c_style_string_lengths_index, T const& arg) noexcept
{
  if constexpr (is_char_array_type<T>())
  {
    // To support non-zero terminated arrays, copy the length first and then the actual string
    uint32_t const len = c_style_string_lengths[c_style_string_lengths_index++];

    if (RteMemCpy)
    {
      rte_memcpy(buffer, &len, sizeof(uint32_t));
      rte_memcpy(buffer + sizeof(uint32_t), arg, len);
    }
    else
    {
      std::memcpy(buffer, &len, sizeof(uint32_t));
      std::memcpy(buffer + sizeof(uint32_t), arg, len);
    }

    buffer += sizeof(uint32_t) + len;
  }
  else if constexpr (is_c_style_string_type<T>())
  {
    uint32_t const len = c_style_string_lengths[c_style_string_lengths_index++];

    if (RteMemCpy)
    {
      rte_memcpy(buffer, arg, len);
    }
    else
    {
      std::memcpy(buffer, arg, len);
    }

    buffer += len;
  }
  else if constexpr (is_std_string_type<T>())
  {
    // Copy the length first and then the actual string
    auto const len = static_cast<uint32_t>(arg.length());

    if (RteMemCpy)
    {
      rte_memcpy(buffer, &len, sizeof(uint32_t));
      rte_memcpy(buffer + sizeof(uint32_t), arg.data(), len);
    }
    else
    {
      std::memcpy(buffer, &len, sizeof(uint32_t));
      std::memcpy(buffer + sizeof(uint32_t), arg.data(), len);
    }

    buffer += sizeof(uint32_t) + len;
  }
  else
  {
    if (RteMemCpy)
    {
      rte_memcpy(buffer, &arg, sizeof(arg));
    }
    else
    {
      std::memcpy(buffer, &arg, sizeof(arg));
    }

    buffer += sizeof(arg);
  }
}

/**
 * @brief Encodes multiple arguments into a buffer.
 * @param buffer Pointer to the buffer for encoding.
 * @param c_style_string_lengths Array storing the c_style_string_lengths of C-style strings and char arrays.
 * @param args The arguments to be encoded.
 */
template <bool RteMemCpy, typename... Args>
inline void encode(uint8_t*& buffer, uint32_t const* c_style_string_lengths, Args const&... args) noexcept
{
  uint32_t c_style_string_lengths_index{0};
  (encode_arg<RteMemCpy>(buffer, c_style_string_lengths, c_style_string_lengths_index, args), ...);
}
} // namespace bitlog::detail