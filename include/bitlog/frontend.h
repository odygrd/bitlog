#pragma once

#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>

#include "bitlog/common/common.h"
#include "bitlog/frontend/frontend_impl.h"

namespace bitlog
{

namespace detail
{
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
} // namespace detail

enum class QueuePolicyOption
{
  BoundedDropping,
  BoundedBlocking,
  UnboundedNoLimit
};

/**
 * @brief Structure representing configuration options for the frontend.
 *
 * @tparam QueuePolicy The policy of queue to be used.
 * @tparam QueueType The type of queue to be used.
 * @tparam UseCustomMemcpyX86 Boolean indicating whether to use a custom memcpy implementation for x86.
 */
template <QueuePolicyOption QueuePolicy, QueueTypeOption QueueType, bool UseCustomMemcpyX86>
struct FrontendOptions
{
  static constexpr QueuePolicyOption queue_policy = QueuePolicy;
  static constexpr bool use_custom_memcpy_x86 = UseCustomMemcpyX86;
  using queue_type =
    std::conditional_t<QueueType == QueueTypeOption::Default, bitlog::detail::BoundedQueue, bitlog::detail::BoundedQueueX86>;

  uint64_t queue_capacity_bytes = 131'072u;
  MemoryPageSize memory_page_size = MemoryPageSize::RegularPage;
};

class LoggerOptions
{
public:
  /**
   * @brief Sets the pattern for log record formatting using placeholders.
   *
   * The following attribute names can be used with the corresponding placeholder in a %-style format string:
   * - %(creation_time): Human-readable time when the LogRecord was created
   * - %(full_source_path): Source file where the logging call was issued
   * - %(source_file): Full source file where the logging call was issued
   * - %(caller_function): Name of function containing the logging call
   * - %(log_level): Text logging level for the log message
   * - %(log_level_id): Single-letter id
   * - %(source_line): Source line number where the logging call was issued
   * - %(logger): Name of the logger used to log the call
   * - %(log_message): The logged message
   * - %(thread_id): Thread ID
   * - %(thread_name): Thread Name if set
   * - %(process_id): Process ID
   *
   * @param value The log record pattern to set.
   */
  void log_record_pattern(std::string value) noexcept { _log_record_pattern = std::move(value); }

  /**
   * @brief Sets the format pattern for timestamp generation.
   *
   * The format pattern uses the same specifiers as strftime() with additional specifiers:
   * - %Qms: Milliseconds
   * - %Qus: Microseconds
   * - %Qns: Nanoseconds
   * @note %Qms, %Qus, %Qns specifiers are mutually exclusive.
   * Example: given "%I:%M.%Qms%p", the output would be "03:21.343PM".
   *
   * @param value The timestamp pattern to set.
   */
  void timestamp_pattern(std::string value) noexcept { _timestamp_pattern = std::move(value); }

  /**
   * @brief Sets the time zone for log records.
   * @param value The time zone to set.
   */
  void timezone(Timezone value) noexcept { _timezone = value; }

  [[nodiscard]] std::string const& log_record_pattern() const noexcept
  {
    return _log_record_pattern;
  }
  [[nodiscard]] std::string const& timestamp_pattern() const noexcept { return _timestamp_pattern; }
  [[nodiscard]] Timezone timezone() const noexcept { return _timezone; }

private:
  std::string _log_record_pattern =
    "%(creation_time) [%(thread_id)] %(source_location:<28) LOG_%(log_level:<9) "
    "%(logger:<12) %(log_message)";
  std::string _timestamp_pattern = "%H:%M:%S.%Qns";
  Timezone _timezone = Timezone::LocalTime;
};

/** Forward declaration **/
template <typename TFrontendOptions>
class FrontendManager;

class SinkOptions
{
public:
  /**
   * @brief Sets the append type for the output file name.
   *
   * Possible append types are: StartDate, StartDateTime, or None.
   * When set, the file name will be appended with the start date or date and time
   * timestamp of when the process started.
   *
   * Example:
   * - application.log -> application_20230101.log (StartDate)
   * - application.log -> application_20230101_121020.log (StartDateTime)
   *
   * @param value The append type to set. Valid options are Date, DateAndTime, or None.
   */
  void output_file_suffix(FileSuffix value) { _output_file_suffix = value; }

