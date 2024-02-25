#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <type_traits>

#include "bitlog/detail/common.h"
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

template <>
struct GetTypeDescriptor<char>
{
  static constexpr TypeDescriptorName value{TypeDescriptorName::SignedChar};
};

template <>
struct GetTypeDescriptor<unsigned char>
{
  static constexpr TypeDescriptorName value{TypeDescriptorName::UnsignedChar};
};

template <>
struct GetTypeDescriptor<short int>
{
  static constexpr TypeDescriptorName value{TypeDescriptorName::ShortInt};
};

template <>
struct GetTypeDescriptor<unsigned short int>
{
  static constexpr TypeDescriptorName value{TypeDescriptorName::UnsignedShortInt};
};

template <>
struct GetTypeDescriptor<int>
{
  static constexpr TypeDescriptorName value{TypeDescriptorName::Int};
};

template <>
struct GetTypeDescriptor<unsigned int>
{
  static constexpr TypeDescriptorName value{TypeDescriptorName::UnsignedInt};
};

template <>
struct GetTypeDescriptor<long int>
{
  static constexpr TypeDescriptorName value{TypeDescriptorName::LongInt};
};

template <>
struct GetTypeDescriptor<unsigned long int>
{
  static constexpr TypeDescriptorName value{TypeDescriptorName::UnsignedLongInt};
};

template <>
struct GetTypeDescriptor<long long int>
{
  static constexpr TypeDescriptorName value{TypeDescriptorName::LongLongInt};
};

template <>
struct GetTypeDescriptor<unsigned long long int>
{
  static constexpr TypeDescriptorName value{TypeDescriptorName::UnsignedLongLongInt};
};

template <>
struct GetTypeDescriptor<float>
{
  static constexpr TypeDescriptorName value{TypeDescriptorName::Float};
};

template <>
struct GetTypeDescriptor<double>
{
  static constexpr TypeDescriptorName value{TypeDescriptorName::Double};
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
  auto get_arg_size = []<typename T>(uint32_t* c_style_string_lengths,
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

  return (0u + ... + get_arg_size(c_style_string_lengths, c_style_string_lengths_index, args));
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