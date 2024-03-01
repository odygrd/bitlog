#pragma once

// Include always common first as it defines FMTBITLOG_HEADER_ONLY
#include "bitlog/common/common.h"

#include <charconv>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "bitlog/bundled/fmt/format.h"
#include "bitlog/common/bounded_queue.h"
#include "decode.h"

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
inline std::pair<std::vector<LogStatementMetadata>, std::string> read_log_statement_metadata_file(
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
inline std::vector<LoggerMetadata> read_loggers_metadata_file(std::filesystem::path const& path)
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

/**
 * @brief Discovers and populates a vector with thread queues found in the specified directory.
 *
 * The function looks for ".ready" files, such as "0.0.ready" or "1.0.ready," representing
 * thread number and sequence information. The discovered thread queues are sorted and added to
 * the provided vector.
 *
 * @param run_dir The path to the directory containing thread queue files.
 * @param ready_queues A vector to store discovered thread queue information.
 *        Each element is a pair representing thread number and sequence.
 * @param ec An output parameter for error codes.
 * @return `true` if the discovery process is successful, `false` otherwise.
 *         If an error occurs, the error code is set in the `ec` parameter.
 */
[[nodiscard]] inline bool discover_queues(std::filesystem::path const& run_dir,
                                          std::vector<std::pair<uint32_t, uint32_t>>& ready_queues,
                                          std::error_code& ec)
{
  ready_queues.clear();

  auto run_dir_it = std::filesystem::directory_iterator(run_dir, ec);

  if (ec && ec != std::errc::no_such_file_or_directory)
  {
    return false;
  }

  for (auto const& file_entry : run_dir_it)
  {
    // Look into all the files under the current run directory
    if (!file_entry.is_regular_file()) [[unlikely]]
    {
      // should never happen but just in case
      continue;
    }

    // look for .ready files, e.g 0.0.ready, 1.0.ready (thread_num.sequence.ready)
    if (file_entry.path().extension() == ".ready")
    {
      // e.g. 0.0 for a file 0.0.ready
      std::string const file_stem = file_entry.path().stem().string();
      size_t const dot_position = file_stem.find('.');

      std::string_view const thread_num_str{file_stem.data(), dot_position};
      uint32_t thread_num;
      auto [ptr1, fec1] = std::from_chars(
        thread_num_str.data(), thread_num_str.data() + thread_num_str.length(), thread_num);

      if ((fec1 == std::errc::invalid_argument) || (fec1 == std::errc::result_out_of_range))
      {
        ec = std::make_error_code(fec1);
        return false;
      }

      std::string_view const sequence_str{file_stem.data() + dot_position + 1,
                                          file_stem.size() - dot_position - 1};
      uint32_t sequence;
      auto [ptr2, fec2] =
        std::from_chars(sequence_str.data(), sequence_str.data() + sequence_str.length(), sequence);

      if ((fec2 == std::errc::invalid_argument) || (fec2 == std::errc::result_out_of_range))
      {
        ec = std::make_error_code(fec2);
        return false;
      }

      auto insertion_point = std::lower_bound(
        std::begin(ready_queues), std::end(ready_queues), std::make_pair(thread_num, sequence), [](const auto& lhs, const auto& rhs)
        { return lhs.first < rhs.first || (lhs.first == rhs.first && lhs.second < rhs.second); });

      ready_queues.insert(insertion_point, std::make_pair(thread_num, sequence));
    }
  }

  return true;
}

/**
 * @brief Finds the next sequence queue for the given thread and current sequence.
 *
 * This function performs a binary search in the sorted vector of thread queues to discover
 * the next sequence for the specified thread and sequence. If a next sequence is found,
 * it is returned; otherwise, std::nullopt is returned.
 *
 * @param thread_num The thread number.
 * @param sequence The current sequence.
 * @param ready_queues The vector of thread queues to search.
 * @return An optional containing the next sequence if found, or std::nullopt otherwise.
 */
[[nodiscard]] inline std::optional<uint32_t> find_next_queue_sequence(
  uint32_t thread_num, uint32_t sequence, std::vector<std::pair<uint32_t, uint32_t>> const& ready_queues)
{
  auto it = std::lower_bound(std::cbegin(ready_queues), std::cend(ready_queues),
                             std::make_pair(thread_num, sequence), [](auto const& lhs, auto const& rhs)
                             { return lhs.first == rhs.first && lhs.second < rhs.second; });

  while (it != std::cend(ready_queues) && it->first == thread_num)
  {
    if (it->second > sequence)
    {
      return it->second;
    }
    ++it;
  }

  return std::nullopt;
}

template <typename TQueue>
struct QueueInfo
{
  QueueInfo(uint32_t thread_num, uint32_t sequence, std::filesystem::path const& queue_path)
    : thread_num(thread_num), sequence(sequence), queue(std::make_unique<TQueue>())
  {
    lock_file_fd = ::open(queue_path.c_str(), O_RDONLY);
  }

  std::unique_ptr<TQueue> queue;
  uint32_t thread_num;
  uint32_t sequence;
  int lock_file_fd;
};

/**
 * @brief Creates and inserts a queue into the active_thread_queues vector.
 *
 * @param thread_num The thread number.
 * @param sequence The sequence number.
 * @param active_thread_queues The vector of active queues.
 * @param run_dir The directory path.
 * @return True if successful, false otherwise.
 */
template <typename TQueue>
[[nodiscard]] bool create_and_insert_queue_to_active_thread_queues(uint32_t thread_num, uint32_t sequence,
                                                                   std::vector<QueueInfo<TQueue>>& active_thread_queues,
                                                                   std::filesystem::path const& run_dir)
{
  std::filesystem::path const queue_path = run_dir / fmtbitlog::format("{}.{}.lock", thread_num, sequence);
  QueueInfo<TQueue> queue_info{thread_num, sequence, queue_path};

  if (queue_info.lock_file_fd == -1)
  {
    // failed to open the lock file
    return false;
  }

  std::error_code ec;

  // TODO: Fix memory page size config
  if (!queue_info.queue->open(queue_path, MemoryPageSize::RegularPage, ec))
  {
    return false;
  }

  auto insertion_point =
    std::lower_bound(std::begin(active_thread_queues), std::end(active_thread_queues), thread_num,
                     [](auto const& lhs, auto const& rhs) { return lhs.thread_num < rhs; });

  active_thread_queues.insert(insertion_point, std::move(queue_info));

  return true;
}

/**
 * @brief Updates the information of active queues based on the ready queues.
 *
 * @param active_thread_queues The vector of active queues.
 * @param ready_queues The vector of ready queues.
 * @param run_dir The directory path.
 */
template <typename TQueue>
void update_active_queue_infos(std::vector<QueueInfo<TQueue>>& active_thread_queues,
                               std::vector<std::pair<uint32_t, uint32_t>> const& ready_queues,
                               std::filesystem::path const& run_dir)
{
  uint32_t last_thread_num = std::numeric_limits<uint32_t>::max();

  for (auto [thread_num, sequence] : ready_queues)
  {
    if (thread_num == last_thread_num)
    {
      // skip future sequences
      continue;
    }

    last_thread_num = thread_num;

    // See if we have that thread num in our active_thread_queues. We still need to do a find here
    // as for example we can have e.g. active thread_num 0,1,2 queues. But when thread num 1
    // finishes we will have 0,2 in our vector, therefore just using the index of the vector
    // for the thread num is not enough
    auto current_active_queue_it = std::lower_bound(
      std::begin(active_thread_queues), std::end(active_thread_queues), thread_num,
      [](QueueInfo<TQueue> const& lhs, uint32_t value) { return lhs.thread_num < value; });

    std::error_code ec;

    if ((current_active_queue_it == std::end(active_thread_queues)) ||
        (current_active_queue_it->thread_num != thread_num))
    {
      // Queue doesn't exist so we create it
      if (!create_and_insert_queue_to_active_thread_queues(thread_num, sequence, active_thread_queues, run_dir))
      {
        continue;
      }
    }
    else if (current_active_queue_it->sequence == sequence && current_active_queue_it->queue->empty())
    {
      // we have the minimum sequence queue already in active_thread_queues and the queue is empty
      // Check if we need to remove an empty queue
      if (std::optional<uint32_t> const next_sequence = find_next_queue_sequence(thread_num, sequence, ready_queues);
          next_sequence.has_value())
      {
        // This means that the producer has moved to a new queue
        // we want to remove it and replace it with the next sequence queue
        active_thread_queues.erase(current_active_queue_it);
        if (!TQueue::remove_shm_files(fmtbitlog::format("{}.{}.ext", thread_num, sequence), run_dir, ec))
        {
          // TODO:: handle error ?
        }

        if (!create_and_insert_queue_to_active_thread_queues(thread_num, *next_sequence, active_thread_queues, run_dir))
        {
          continue;
        }
      }
      else if (lock_file(current_active_queue_it->lock_file_fd, ec))
      {
        // if we can lock the lock file it means the producer process is not running anymore
        active_thread_queues.erase(current_active_queue_it);
        if (!TQueue::remove_shm_files(fmtbitlog::format("{}.{}.ext", thread_num, sequence), run_dir, ec))
        {
          // TODO:: handle error ?
        }
      }
    }
  }
}

} // namespace bitlog::detail