  /**
   * @brief Sets the open mode for the output log file.
   * Valid options for the open mode are 'a' (append) or 'w' (write). Default is 'a'.
   * @param value The open mode for the file.
   */
  void output_file_open_mode(FileOpenMode value) { _output_file_open_mode = value; }

  /**
   * @brief Sets the maximum file size in bytes for log file rotation.
   * Enabling this option will enable file rotation based on file size. Default is disabled.
   * @param value The maximum file size in bytes per file.
   */
  void rotation_max_size(uint64_t value) { _rotation_max_file_size = value; }

  /**
   * @brief Sets the frequency and interval for log file rotation.
   * Enabling this option will enable file rotation based on a specified frequency and interval.
   * Default is disabled. Valid values for the frequency are 'M' for minutes and 'H' for hours.
   * @param frequency The frequency of file rotation to set.
   * @param interval The rotation interval to set.
   */
  void rotation_schedule(FileRotationFrequency frequency, uint32_t interval)
  {
    _rotation_time_frequency = frequency;
    _rotation_time_interval = interval;
    _rotation_daily_at_time.clear();
  }

  /**
   * @brief Sets the time of day for daily log file rotation.
   * When set, the rotation frequency is automatically set to 'daily'.
   * Default is disabled.
   * @param at_time The time of day to perform the log file rotation. Format: "HH:MM".
   */
  void rotation_daily_at_time(std::string const& value)
  {
    _rotation_time_frequency = FileRotationFrequency::Daily;
    _rotation_time_interval = 0;
    _rotation_daily_at_time = value;
  }

  /**
   * @brief Sets the maximum number of log files to keep.
   * By default, there is no limit on the number of log files.
   * @param value The maximum number of log files to set.
   */
  void rotation_max_backup_files(uint32_t value) { _rotation_max_backup_files = value; }

  /**
   * @brief Sets whether the oldest rolled logs should be overwritten when the maximum backup count
   * is reached. If set to false, the oldest logs will not be overwritten, and log file rotation
   * will stop. Default is true.
   * @param value True to overwrite the oldest logs, false otherwise.
   */
  void rotation_overwrite_oldest_files(uint32_t value) { _rotation_overwrite_oldest_files = value; }

  [[nodiscard]] FileSuffix output_file_suffix() const noexcept { return _output_file_suffix; }
  [[nodiscard]] FileOpenMode output_file_open_mode() const noexcept
  {
    return _output_file_open_mode;
  }
  [[nodiscard]] uint64_t rotation_max_file_size() const noexcept { return _rotation_max_file_size; }
  [[nodiscard]] FileRotationFrequency rotation_time_frequency() const noexcept
  {
    return _rotation_time_frequency;
  }
  [[nodiscard]] uint64_t rotation_time_interval() const noexcept { return _rotation_time_interval; }
  [[nodiscard]] std::string const& rotation_daily_at_time() const noexcept
  {
    return _rotation_daily_at_time;
  }
  [[nodiscard]] uint32_t rotation_max_backup_files() const noexcept
  {
    return _rotation_max_backup_files;
  }
  [[nodiscard]] bool rotation_overwrite_oldest_files() const noexcept
  {
    return _rotation_overwrite_oldest_files;
  }

private:
  std::string _rotation_daily_at_time;
  uint64_t _rotation_max_file_size = 0;
  uint64_t _rotation_time_interval = 0;
  uint32_t _rotation_max_backup_files = std::numeric_limits<uint32_t>::max();
  FileOpenMode _output_file_open_mode = FileOpenMode::Write;
  FileRotationFrequency _rotation_time_frequency = FileRotationFrequency::Disabled;
  FileSuffix _output_file_suffix = FileSuffix::None;
  bool _rotation_overwrite_oldest_files = true;
};

class Sink
{
public:
  [[nodiscard]] std::string const& output_file_path() const noexcept { return _output_file_path; }
  [[nodiscard]] SinkOptions const& options() const noexcept { return _options; }
  [[nodiscard]] SinkType sink_type() const noexcept { return _sink_type; }

private:
  template <typename T>
  friend class FrontendManager;

