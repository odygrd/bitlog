#pragma once

#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>

#include "bitlog/common/common.h"
#include "bitlog/frontend/frontend_impl.h"

namespace bitlog
{
enum class QueueType
{
  BoundedDropping,
  BoundedBlocking,
  UnboundedNoLimit
};

/**
 * @brief Structure representing configuration options for Bitlog.
 *
 * @tparam QueueOption The type of queue to be used.
 * @tparam UseCustomMemcpyX86 Boolean indicating whether to use a custom memcpy implementation for x86.
 */
template <QueueType QueueOption, bool UseCustomMemcpyX86>
struct FrontendOptions
{
  static constexpr QueueType queue_type = QueueOption;
  static constexpr bool use_custom_memcpy_x86 = UseCustomMemcpyX86;
  uint64_t queue_capacity_bytes{131'072u};
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
   */
  explicit LoggerBase(std::string_view name) : _name(name){};

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
    uint32_t c_style_string_lengths_array[(std::max)(detail::count_c_style_strings<Args...>(), static_cast<uint32_t>(1))];

    // Also reserve space for the timestamp the metadata id and the logger id
    constexpr auto additional_space =
      static_cast<uint32_t>(sizeof(uint64_t) + sizeof(uint32_t) + sizeof(uint32_t));

    uint32_t const total_args_size = additional_space +
      calculate_args_size_and_populate_string_lengths(c_style_string_lengths_array, args...);

    auto& thread_context = get_thread_context<frontend_options_t>(_options);
    uint8_t* write_buffer = thread_context.queue().prepare_write(total_args_size);

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

      thread_context.queue().finish_write(total_args_size);
      thread_context.queue().commit_write();
    }
    else
    {
      // Queue is full TODO
    }
  }

private:
  friend class FrontendManager<frontend_options_t>;

  /**
   * @brief Private constructor for Logger.
   *
   * @param name Name of the logger.
   * @param options Frontend options for the logger.
   */
  Logger(std::string_view name, frontend_options_t const& options)
    : LoggerBase(name), _options(options)
  {
  }

private:
  frontend_options_t const& _options;
};

/**
 * @brief Manager class for Bitlog
 */
template <typename TFrontendOptions>
class FrontendManager
{
public:
  using frontend_options_t = TFrontendOptions;

  /**
   * @brief Explicit constructor for BitlogManager.
   * @param application_id Identifier for the application.
   * @param options Options for Bitlog.
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

    if (!detail::create_log_statements_metadata_file(_run_dir))
    {
      std::abort();
    }

    if (!detail::create_loggers_metadata_file(_run_dir))
    {
      std::abort();
    }
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
   * @brief Get the options for Bitlog.
   * @return Bitlog options.
   */
  [[nodiscard]] frontend_options_t const& options() const noexcept { return _options; }

private:
  std::filesystem::path _run_dir;
  frontend_options_t _options;
};

/**
 * @brief Singleton class for Bitlog, providing initialization and access to BitlogManager.
 */
template <typename TFrontendOptions>
class Frontend
{
public:
  using frontend_options_t = TFrontendOptions;

  /**
   * @brief Initialize the Bitlog singleton.
   *
   * @param application_id Identifier for the application.
   * @param options optionsuration options for Bitlog.
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
   * @brief Get the instance of the Bitlog singleton.
   * @return Reference to the Bitlog singleton instance.
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
   * @brief Get the optionsuration options for Bitlog.
   * @return Bitlog optionsuration options.
   */
  [[nodiscard]] frontend_options_t const& options() const noexcept
  {
    return _frontend_manager.options();
  }

private:
  Frontend(std::string_view application_id, frontend_options_t options, std::string_view base_dir)
    : _frontend_manager(application_id, std::move(options), base_dir){};
  static inline std::unique_ptr<Frontend<frontend_options_t>> _instance;
  FrontendManager<frontend_options_t> _frontend_manager;
};
} // namespace bitlog