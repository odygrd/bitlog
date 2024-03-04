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

/** Forward declaration **/
template <typename TFrontendOptions>
class FrontendManager;

/**
 * @brief Base class for loggers with a unique identifier.
 */
class LoggerBase : public detail::UniqueId<LoggerBase>
{
public:
  /**
   * @brief Constructs a LoggerBase with a given name.
   * @param name The name of the logger.
   * @param run_dir Current running dir
   */
  LoggerBase(std::filesystem::path const& run_dir, std::string_view name)
    : _run_dir(run_dir), _name(name){};

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
  void log_level(LogLevel level) { _log_level.store(level, std::memory_order_release); }

  /**
   * @brief Gets the name of the logger.
   * @return The logger's name.
   */
  [[nodiscard]] std::string const& name() const noexcept { return _name; }

  /**
   * @brief Gets the current run_dir
   * @return run dir
   */
  [[nodiscard]] std::filesystem::path const& run_dir() const noexcept { return _run_dir; }

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

private:
  std::filesystem::path const& _run_dir;
  std::string _name;
  std::atomic<LogLevel> _log_level{LogLevel::Info};
};

/**
 * @brief Logger class
 */
template <typename TFrontendOptions>
class Logger : public LoggerBase
{
public:
  using frontend_options_t = TFrontendOptions;

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
  void log(Args const&... args)
  {
    // Get the queue associated with this thread
    detail::ThreadLocalQueue<frontend_options_t>& tl_queue =
      detail::get_thread_local_queue<frontend_options_t>(run_dir(), _options);

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
  friend class FrontendManager<frontend_options_t>;

  /**
   * @brief Private constructor for Logger.
   *
   * @param name Name of the logger.
   * @param run_dir Current running dir
   * @param options Frontend options for the logger.
   */
  Logger(std::filesystem::path const& run_dir, std::string_view name, frontend_options_t const& options)
    : LoggerBase(run_dir, name), _options(options)
  {
  }

private:
  frontend_options_t const& _options;
};

/**
 * @brief Manager class for the frontend
 */
template <typename TFrontendOptions>
class FrontendManager
{
public:
  using frontend_options_t = TFrontendOptions;

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

    if (!detail::create_loggers_metadata_file(_run_dir))
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
   * @brief Retrieves or creates a logger with the specified name.
   *
   * @param name The name of the logger.
   * @return A pointer to the logger with the given name.
   *         If creation fails while appending to loggers-metadata returns nullptr.
   */
  [[nodiscard]] Logger<frontend_options_t>* logger(std::string const& name)
  {
    std::lock_guard<std::mutex> lock_guard{_lock};

    // Check if the logger already exists in the registry
    if (auto search = _logger_registry.find(name); search != std::end(_logger_registry))
    {
      return reinterpret_cast<Logger<frontend_options_t>*>(search->second.get());
    }

    // Logger doesn't exist, create a new one
    auto [it, emplaced] = _logger_registry.emplace(
      std::make_pair(name, new Logger<frontend_options_t>(_run_dir, name, _options)));
    assert(emplaced);

    // Append logger metadata to the loggers-metadata.yaml file
    if (!detail::append_loggers_metadata_file(_run_dir, it->second->id, it->second->name())) [[unlikely]]
    {
      _logger_registry.erase(it);
      return nullptr;
    }

    return reinterpret_cast<Logger<frontend_options_t>*>(it->second.get());
  }

private:
  detail::MetadataFile _app_running_file;
  mutable std::mutex _lock;
  std::unordered_map<std::string, std::unique_ptr<LoggerBase>> _logger_registry; ///< key is the logger name, value is a pointer to the logger.
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
   * @brief Retrieves or creates a logger with the specified name.
   *
   * @param name The name of the logger.
   * @return A pointer to the logger with the given name.
   *         If creation fails while appending to loggers-metadata returns nullptr.
   */
  [[nodiscard]] Logger<frontend_options_t>* logger(std::string const& name)
  {
    return _frontend_manager.logger(name);
  }

private:
  Frontend(std::string_view application_id, frontend_options_t options, std::string_view base_dir)
    : _frontend_manager(application_id, std::move(options), base_dir){};
  static inline std::unique_ptr<Frontend<frontend_options_t>> _instance;
  FrontendManager<frontend_options_t> _frontend_manager;
};
} // namespace bitlog