  Sink(std::string output_file_path, SinkOptions options, SinkType sink_type)
    : _output_file_path(std::move(output_file_path)), _options(std::move(options)), _sink_type(sink_type)
  {
  }

  std::string _output_file_path;
  SinkOptions _options;
  SinkType _sink_type;
};

/**
 * @brief Logger class
 */
template <typename TFrontendOptions>
class Logger : public detail::UniqueId<Logger<TFrontendOptions>>
{
public:
  using frontend_options_t = TFrontendOptions;

  /**
   * Move ctor
   * @param other
   */
  Logger(Logger&& other) noexcept
    : detail::UniqueId<Logger<TFrontendOptions>>(std::move(other)),
      _name(std::move(other._name)),
      _run_dir(other._run_dir),
      _options(other._options),
      _log_level(other.log_level())
  {
  }

  /**
   * @brief Gets the log level of the logger.
   * @return The log level.
   */
  [[nodiscard]] LogLevel log_level() const noexcept
  {
    return _log_level.load(std::memory_order_acquire);
  }

  /**
   * @brief Sets the log level of the logger.
   * @param level The new log level.
   */
  void log_level(LogLevel level) noexcept { _log_level.store(level, std::memory_order_release); }

  /**
   * @brief Gets the name of the logger.
   * @return The logger's name.
   */
  [[nodiscard]] std::string const& name() const noexcept { return _name; }

  /**
   * @brief Checks if a log statement with the given level can be logged by this logger.
   * @param log_statement_level The log level of the log statement.
   * @return True if the log statement can be logged, false otherwise.
   */
  [[nodiscard]] bool should_log(LogLevel log_statement_level) const noexcept
  {
    return log_statement_level >= log_level();
  }

  /**
   * @brief Checks if a log statement with a specific level can be logged by this logger.
   * @tparam LogStatementLevel The log level of the log statement.
   * @return True if the log statement can be logged, false otherwise.
   */
  template <LogLevel LogStatementLevel>
  [[nodiscard]] bool should_log() const noexcept
  {
    return LogStatementLevel >= log_level();
  }

  /**
   * @brief Logs a message with specified parameters.
   *
   * @tparam File The file name where the log statement originates.
   * @tparam Function The function name where the log statement originates.
   * @tparam Line The line number where the log statement originates.
   * @tparam Level The log level of the statement.
   * @tparam LogFormat The log format string.
   * @tparam Args Variadic template for log message arguments.
   * @param args Arguments for the log message.
   */
  template <detail::StringLiteral File, detail::StringLiteral Function, uint32_t Line,
            LogLevel Level, detail::StringLiteral LogFormat, typename... Args>
  void log(Args const&... args) const noexcept
  {
    // Get the queue associated with this thread
    detail::ThreadLocalQueue<frontend_options_t>& tl_queue =
      detail::get_thread_local_queue<frontend_options_t>(_run_dir, _options);

    if (!tl_queue.queue().has_value()) [[unlikely]]
    {
      // This can only happen if the queue creation has failed
      // TODO:: Handle error ?
      return;
    }

    uint32_t c_style_string_lengths_array[(std::max)(detail::count_c_style_strings<Args...>(), static_cast<uint32_t>(1))];

    // Also reserve space for the timestamp the metadata id and the logger id
    constexpr auto additional_space =
      static_cast<uint32_t>(sizeof(uint64_t) + sizeof(uint32_t) + sizeof(uint32_t));

    uint32_t const total_args_size = additional_space +
      detail::calculate_args_size_and_populate_string_lengths(c_style_string_lengths_array, args...);

    uint8_t* write_buffer = tl_queue.queue()->prepare_write(total_args_size);

#ifndef NDEBUG
    uint8_t* start = write_buffer;
#endif

    if (write_buffer)
    {
      // TODO:: timestamp types rdtsc
      uint64_t const timestamp = std::chrono::system_clock::now().time_since_epoch().count();

      detail::encode<frontend_options_t::use_custom_memcpy_x86>(
        write_buffer, c_style_string_lengths_array, timestamp,
        detail::marco_metadata_node<File, Function, Line, Level, LogFormat, Args...>.id, this->id, args...);

#ifndef NDEBUG
      assert(write_buffer - start == total_args_size);
#endif

      tl_queue.queue()->finish_write(total_args_size);
      tl_queue.queue()->commit_write();
    }
    else
    {
      // TODO Queue is full
      // TODO call tl_queue.reset() to allocate a new queue then tl_queue.queue() again
    }
  }

private:
  template <typename T>
  friend class FrontendManager;

