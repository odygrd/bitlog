#pragma once

// Include always common first as it defines FMTBITLOG_HEADER_ONLY
#include "bitlog/common/common.h"

#include <charconv>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "bitlog/backend/backend_types.h"
#include "bitlog/backend/pattern_formatter.h"
#include "bitlog/backend/sink.h"
#include "bitlog/bundled/fmt/format.h"
#include "bitlog/common/bounded_queue.h"
#include "decode.h"

#include <iostream> // TODO: Remove

namespace bitlog::detail
{
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
 * @param ec An output parameter for error codes.
 * @return A pair containing the vector of log statement metadata and a string representing additional information.
 */
[[nodiscard]] inline std::pair<std::vector<LogStatementMetadata>, std::string> read_log_statement_metadata_file(
  std::filesystem::path const& path, std::error_code& ec)
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
            ec = std::make_error_code(fec1);
          }

          if (lsm_id != ret_val.first.size())
          {
            // Expecting incrementing id
            std::abort();
          }

          std::string full_source_path;
          std::string source_line;
          std::string caller_function;
          std::string message_format;
          std::vector<TypeDescriptorName> type_descriptors;
          LogLevel log_level;

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
            else if (line.starts_with("    full_source_path"))
            {
              full_source_path = extract_value_from_line(line, "    full_source_path");
            }
            else if (line.starts_with("    source_line"))
            {
              source_line = extract_value_from_line(line, "    source_line");
            }
            else if (line.starts_with("    caller_function"))
            {
              caller_function = extract_value_from_line(line, "    caller_function");
            }
            else if (line.starts_with("    message_format"))
            {
              message_format = extract_value_from_line(line, "    message_format");
            }
            else if (line.starts_with("    type_descriptors"))
            {
              std::string_view const type_descriptors_str =
                extract_value_from_line(line, "    type_descriptors");

              std::vector<std::string_view> const tokens = split_string(type_descriptors_str, ' ');

              for (auto const& type_descriptor : tokens)
              {
                uint8_t tdn;
                auto [ptr2, fec2] = std::from_chars(
                  type_descriptor.data(), type_descriptor.data() + type_descriptor.length(), tdn);

                if (fec2 != std::errc{})
                {
                  ec = std::make_error_code(fec2);
                }

                type_descriptors.push_back(static_cast<TypeDescriptorName>(tdn));
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
                ec = std::make_error_code(fec2);
              }

              log_level = static_cast<LogLevel>(level);
            }
          }

          ret_val.first.emplace_back(std::move(full_source_path), std::move(source_line),
                                     std::move(caller_function), std::move(message_format),
                                     log_level, std::move(type_descriptors));
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
 * @param ec An output parameter for error codes.
 * @return A vector containing the logger metadata.
 */
