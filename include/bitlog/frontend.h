#pragma once

#include <cassert>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>

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
    std::error_code ec{};
    _run_dir = base_dir.empty() ? (std::filesystem::exists("/dev/shm", ec) ? "/dev/shm" : "/tmp") : base_dir;

    if (ec)
    {
      std::abort();
    }

    _run_dir /= application_id;
    _run_dir /= std::to_string(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                 std::chrono::system_clock::now().time_since_epoch())
                                 .count());

    if (std::filesystem::exists(_run_dir, ec) || ec)
    {
      std::abort();
    }

    std::filesystem::create_directories(_run_dir, ec);

    if (ec)
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