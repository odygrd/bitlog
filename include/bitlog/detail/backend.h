#pragma once

// Include always common first as it defines FMTBITLOG_HEADER_ONLY
#include "bitlog/detail/common.h"

#include <charconv>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "bitlog/bundled/fmt/format.h"
#include "bitlog/detail/decode.h"

namespace bitlog::detail
{
/**
 * @brief Structure to store metadata information for a log statement.
 */
struct LogStatementMetadata
{
  std::vector<TypeDescriptorName> type_descriptors;
  std::string file;
  std::string function;
  std::string log_format;
  std::string line;
  LogLevel level{LogLevel::None};
};

/**
 * @brief Structure to store metadata information for a logger.
 */
struct LoggerMetadata
{
  std::string name;
};

/**
 * @brief Extracts the value from a line based on the starting keyword.
 *
 * This function extracts the value from a line that starts with a specified keyword.
 *
 * @param line The input string view containing the line to process.
 * @param starts_with The keyword indicating the start of the value.
 * @return The extracted value as a string view.
 */
[[nodiscard]] inline std::string_view extract_value_from_line(std::string_view line, std::string_view starts_with)
{
  // exclude ':' delimiter and leading whitespace
  size_t const pos = starts_with.size() + 2;
  return std::string_view{line.data() + pos, line.size() - pos - 1};
}

/**
 * @brief Splits a string based on a delimiter and returns a vector of string views.
 *
 * This function splits a string based on a delimiter and returns a vector of string views.
 *
 * @param str The input string view to split.
 * @param delimiter The character used as the delimiter for splitting.
 * @return A vector of string views representing the split parts.
 */
[[nodiscard]] inline std::vector<std::string_view> split_string(std::string_view str, char delimiter)
{
  std::vector<std::string_view> tokens;

  size_t start = 0, end = 0;
  while ((end = str.find(delimiter, start)) != std::string_view::npos)
  {
    tokens.push_back(str.substr(start, end - start));
    start = end + 1;
  }
  tokens.push_back(str.substr(start));

  return tokens;
}

/**
 * @brief Reads the log statement metadata file and populates the log statement metadata.
 *
 * This function reads the specified log statement metadata file and populates the log statement metadata.
 * It is intended to be called once at the beginning.
 *
 * @param path The path to the log statement metadata file.
 * @return A pair containing the vector of log statement metadata and a string representing additional information.
 */
std::pair<std::vector<LogStatementMetadata>, std::string> inline read_log_statement_metadata_file(
  std::filesystem::path const& path)
{
  MetadataFile log_statements_metadata_file;
  std::pair<std::vector<LogStatementMetadata>, std::string> ret_val;

  // We only construct this object when we see the first .ready queue. It is guaranteed
  // that the log-statements-metadata.yaml is already there and unlocked.
  // We only try to lock it here again for extra safety
  if (!log_statements_metadata_file.init_reader(path / LOG_STATEMENTS_METADATA_FILENAME))
  {
    return ret_val;
  }

  // Read the file line by line
  char buffer[2048];
  while (std::fgets(buffer, sizeof(buffer), log_statements_metadata_file.file_ptr()))
  {
    auto line = std::string_view{buffer};

    if (line.starts_with("process_id"))
    {
      ret_val.second = extract_value_from_line(line, "process_id");
    }
    else if (line.starts_with("log_statements"))
    {
      long int prev_pos = std::ftell(log_statements_metadata_file.file_ptr());

      while (std::fgets(buffer, sizeof(buffer), log_statements_metadata_file.file_ptr()))
      {
        line = std::string_view{buffer};

        if (!line.starts_with("  "))
        {
          // finished reading log_statements block
          fseek(log_statements_metadata_file.file_ptr(), prev_pos, SEEK_SET);
          break;
        }
        else if (line.starts_with("  - id"))
        {
          auto const lsm_id_str = extract_value_from_line(line, "  - id");
          uint32_t lsm_id;
          auto [ptr1, fec1] =
            std::from_chars(lsm_id_str.data(), lsm_id_str.data() + lsm_id_str.length(), lsm_id);

          if (fec1 != std::errc{})
          {
            // TODO:: report error
          }

          if (lsm_id != ret_val.first.size())
          {
            // Expecting incrementing id
            std::abort();
          }

          auto& lsm = ret_val.first.emplace_back();

          // Read rest of lines for this id
          prev_pos = ftell(log_statements_metadata_file.file_ptr());

          while (std::fgets(buffer, sizeof(buffer), log_statements_metadata_file.file_ptr()))
          {
            line = std::string_view{buffer};

            if (!line.starts_with("    "))
            {
              // finished reading this log statement fields
              fseek(log_statements_metadata_file.file_ptr(), prev_pos, SEEK_SET);
              break;
            }
            else if (line.starts_with("    file"))
            {
              lsm.file = extract_value_from_line(line, "    file");
            }
            else if (line.starts_with("    line"))
            {
              lsm.line = extract_value_from_line(line, "    line");
            }
            else if (line.starts_with("    function"))
            {
              lsm.function = extract_value_from_line(line, "    function");
            }
            else if (line.starts_with("    log_format"))
            {
              lsm.log_format = extract_value_from_line(line, "    log_format");
            }
            else if (line.starts_with("    type_descriptors"))
            {
              std::string_view const type_descriptors =
                extract_value_from_line(line, "    type_descriptors");

              std::vector<std::string_view> const tokens = split_string(type_descriptors, ' ');

              for (auto const& type_descriptor : tokens)
              {
                uint8_t tdn;
                auto [ptr2, fec2] = std::from_chars(
                  type_descriptor.data(), type_descriptor.data() + type_descriptor.length(), tdn);

                if (fec2 != std::errc{})
                {
                  // TODO:: report error
                }

                lsm.type_descriptors.push_back(static_cast<TypeDescriptorName>(tdn));
              }
            }
            else if (line.starts_with("    log_level"))
            {
              auto const level_str = extract_value_from_line(line, "    log_level");
              uint8_t level;
              auto [ptr2, fec2] =
                std::from_chars(level_str.data(), level_str.data() + level_str.length(), level);

              if (fec2 != std::errc{})
              {
                // TODO:: report error
              }

              lsm.level = static_cast<LogLevel>(level);
            }
          }
        }
      }
    }
  }

  return ret_val;
}

/**
 * @brief Reads the logger metadata file and populates the logger metadata.
 *
 * This function reads the specified logger metadata file and populates the logger metadata.
 * It is intended to be called once at the beginning.
 *
 * @param path The path to the logger metadata file.
 * @return A vector containing the logger metadata.
 */
std::vector<LoggerMetadata> inline read_loggers_metadata_file(std::filesystem::path const& path)
{
  MetadataFile logger_metadata_file;
  std::vector<LoggerMetadata> ret_val;

  if (!logger_metadata_file.init_reader(path / LOGGERS_METADATA_FILENAME))
  {
    return ret_val;
  }

  // Read the file line by line
  char buffer[2048];
  while (fgets(buffer, sizeof(buffer), logger_metadata_file.file_ptr()))
  {
    auto line = std::string_view{buffer};

    if (line.starts_with("loggers"))
    {
      long int prev_pos = ftell(logger_metadata_file.file_ptr());

      while (fgets(buffer, sizeof(buffer), logger_metadata_file.file_ptr()))
      {
        line = std::string_view{buffer};

        if (!line.starts_with("  "))
        {
          // finished reading loggers block
          fseek(logger_metadata_file.file_ptr(), prev_pos, SEEK_SET);
          break;
        }
        else if (line.starts_with("  - id"))
        {
          auto const logger_id_str = extract_value_from_line(line, "  - id");
          uint32_t logger_id;
          auto [ptr1, fec1] = std::from_chars(
            logger_id_str.data(), logger_id_str.data() + logger_id_str.length(), logger_id);

          if (fec1 != std::errc{})
          {
            // TODO:: report error
          }

          if (logger_id != ret_val.size())
          {
            // Expecting incrementing id
            std::abort();
          }

          auto& lm = ret_val.emplace_back();

          // Read rest of lines for this id
          prev_pos = ftell(logger_metadata_file.file_ptr());
          while (fgets(buffer, sizeof(buffer), logger_metadata_file.file_ptr()))
          {
            line = std::string_view{buffer};

            if (!line.starts_with("    "))
            {
              // finished reading this log statement fields
              fseek(logger_metadata_file.file_ptr(), prev_pos, SEEK_SET);
              break;
            }
            else if (line.starts_with("    name"))
            {
              lm.name = extract_value_from_line(line, "    name");
            }
          }
        }
      }
    }
  }

  return ret_val;
}
} // namespace bitlog::detail