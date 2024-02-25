#pragma once

#include <filesystem>
#include <string>
#include <chrono>

namespace bitlog
{
template <typename TQueue, bool CustomMemcpyX86>
class Config
{
public:
  explicit Config(std::string application_id, std::filesystem::path shm_root_dir = std::filesystem::path{})
    : _application_id(std::move(application_id)), _root_dir(std::move(shm_root_dir))
  {
    // resolve shared memory directories
    if (_root_dir.empty())
    {
      if (std::filesystem::exists("/dev/shm"))
      {
        _root_dir = "/dev/shm";
      }
      else if (std::filesystem::exists("/tmp"))
      {
        _root_dir = "/tmp";
      }
    }

    _app_dir = _root_dir / _application_id;

    std::error_code ec{};

    if (!std::filesystem::exists(_app_dir))
    {
      std::filesystem::create_directories(_app_dir, ec);

      if (ec)
      {
        std::abort();
      }
    }

    uint64_t const start_ts = std::chrono::duration_cast<std::chrono::nanoseconds>(
                                std::chrono::system_clock::now().time_since_epoch())
                                .count();

    _instance_dir = _app_dir / std::to_string(start_ts);

    if (std::filesystem::exists(_instance_dir))
    {
      std::abort();
    }

    std::filesystem::create_directories(_instance_dir, ec);

    if (ec)
    {
      std::abort();
    }
  }

  using queue_t = TQueue;
  static constexpr bool use_custom_memcpy_x86 = CustomMemcpyX86;

  void set_queue_capacity(uint64_t queue_capacity) noexcept { _queue_capacity = queue_capacity; }

  [[nodiscard]] std::filesystem::path const& root_dir() const noexcept { return _root_dir; }
  [[nodiscard]] std::filesystem::path const& app_dir() const noexcept { return _app_dir; }
  [[nodiscard]] std::filesystem::path const& instance_dir() const noexcept { return _instance_dir; }
  [[nodiscard]] uint64_t queue_capacity() const noexcept { return _queue_capacity; }

private:
  std::string _application_id;
  std::filesystem::path _root_dir;
  std::filesystem::path _app_dir;
  std::filesystem::path _instance_dir;
  uint64_t _queue_capacity{131'072u};
};
} // namespace bitlog