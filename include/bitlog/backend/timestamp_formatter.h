/**
 * Copyright(c) 2024-present, Odysseas Georgoudis & Bitlog contributors.
 * Distributed under the MIT License (http://opensource.org/licenses/MIT)
 */

#pragma once

// Include always common first as it defines FMTBITLOG_HEADER_ONLY
#include "bitlog/backend/backend_types.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "bitlog/backend/cached_timestamp_builder.h"

/** forward declarations **/
struct tm;

namespace bitlog::detail
{

/**
 * Formats a timestamp given a format string as a pattern. The format pattern uses the
 * same format specifiers as strftime() but with the following additional specifiers :
 * 1) %Qms - Milliseconds
 * 2) %Qus - Microseconds
 * 3) %Qns - Nanoseconds
 * @note %Qms, %Qus, %Qns specifiers are mutually exclusive
 * e.g given : "%I:%M.%Qms%p" the output would be "03:21.343PM"
 */
class TimestampFormatter
{
private:
  enum AdditionalSpecifier : uint8_t
  {
    None,
    Qms,
    Qus,
    Qns
  };

public:
  /**
   * Constructor
   * @param timestamp_format_string  format string
   * @param timezone_type local time or gmt time, defaults to local time
   */
  explicit TimestampFormatter(std::string const& timestamp_format_string, Timezone timezone_type = Timezone::LocalTime)
    : _timezone_type(timezone_type)
  {
    assert((_timezone_type == Timezone::LocalTime || _timezone_type == Timezone::GmtTime) &&
           "Invalid timezone type");

    // store the beginning of the found specifier
    size_t specifier_begin{std::string::npos};

    // look for all three special specifiers

    if (size_t const search_qms = timestamp_format_string.find(SPECIFIER_NAME[AdditionalSpecifier::Qms]);
        search_qms != std::string::npos)
    {
      _additional_format_specifier = AdditionalSpecifier::Qms;
      specifier_begin = search_qms;
    }

    if (size_t const search_qus = timestamp_format_string.find(SPECIFIER_NAME[AdditionalSpecifier::Qus]);
        search_qus != std::string::npos)
    {
      if (specifier_begin != std::string::npos)
      {
        // TODO:: Handle Error
      }

      _additional_format_specifier = AdditionalSpecifier::Qus;
      specifier_begin = search_qus;
    }

    if (size_t const search_qns = timestamp_format_string.find(SPECIFIER_NAME[AdditionalSpecifier::Qns]);
        search_qns != std::string::npos)
    {
      if (specifier_begin != std::string::npos)
      {
        // TODO:: Handle Error
      }

      _additional_format_specifier = AdditionalSpecifier::Qns;
      specifier_begin = search_qns;
    }

    if (specifier_begin == std::string::npos)
    {
      // If no additional specifier was found then we can simply store the whole format string
      assert(_additional_format_specifier == AdditionalSpecifier::None);
      _format_part_1 = timestamp_format_string;
    }
    else
    {
      // We now the index where the specifier begins so copy everything until there from beginning
      _format_part_1 = timestamp_format_string.substr(0, specifier_begin);

      // Now copy the remaining format string, ignoring the specifier
      size_t const specifier_end = specifier_begin + SPECIFIER_LENGTH;

      _format_part_2 =
        timestamp_format_string.substr(specifier_end, timestamp_format_string.length() - specifier_end);
    }

    // Now init two custom string from time classes with pre-formatted strings
    _ctb_part_1.init(_format_part_1, _timezone_type);
    _ctb_part_2.init(_format_part_2, _timezone_type);
  }

  /**
   * Formats a strings given a timestamp
   * @param time_since_epoch the timestamp from epoch
   * @return formatted string
   */
  [[nodiscard]] std::string_view format_timestamp(std::chrono::nanoseconds time_since_epoch)
  {
    int64_t const timestamp_ns = time_since_epoch.count();

    // convert timestamp to seconds
    int64_t const timestamp_secs = timestamp_ns / 1'000'000'000;

    // First always clear our cached string
    _formatted_date.clear();

    // 1. we always format part 1
    _formatted_date += _ctb_part_1.format_timestamp(timestamp_secs);

    // 2. We add any special ms/us/ns specifier if any
    auto const extracted_ns = static_cast<uint32_t>(timestamp_ns - (timestamp_secs * 1'000'000'000));

    if (_additional_format_specifier == AdditionalSpecifier::Qms)
    {
      uint32_t const extracted_ms = extracted_ns / 1'000'000;
      constexpr char const* zeros = "000";

      // Add as many zeros as the extracted_fractional_seconds_length
      _formatted_date += zeros;

      _append_fractional_seconds(extracted_ms);
    }
    else if (_additional_format_specifier == AdditionalSpecifier::Qus)
    {
      uint32_t const extracted_us = extracted_ns / 1'000;
      constexpr char const* zeros = "000000";

      // Add as many zeros as the extracted_fractional_seconds_length
      _formatted_date += zeros;

      _append_fractional_seconds(extracted_us);
    }
    else if (_additional_format_specifier == AdditionalSpecifier::Qns)
    {
      constexpr char const* zeros = "000000000";

      // Add as many zeros as the extracted_fractional_seconds_length
      _formatted_date += zeros;

      _append_fractional_seconds(extracted_ns);
    }

    // 3. format part 2 after fractional seconds - if any
    if (!_format_part_2.empty())
    {
      _formatted_date += _ctb_part_2.format_timestamp(timestamp_secs);
    }

    return _formatted_date;
  }

private:
  /**
   * Append fractional seconds to the formatted strings
   * @param extracted_fractional_seconds the fractional seconds extracted e.g 123 or 654332 etc
   */
  void _append_fractional_seconds(uint32_t extracted_fractional_seconds)
  {
    // Format the seconds and add them
    fmtbitlog::format_int const extracted_ms_string{extracted_fractional_seconds};

    // _formatted_date.size() - extracted_ms_string.size() is where we want to begin placing the fractional seconds
    memcpy(&_formatted_date[_formatted_date.size() - extracted_ms_string.size()],
           extracted_ms_string.data(), extracted_ms_string.size());
  }

private:
  // Contains the additional specifier name, at the same index as the enum
  static constexpr std::array<char const*, 4> SPECIFIER_NAME{"", "%Qms", "%Qus", "%Qns"};

  // All special specifiers have same length at the moment
  static constexpr size_t SPECIFIER_LENGTH = 4u;

  /** As class member to avoid re-allocating **/
  std::string _formatted_date;

  /** The format string is broken down to two parts. Before and after our additional specifiers */
  std::string _format_part_1;
  std::string _format_part_2;

  /** Strftime cache for both parts of the string */
  CachedTimestampBuilder _ctb_part_1;
  CachedTimestampBuilder _ctb_part_2;

  /** Timezone, Local time by default **/
  Timezone _timezone_type;

  /** fractional seconds */
  AdditionalSpecifier _additional_format_specifier{AdditionalSpecifier::None};
};

} // namespace bitlog::detail