[[nodiscard]] inline std::vector<LoggerMetadata> read_logger_metadata_file(std::filesystem::path const& path,
                                                                           std::error_code& ec)
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
            ec = std::make_error_code(fec1);
          }

          if (logger_id != ret_val.size())
          {
            // Expecting incrementing id
            std::abort();
          }

          std::string name;
          std::string log_record_pattern;
          std::string timestamp_pattern;
          Timezone timezone;
          SinkType sink_type;
          std::string output_file_path;
          uint64_t rotation_max_file_size = 0;
          uint64_t rotation_time_interval = 0;
          std::pair<std::chrono::hours, std::chrono::minutes> rotation_daily_at_time;
          uint32_t rotation_max_backup_files = std::numeric_limits<uint32_t>::max();
          FileOpenMode output_file_open_mode = FileOpenMode::Write;
          FileRotationFrequency rotation_time_frequency = FileRotationFrequency::Disabled;
          FileSuffix output_file_suffix = FileSuffix::None;
          bool rotation_overwrite_oldest_files = true;

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
              name = extract_value_from_line(line, "    name");
            }
            else if (line.starts_with("    log_record_pattern"))
            {
              log_record_pattern = extract_value_from_line(line, "    log_record_pattern");
            }
            else if (line.starts_with("    timestamp_pattern"))
            {
              timestamp_pattern = extract_value_from_line(line, "    timestamp_pattern");
            }
            else if (line.starts_with("    timezone"))
            {
              timezone =
                get_timezone_enum(std::string{extract_value_from_line(line, "    timezone")});
            }
            else if (line.starts_with("    sink_type"))
            {
              sink_type =
                get_sink_type_enum(std::string{extract_value_from_line(line, "    sink_type")});
            }
            else if (line.starts_with("    output_file_path"))
            {
              output_file_path = extract_value_from_line(line, "    output_file_path");
            }
            else if (line.starts_with("    rotation_max_file_size"))
            {
              std::string_view const rotation_max_file_size_str =
                extract_value_from_line(line, "    rotation_max_file_size");
              auto [ptr, fec] =
                std::from_chars(rotation_max_file_size_str.data(),
                                rotation_max_file_size_str.data() + rotation_max_file_size_str.length(),
                                rotation_max_file_size);

              if ((fec == std::errc::invalid_argument) || (fec == std::errc::result_out_of_range))
              {
                ec = std::make_error_code(fec);
                // TODO:: Handle ec
                continue;
              }
            }
            else if (line.starts_with("    rotation_time_interval"))
            {
              std::string_view const rotation_time_interval_str =
                extract_value_from_line(line, "    rotation_time_interval");
              auto [ptr, fec] =
                std::from_chars(rotation_time_interval_str.data(),
                                rotation_time_interval_str.data() + rotation_time_interval_str.length(),
                                rotation_time_interval);

              if ((fec == std::errc::invalid_argument) || (fec == std::errc::result_out_of_range))
              {
                ec = std::make_error_code(fec);
                // TODO:: Handle ec
                continue;
              }
            }
            else if (line.starts_with("    rotation_daily_at_time"))
            {
              auto const rotation_daily_at_time_str =
                std::string{extract_value_from_line(line, "    rotation_daily_at_time")};

              std::vector<std::string> tokens;
              std::string token;
              std::istringstream token_stream(rotation_daily_at_time_str);

              while (std::getline(token_stream, token, ':'))
              {
                tokens.push_back(token);
              }

              if (tokens.size() != 2)
              {
                // TODO:: HAndle error
                continue;
              }

              for (auto const& parsed_token : tokens)
              {
                if (parsed_token.size() != 2)
                {
                  // TODO:: HAndle error
                  continue;
                }
              }

              rotation_daily_at_time = std::make_pair(std::chrono::hours{std::stoi(tokens[0])},
                                                      std::chrono::minutes{std::stoi(tokens[1])});

              if ((rotation_daily_at_time.first > std::chrono::hours{23}) ||
                  (rotation_daily_at_time.second > std::chrono::minutes{59}))
              {
                // TODO:: HAndle error
                continue;
              }
            }
            else if (line.starts_with("    rotation_max_backup_files"))
            {
              std::string_view const rotation_max_backup_files_str =
                extract_value_from_line(line, "    rotation_max_backup_files");
              auto [ptr, fec] = std::from_chars(
                rotation_max_backup_files_str.data(),
                rotation_max_backup_files_str.data() + rotation_max_backup_files_str.length(),
                rotation_max_backup_files);

              if ((fec == std::errc::invalid_argument) || (fec == std::errc::result_out_of_range))
              {
                ec = std::make_error_code(fec);
                // TODO:: Handle ec
                continue;
              }
            }
            else if (line.starts_with("    output_file_open_mode"))
            {
              output_file_open_mode = get_file_open_mode_enum(
                std::string{extract_value_from_line(line, "    output_file_open_mode")});
            }
            else if (line.starts_with("    rotation_time_frequency"))
            {
              rotation_time_frequency = get_file_rotation_frequency_enum(
                std::string{extract_value_from_line(line, "    rotation_time_frequency")});
            }
            else if (line.starts_with("    output_file_suffix"))
            {
              output_file_suffix = get_file_suffix_enum(
                std::string{extract_value_from_line(line, "    output_file_suffix")});
            }
            else if (line.starts_with("    rotation_overwrite_oldest_files"))
            {
              rotation_overwrite_oldest_files =
                (extract_value_from_line(line, "    rotation_overwrite_oldest_files") == "true");
            }
          }

          ret_val.emplace_back(std::move(name), std::move(log_record_pattern),
                               std::move(timestamp_pattern), timezone, sink_type, output_file_path,
                               rotation_max_file_size, rotation_time_interval, rotation_daily_at_time,
                               rotation_max_backup_files, output_file_open_mode, rotation_time_frequency,
                               output_file_suffix, rotation_overwrite_oldest_files);
        }
      }
    }
  }

  return ret_val;
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

template <typename TBackendOptions>
class ThreadQueueManager
{
public:
  using backend_options_t = TBackendOptions;
  using queue_info_t = QueueInfo<typename backend_options_t::queue_type>;