  /**
   * @brief Private constructor for Logger.
   *
   * @param name Name of the logger.
   * @param run_dir Current running dir
   * @param options Frontend options for the logger.
   */
  Logger(std::string name, std::filesystem::path const& run_dir, frontend_options_t const& options)
    : _name(std::move(name)), _run_dir(run_dir), _options(options)
  {
  }

private:
  std::string _name;
  std::filesystem::path const& _run_dir;
  frontend_options_t const& _options;
  std::atomic<LogLevel> _log_level{LogLevel::Info};
};

/**
 * @brief Manager class for the frontend
 */
template <typename TFrontendOptions>
class FrontendManager
{
public:
  using frontend_options_t = TFrontendOptions;
  using logger_t = Logger<frontend_options_t>;

  /**
   * @brief Explicit constructor for FrontendManager.
   * @param application_id Identifier for the application.
   * @param options Options for the frontend.
   * @param base_dir Base directory path.
   */
  explicit FrontendManager(std::string_view application_id, frontend_options_t options = frontend_options_t{},
                           std::string_view base_dir = std::string_view{})
    : _options(std::move(options))
  {
    std::optional<std::filesystem::path> const run_dir = detail::create_run_directory(application_id, base_dir);

    if (!run_dir.has_value())
    {
      std::abort();
    }

    _run_dir = *run_dir;

    if (!_app_running_file.init_writer(_run_dir / detail::APP_RUNNING_FILENAME))
    {
      std::abort();
    }

    if (!detail::create_log_statements_metadata_file(_run_dir))
    {
      std::abort();
    }

    if (!detail::create_logger_metadata_file(_run_dir))
    {
      std::abort();
    }

    std::filesystem::path app_ready_file_path = _run_dir / detail::APP_READY_FILENAME;
    int app_ready_file_id = ::open(app_ready_file_path.c_str(), O_CREAT | O_RDWR | O_EXCL, 0660);

    if (app_ready_file_id == -1)
    {
      std::abort();
    }

    ::close(app_ready_file_id);
  }

  /**
   * @brief Get the base directory path.
   * @return Base directory path.
   */
  [[nodiscard]] std::filesystem::path base_dir() const noexcept
  {
    return application_dir().parent_path();
  }

  /**
   * @brief Get the application directory path.
   * @return Application directory path.
   */
  [[nodiscard]] std::filesystem::path application_dir() const noexcept
  {
    return _run_dir.parent_path();
  }

  /**
   * @brief Get the run directory path.
   * @return Run directory path.
   */
  [[nodiscard]] std::filesystem::path const& run_dir() const noexcept { return _run_dir; }

  /**
   * @brief Get the options for frontend
   * @return frontend options.
   */
  [[nodiscard]] frontend_options_t const& options() const noexcept { return _options; }

  /**
   * @brief Retrieves or creates a file sink with the specified name.
   *
   * This function retrieves an existing file sink associated with the provided
   * output file path or creates a new one if it does not exist. The file sink is
   * identified by the output file path
   *
   * @param output_file_path The output file path
   * @param sink_options     The options for configuring the file sink.
   * @return A pointer to the sink with the given output file path.
   */
  [[nodiscard]] Sink const* create_file_sink(std::string const& output_file_path, SinkOptions const& sink_options)
  {
    std::lock_guard<std::mutex> lock_guard{_lock};

    // Check if the sink already exists in the registry
    if (auto search = _sink_registry.find(output_file_path); search != std::end(_sink_registry))
    {
      return &(search->second);
    }

    // Sink doesn't exist, create a new one
    auto [it, emplaced] =
      _sink_registry.emplace(output_file_path, Sink{output_file_path, sink_options, SinkType::File});
    assert(emplaced);

    return &(it->second);
  }

