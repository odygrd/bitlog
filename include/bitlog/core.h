#pragma once

#include <cassert>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>

#include "bitlog/detail/common.h"

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
struct BitlogOptions
{
  static constexpr QueueType queue_type = QueueOption;
  static constexpr bool use_custom_memcpy_x86 = UseCustomMemcpyX86;
  uint64_t queue_capacity_bytes{131'072u};
};

/**
 * @brief Manager class for Bitlog
 *
 * @tparam TBitlogConfig The configuration type for Bitlog.
 */
template <typename TBitlogConfig>
class BitlogManager
{
public:
  using bitlog_options_t = TBitlogConfig;

  /**
   * @brief Explicit constructor for BitlogManager.
   * @param application_id Identifier for the application.
   * @param config Configuration options for Bitlog.
   * @param base_dir Base directory path.
   */
  explicit BitlogManager(std::string_view application_id, bitlog_options_t config = bitlog_options_t{},
                         std::string_view base_dir = std::string_view{})
    : _config(std::move(config))
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
   * @brief Get the configuration options for Bitlog.
   * @return Bitlog configuration options.
   */
  [[nodiscard]] bitlog_options_t const& config() const noexcept { return _config; }

private:
  std::filesystem::path _run_dir;
  bitlog_options_t _config;
};

/**
 * @brief Singleton class for Bitlog, providing initialization and access to BitlogManager.
 *
 * @tparam TBitlogConfig The configuration type for Bitlog.
 */
template <typename TBitlogConfig>
class Bitlog
{
public:
  using bitlog_options_t = TBitlogConfig;

  /**
   * @brief Initialize the Bitlog singleton.
   *
   * @param application_id Identifier for the application.
   * @param config Configuration options for Bitlog.
   * @param base_dir Base directory path.
   * @return True if initialization is successful, false otherwise.
   */
  static bool init(std::string_view application_id, bitlog_options_t config = bitlog_options_t{},
                   std::string_view base_dir = std::string_view{})
  {
    if (!detail::initialise_bitlog_once())
    {
      return false;
    }

    // Set up the singleton
    _instance.reset(new Bitlog<bitlog_options_t>(application_id, std::move(config), base_dir));

    return true;
  }

  /**
   * @brief Get the instance of the Bitlog singleton.
   *
   * @return Reference to the Bitlog singleton instance.
   */
  [[nodiscard]] static Bitlog<bitlog_options_t>& instance() noexcept
  {
    assert(_instance);
    return *_instance;
  }

  /**
   * @brief Get the base directory path.
   * @return Base directory path.
   */
  [[nodiscard]] std::filesystem::path base_dir() const noexcept { return _manager.base_dir(); }

  /**
   * @brief Get the application directory path.
   * @return Application directory path.
   */
  [[nodiscard]] std::filesystem::path application_dir() const noexcept
  {
    return _manager.application_dir();
  }

  /**
   * @brief Get the run directory path.
   * @return Run directory path.
   */
  [[nodiscard]] std::filesystem::path const& run_dir() const noexcept { return _manager.run_dir(); }

  /**
   * @brief Get the configuration options for Bitlog.
   * @return Bitlog configuration options.
   */
  [[nodiscard]] bitlog_options_t const& config() const noexcept { return _manager.config(); }

private:
  Bitlog(std::string_view application_id, bitlog_options_t config, std::string_view base_dir)
    : _manager(application_id, std::move(config), base_dir){};
  static inline std::unique_ptr<Bitlog<bitlog_options_t>> _instance;
  BitlogManager<bitlog_options_t> _manager;
};
} // namespace bitlog