  ThreadQueueManager(std::filesystem::path run_dir, backend_options_t const& options)
    : _run_dir(std::move(run_dir)), _options(options){};

  [[nodiscard]] std::vector<queue_info_t> const& active_queues() const noexcept
  {
    return _active_queues;
  }

  [[nodiscard]] std::vector<std::pair<uint32_t, uint32_t>> const& discovered_queues() const noexcept
  {
    return _discovered_queues;
  }

  /**
   * @brief Updates the information of active queues based on the ready queues.
   */
  void update_active_queues()
  {
    std::error_code ec;

    for (auto it = std::begin(_active_queues); it != std::end(_active_queues);)
    {
      auto& active_queue = *it;

      if (!active_queue.queue->empty())
      {
        ++it;
        continue;
      }

      // we have the minimum sequence queue already in active_queues and the queue is empty
      // Check if we need to remove an empty queue
      if (std::optional<uint32_t> const next_sequence =
            _find_next_sequence(active_queue.thread_num, active_queue.sequence);
          next_sequence.has_value())
      {
        uint32_t const thread_num = active_queue.thread_num;
        uint32_t const sequence = active_queue.sequence;

        // This means that the producer has moved to a new queue
        // we want to remove it and replace it with the next sequence queue
        it = _active_queues.erase(it);
        if (!backend_options_t::queue_type::remove_shm_files(
              fmtbitlog::format("{}.{}.ext", thread_num, sequence), _run_dir, ec))
        {
          // TODO:: handle error ?
        }

        if (!_insert_to_active_queues(thread_num, *next_sequence))
        {
          ++it;
          continue;
        }
      }
      else if (lock_file(active_queue.lock_file_fd, ec))
      {
        // if we can lock the .lock file it means the producer process is not running anymore
        unlock_file(active_queue.lock_file_fd, ec);

        uint32_t const thread_num = active_queue.thread_num;
        uint32_t const sequence = active_queue.sequence;

        it = _active_queues.erase(it);
        if (!backend_options_t::queue_type::remove_shm_files(
              fmtbitlog::format("{}.{}.ext", thread_num, sequence), _run_dir, ec))
        {
          // TODO:: handle error ?
        }
      }
      else
      {
        ++it;
      }
    }
  }

  /**
   * @brief Discovers and populates a vector with thread queues found in the specified directory.
   *
   * The function looks for ".ready" files, such as "0.0.ready" or "1.0.ready," representing
   * thread number and sequence information. The discovered thread queues are sorted and added to
   * the provided vector.
   *
   * @param ec An output parameter for error codes.
   * @return `true` if the discovery process is successful, `false` otherwise.
   *         If an error occurs, the error code is set in the `ec` parameter.
   */
  [[nodiscard]] bool discover_queues(std::error_code& ec)
  {
    _discovered_queues.clear();

    auto run_dir_it = std::filesystem::directory_iterator(_run_dir, ec);

    if (ec && ec != std::errc::no_such_file_or_directory)
    {
      return false;
    }

    for (auto const& file_entry : run_dir_it)
    {
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
          std::begin(_discovered_queues), std::end(_discovered_queues),
          std::make_pair(thread_num, sequence), [](const auto& lhs, const auto& rhs)
          { return lhs.first < rhs.first || (lhs.first == rhs.first && lhs.second < rhs.second); });

        _discovered_queues.insert(insertion_point, std::make_pair(thread_num, sequence));
      }
    }

    uint32_t last_thread_num = std::numeric_limits<uint32_t>::max();

    for (auto [thread_num, sequence] : _discovered_queues)
    {
      if (thread_num == last_thread_num)
      {
        // skip future sequences
        continue;
      }

      last_thread_num = thread_num;

      // See if we have that thread num in our active_queues. We still need to do a find here
      // as for example we can have e.g. active thread_num 0,1,2 queues. But when thread num 1
      // finishes we will have 0,2 in our vector, therefore just using the index of the vector
      // for the thread num is not enough
      auto current_active_queue_it = std::lower_bound(
        std::begin(_active_queues), std::end(_active_queues), thread_num,
        [](queue_info_t const& lhs, uint32_t value) { return lhs.thread_num < value; });

      if ((current_active_queue_it == std::end(_active_queues)) ||
          (current_active_queue_it->thread_num != thread_num))
      {
        // Queue doesn't exist so we create it
        if (!_insert_to_active_queues(thread_num, sequence))
        {
          continue;
        }
      }
    }

    return true;
  }