  /**
   * @brief Retrieves a file sink associated with the specified output file path.
   *
   * This function searches for an existing file sink in the registry based on the
   * provided output file path. If found, a pointer to the corresponding file sink
   * is returned. If no file sink is found, a null pointer is returned.
   *
   * @param output_file_path The output file path associated with the file sink.
   * @return A pointer to the file sink with the given output file path.
   *         If no matching file sink is found, returns nullptr.
   */
  [[nodiscard]] Sink const* find_file_sink(std::string const& output_file_path) const
  {
    std::lock_guard<std::mutex> lock_guard{_lock};

    // Check if the logger already exists in the registry
    if (auto search = _sink_registry.find(output_file_path); search != std::end(_sink_registry))
    {
      return &(search->second);
    }

    return nullptr;
  }

  [[nodiscard]] Sink const* console_sink() const noexcept { return &_console_sink; }

  /**
   * @brief Retrieves or creates a logger with the specified name.
   *
   * @param name The name of the logger.
   * @return A pointer to the logger with the given name.
   *         If creation fails while appending to logger-metadata returns nullptr.
   */
  [[nodiscard]] logger_t* create_logger(std::string const& name, Sink const& sink, LoggerOptions const& options)
  {
    std::lock_guard<std::mutex> lock_guard{_lock};

    // Check if the logger already exists in the registry
    if (auto search = _logger_registry.find(name); search != std::end(_logger_registry))
    {
      return &(search->second);
    }

    auto [it, emplaced] = _logger_registry.emplace(name, logger_t{name, _run_dir, _options});
    assert(emplaced);

    // Append logger metadata to the logger-metadata.yaml file
    bool res{false};

    if (sink.sink_type() == SinkType::Console)
    {
      res = detail::append_logger_metadata_file(
        _run_dir, it->second.id, it->second.name(), options.log_record_pattern(),
        options.timestamp_pattern(), options.timezone(), sink.sink_type());
    }
    else if (sink.sink_type() == SinkType::File)
    {
      res = detail::append_logger_metadata_file(
        _run_dir, it->second.id, it->second.name(), options.log_record_pattern(),
        options.timestamp_pattern(), options.timezone(), sink.sink_type(), sink.output_file_path(),
        sink.options().rotation_max_file_size(), sink.options().rotation_time_interval(),
        sink.options().rotation_daily_at_time(), sink.options().rotation_max_backup_files(),
        sink.options().output_file_open_mode(), sink.options().rotation_time_frequency(),
        sink.options().output_file_suffix(), sink.options().rotation_overwrite_oldest_files());
    }

    if (!res) [[unlikely]]
    {
      _logger_registry.erase(it);
      return nullptr;
    }

    return &(it->second);
  }

  /**
   * @brief Retrieves a logger with the specified name.
   *
   * @param name The name of the logger.
   * @return A pointer to the logger with the given name.
   */
  [[nodiscard]] logger_t* find_logger(std::string const& name) const
  {
    std::lock_guard<std::mutex> lock_guard{_lock};

    // Check if the logger already exists in the registry
    if (auto search = _logger_registry.find(name); search != std::end(_logger_registry))
    {
      return &(search->second);
    }

    return nullptr;
  }

private:
  detail::MetadataFile _app_running_file;
  mutable std::mutex _lock;
  std::unordered_map<std::string, logger_t> _logger_registry; ///< key is the logger name, value is a pointer to the logger. TODO:: ?
  std::unordered_map<std::string, Sink> _sink_registry;
  Sink _console_sink{std::string{}, SinkOptions{}, SinkType::Console};
  std::filesystem::path _run_dir;
  frontend_options_t _options;
};

/**
 * @brief Singleton class for the frontend, providing initialization and access to FrontendManager.
 */
template <typename TFrontendOptions>
class Frontend
{
public:
  using frontend_options_t = TFrontendOptions;
  using logger_t = Logger<frontend_options_t>;

  /**
   * @brief Initialize the Frontend singleton.
   *
   * @param application_id Identifier for the application.
   * @param options configuration options for the frontend.
   * @param base_dir Base directory path.
   * @return True if initialization is successful, false otherwise.
   */
  static bool init(std::string_view application_id, frontend_options_t options = frontend_options_t{},
                   std::string_view base_dir = std::string_view{})
  {
    if (!detail::initialise_frontend_once())
    {
      return false;
    }

    // Set up the singleton
    _instance.reset(new Frontend<frontend_options_t>(application_id, std::move(options), base_dir));

    return true;
  }

  /**
   * @brief Get the instance of the Frontend singleton.
   * @return Reference to the Frontend singleton instance.
   */
  [[nodiscard]] static Frontend<frontend_options_t>& instance() noexcept
  {
    assert(_instance);
    return *_instance;
  }

  /**
   * @brief Get the base directory path.
   * @return Base directory path.
   */
  [[nodiscard]] std::filesystem::path base_dir() const noexcept
  {
    return _frontend_manager.base_dir();
  }

  /**
   * @brief Get the application directory path.
   * @return Application directory path.
   */
  [[nodiscard]] std::filesystem::path application_dir() const noexcept
  {
    return _frontend_manager.application_dir();
  }

  /**
   * @brief Get the run directory path.
   * @return Run directory path.
   */
  [[nodiscard]] std::filesystem::path const& run_dir() const noexcept
  {
    return _frontend_manager.run_dir();
  }

  /**
   * @brief Get the configuration options for the frontend.
   * @return Frontend configuration options.
   */
  [[nodiscard]] frontend_options_t const& options() const noexcept
  {
    return _frontend_manager.options();
  }

  /**
   * @brief Retrieves or creates a file sink with the specified name.
   *
   * This function retrieves an existing file sink associated with the provided
   * output file path or creates a new one if it does not exist. The file sink is
   * identified by the output file path
   *
   * @param output_file_path The output file path
   * @param sink_options     The options for configuring the file sink.
   * @return A pointer to the sink with the given output file path.
   */
  [[nodiscard]] Sink const* create_file_sink(std::string const& output_file_path,
                                             SinkOptions const& sink_options = SinkOptions{})
  {
    return _frontend_manager.create_file_sink(output_file_path, sink_options);
  }

  /**
   * @brief Retrieves a file sink associated with the specified output file path.
   *
   * This function searches for an existing file sink in the registry based on the
   * provided output file path. If found, a pointer to the corresponding file sink
   * is returned. If no file sink is found, a null pointer is returned.
   *
   * @param output_file_path The output file path associated with the file sink.
   * @return A pointer to the file sink with the given output file path.
   *         If no matching file sink is found, returns nullptr.
   */
  [[nodiscard]] Sink const* find_file_sink(std::string const& output_file_path) const
  {
    return _frontend_manager.find_file_sink(output_file_path);
  }

  [[nodiscard]] Sink const* console_sink() const noexcept
  {
    return _frontend_manager.console_sink();
  }

  /**
   * @brief Creates a logger with the specified name.
   *
   * @param name The name of the logger.
   * @param options Logger options
   * @return A pointer to the logger with the given name.
   *         If creation fails while appending to logger-metadata returns nullptr.
   */
  [[nodiscard]] logger_t* create_logger(std::string const& name, Sink const* sink,
                                        LoggerOptions const& options = LoggerOptions{})
  {
    if (!sink) [[unlikely]]
    {
      std::abort();
    }

    return _frontend_manager.create_logger(name, *sink, options);
  }

  /**
   * @brief Retrieves an existing logger with the specified name.
   *
   * @param name The name of the logger.
   * @param options Logger options
   * @return A pointer to the logger with the given name.
   *         If creation fails while appending to logger-metadata returns nullptr.
   */

  [[nodiscard]] logger_t* find_logger(std::string const& name)
  {
    return _frontend_manager.create_logger(name);
  }

private:
  Frontend(std::string_view application_id, frontend_options_t options, std::string_view base_dir)
    : _frontend_manager(application_id, std::move(options), base_dir){};
  static inline std::unique_ptr<Frontend<frontend_options_t>> _instance;
  FrontendManager<frontend_options_t> _frontend_manager;
};
} // namespace bitlog