protected:
  /**
   * @brief Creates and inserts a queue into the active_queues vector.
   *
   * @param thread_num The thread number.
   * @param sequence The sequence number.
   * @return True if successful, false otherwise.
   */
  [[nodiscard]] bool _insert_to_active_queues(uint32_t thread_num, uint32_t sequence)
  {
    std::filesystem::path const queue_path = _run_dir / fmtbitlog::format("{}.{}.lock", thread_num, sequence);
    queue_info_t queue_info{thread_num, sequence, queue_path};

    if (queue_info.lock_file_fd == -1)
    {
      // failed to open the lock file
      return false;
    }

    std::error_code ec;

    if (!queue_info.queue->open(queue_path, _options.memory_page_size, ec))
    {
      return false;
    }

    auto insertion_point =
      std::lower_bound(std::begin(_active_queues), std::end(_active_queues), thread_num,
                       [](auto const& lhs, auto const& rhs) { return lhs.thread_num < rhs; });

    _active_queues.insert(insertion_point, std::move(queue_info));

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
   */
  [[nodiscard]] std::optional<uint32_t> _find_next_sequence(uint32_t thread_num, uint32_t sequence)
  {
    auto it = std::lower_bound(std::cbegin(_discovered_queues), std::cend(_discovered_queues),
                               std::make_pair(thread_num, sequence), [](auto const& lhs, auto const& rhs)
                               { return lhs.first == rhs.first && lhs.second < rhs.second; });

    while (it != std::cend(_discovered_queues) && it->first == thread_num)
    {
      if (it->second > sequence)
      {
        return it->second;
      }
      ++it;
    }

    return std::nullopt;
  }

private:
  std::vector<queue_info_t> _active_queues;
  std::vector<std::pair<uint32_t, uint32_t>> _discovered_queues;
  std::filesystem::path _run_dir;
  backend_options_t const& _options;
};

class LoggerContext
{
public:
  LoggerContext(std::string logger_name, PatternFormatter* pattern_formater, SinkBase* sink)
    : _logger_name(std::move(logger_name)), _pattern_formatter(pattern_formater), _sink(sink)
  {
  }

  [[nodiscard]] std::string const& logger_name() const noexcept { return _logger_name; }
  [[nodiscard]] PatternFormatter* pattern_formatter() const noexcept { return _pattern_formatter; }
  [[nodiscard]] SinkBase* sink() const noexcept { return _sink; }

private:
  std::string _logger_name;
  PatternFormatter* _pattern_formatter;
  SinkBase* _sink;
};

/**
 * @brief Application context class template.
 *
 * Manages the application state, including thread queues, logging metadata, and options.
 */
template <typename TBackendOptions>
class ApplicationContext
{
public:
  using backend_options_t = TBackendOptions;

  /**
   * @brief Constructor for ApplicationContext.
   *
   * @param run_dir The path to the application's run directory.
   * @param options Backend options for the application.
   */
  ApplicationContext(std::filesystem::path const& run_dir, backend_options_t const& options)
    : _thread_queue_manager(run_dir, options),
      _run_dir(run_dir.string()),
      _application_id(run_dir.parent_path().stem()),
      _start_ts(run_dir.stem()),
      _options(options)
  {
  }

  /**
   * @brief Initializes the application context, reading log statement metadata and process ID.
   *
   * In case of an error during initialization, a runtime_error is thrown.
   */
  [[nodiscard]] bool init(std::error_code& ec)
  {
    auto [log_statement_metadata, process_id] = detail::read_log_statement_metadata_file(_run_dir, ec);

    if (ec)
    {
      return false;
    }

    _log_statement_metadata = std::move(log_statement_metadata);
    _process_id = std::move(process_id);

    auto run_dir = std::filesystem::path{_run_dir} / APP_RUNNING_FILENAME;
    _running_file_fd = ::open(run_dir.c_str(), O_RDONLY);

    if (_running_file_fd == -1)
    {
      ec = std::make_error_code(static_cast<std::errc>(errno));
      return false;
    }

    return true;
  }

  /**
   * @brief Processes thread queues and logs messages.
   *
   * Discovers and updates active queues and logs messages from those queues.
   */
  void process_queues_and_log()
  {
    std::error_code ec;

    if (!_thread_queue_manager.discover_queues(ec)) [[unlikely]]
    {
      // TODO:: Handle error
    }

    _thread_queue_manager.update_active_queues();

    for (auto& queue_info : _thread_queue_manager.active_queues())
    {
      uint8_t const* read_buffer = queue_info.queue->prepare_read();

      size_t const queue_capacity = queue_info.queue->capacity();
      size_t total_bytes_read{0};

      // read max of one full queue otherwise we can get stuck here forever if the
      // producer keeps producing
      while ((total_bytes_read < queue_capacity) && read_buffer)
      {
        uint8_t const* read_buffer_begin = read_buffer;

        // Read a log message from the buffer
        // TODO:: Use custom memcpy ?
        uint64_t timestamp;
        std::memcpy(&timestamp, read_buffer, sizeof(timestamp));
        read_buffer += sizeof(timestamp);

        uint32_t metadata_id;
        std::memcpy(&metadata_id, read_buffer, sizeof(metadata_id));
        read_buffer += sizeof(metadata_id);

        uint32_t logger_id;
        std::memcpy(&logger_id, read_buffer, sizeof(logger_id));
        read_buffer += sizeof(logger_id);

        LogStatementMetadata const* lsm = get_log_statement_metadata(metadata_id);

        if (!lsm) [[unlikely]]
        {
          // TODO:: Handle Error
          std::abort();
        }

        _log_message_fmt_args_store.clear();
        bitlog::detail::decode(read_buffer, lsm->type_descriptors(), _log_message_fmt_args_store);

        _log_message.clear();
        fmtbitlog::vformat_to(std::back_inserter(_log_message), lsm->message_format(),
                              fmtbitlog::basic_format_args(_log_message_fmt_args_store.data(),
                                                           _log_message_fmt_args_store.size()));

        LoggerContext* logger_context = get_logger_context(logger_id);

        std::string_view const formatted_statement = logger_context->pattern_formatter()->format(
          *lsm, timestamp, queue_info.queue->thread_id(), queue_info.queue->thread_name(), _process_id,
          logger_context->logger_name(), std::string_view{_log_message.data(), _log_message.size()});

        logger_context->sink()->write(formatted_statement, *lsm);

        // finished reading
        assert(read_buffer >= read_buffer_begin);
        size_t const read_size = read_buffer - read_buffer_begin;
        queue_info.queue->finish_read(read_size);
        total_bytes_read += read_size;

        // Read next message
        read_buffer = queue_info.queue->prepare_read();
      }

      if (total_bytes_read != 0)
      {
        // we read something from the queue, we commit all the reads together at the end
        queue_info.queue->commit_read();
      }
    }

    for (auto& sink : _sinks)
    {
      sink->flush();
    }
  }

  /**
   * @brief Checks if the application is running.
   *
   * @return True if the application is running; otherwise, false.
   */
  [[nodiscard]] bool is_running()
  {
    if (_running_file_fd == -1)
    {
      // probably not initialised yet
      return true;
    }

    for (auto& queue_info : _thread_queue_manager.active_queues())
    {
      if (!queue_info.queue->empty())
      {
        // At least one queue not empty
        return true;
      }
    }

    // all active queues are empty, check for any queues that we haven't popped yet
    std::error_code ec;

    if (!_thread_queue_manager.discover_queues(ec)) [[unlikely]]
    {
      // TODO:: Handle error
    }

    _thread_queue_manager.update_active_queues();

    for (auto& queue_info : _thread_queue_manager.active_queues())
    {
      if (!queue_info.queue->empty())
      {
        // At least one queue not empty
        return true;
      }
    }

    // all active queues are empty and nothing new discovered
    // Check if the application is still running
    if (detail::lock_file(_running_file_fd, ec))
    {
      // if we can lock the file then the application is not running
      detail::unlock_file(_running_file_fd, ec);
      return false;
    }

    return true;
  }

  /**
   * @brief Gets the application ID.
   *
   * @return The application ID.
   */
  [[nodiscard]] std::string const& application_id() const noexcept { return _application_id; }

  /**
   * @brief Gets the start timestamp.
   *
   * @return The start timestamp.
   */
  [[nodiscard]] std::string const& start_ts() const noexcept { return _start_ts; }

  /**
   * @brief Gets the run directory.
   *
   * @return The run directory.
   */
  [[nodiscard]] std::string const& run_dir() const noexcept { return _run_dir; }

private:
  /**
   * @brief Gets logger context by ID.
   *
   * If the logger ID is not found, attempts to reload metadata from file and create the missing logger contexts
   *
   * @param id Logger ID.
   * @return Pointer to the logger context if found; otherwise, nullptr.
   */
  [[nodiscard]] LoggerContext* get_logger_context(uint32_t id)
  {
    if (id >= _logger_contexts.size())
    {
      // We do not have this logger, we want to try to reload the file
      std::error_code ec;
      std::vector<LoggerMetadata> const logger_metadata = detail::read_logger_metadata_file(_run_dir, ec);

      if (ec) [[unlikely]]
      {
        return nullptr;
      }

      // Process only the logger contexts we do not already have
      size_t const logger_contexts_size = _logger_contexts.size();
      assert(logger_metadata.size() >= logger_contexts_size);
      for (size_t i = logger_contexts_size; i < logger_metadata.size(); ++i)
      {
        LoggerMetadata const& lm = logger_metadata[i];

        PatternFormatter* pattern_formatter{nullptr};
        SinkBase* sink{nullptr};

        // Find or create the pattern formatter
        auto pattern_formatter_it =
          std::find_if(std::begin(_pattern_formatters), std::end(_pattern_formatters),
                       [&lm](std::unique_ptr<PatternFormatter> const& elem)
                       {
                         return elem->format_pattern() == lm.log_record_pattern() &&
                           elem->timestamp_formatter().format_pattern() == lm.timestamp_pattern() &&
                           elem->timestamp_formatter().timezone() == lm.timezone();
                       });

        if (pattern_formatter_it != std::end(_pattern_formatters))
        {
          pattern_formatter = pattern_formatter_it->get();
        }
        else
        {
          _pattern_formatters.push_back(std::make_unique<PatternFormatter>(
            lm.log_record_pattern(), lm.timestamp_pattern(), lm.timezone()));
          pattern_formatter = _pattern_formatters.back().get();
        }

        // Find or create the sink
        auto sink_it = std::find_if(std::begin(_sinks), std::end(_sinks),
                                    [&lm](std::unique_ptr<SinkBase> const& elem)
                                    {
                                      if (lm.sink_type() == SinkType::Console)
                                      {
                                        return elem->type() == SinkType::Console;
                                      }
                                      else if (lm.sink_type() == SinkType::File)
                                      {
                                        return elem->type() == SinkType::File &&
                                          elem->name() == lm.output_file_path();
                                      }
                                      else
                                      {
                                        // TODO:: unreachable
                                        std::abort();
                                        // unreachable
                                        return true;
                                      }
                                    });

        if (sink_it != std::end(_sinks))
        {
          sink = sink_it->get();
        }
        else
        {
          // Create new sink
          if (lm.sink_type() == SinkType::Console)
          {
            _sinks.push_back(std::make_unique<ConsoleSink>());
            sink = _sinks.back().get();
          }
          else if (lm.sink_type() == SinkType::File)
          {
            auto file_sink = std::make_unique<FileSink>();
            bool const ok = file_sink->init(lm.output_file_path(), lm.output_file_open_mode());

            if (!ok)
            {
              // TODO:: Handle error
              continue;
            }

            _sinks.push_back(std::move(file_sink));
            sink = _sinks.back().get();
          }
        }

        _logger_contexts.emplace_back(lm.logger_name(), pattern_formatter, sink);
      }
    }

    if (id >= _logger_contexts.size()) [[unlikely]]
    {
      return nullptr;
    }

    return &_logger_contexts[id];
  }

  /**
   * @brief Gets log statement metadata by ID.
   *
   * Log Statement Metadata are loaded once on the first statement we try to format
   *
   * @param id Log Statement ID.
   * @return Pointer to the log statement metadata if found; otherwise, nullptr.
   */
  [[nodiscard]] LogStatementMetadata const* get_log_statement_metadata(uint32_t id)
  {
    if (id >= _log_statement_metadata.size()) [[unlikely]]
    {
      return nullptr;
    }

    return &_log_statement_metadata[id];
  }

private:
  std::vector<LogStatementMetadata> _log_statement_metadata;
  ThreadQueueManager<backend_options_t> _thread_queue_manager;
  std::vector<fmtbitlog::basic_format_arg<fmtbitlog::format_context>> _log_message_fmt_args_store;
  fmtbitlog::basic_memory_buffer<char, 1024> _log_message;
  std::vector<std::unique_ptr<PatternFormatter>> _pattern_formatters;
  std::vector<std::unique_ptr<SinkBase>> _sinks;
  std::vector<LoggerContext> _logger_contexts;
  std::string _run_dir;
  std::string _application_id;
  std::string _start_ts;
  std::string _process_id;
  backend_options_t const& _options;
  int _running_file_fd{-1};
};
} // namespace bitlog::detail