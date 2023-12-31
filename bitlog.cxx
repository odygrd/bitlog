// Global module fragment where #includes can happen
module;

#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <expected>
#include <filesystem>
#include <limits>
#include <system_error>
#include <vector>

#include <fcntl.h>
#include <linux/mman.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#if defined(__x86_64__) || defined(_M_X64)
  #define X86_ARCHITECTURE_ENABLED
#endif

#if defined(X86_ARCHITECTURE_ENABLED)
  #include <immintrin.h>
  #include <x86intrin.h>
#endif

// first thing after the Global module fragment must be a module command
export module bitlog;

namespace bitlog
{
// Constants
static constexpr size_t CACHE_LINE_SIZE_BYTES{64u};
static constexpr size_t CACHE_LINE_ALIGNED{2 * CACHE_LINE_SIZE_BYTES};

// Attributes
#ifdef NDEBUG // Check if not in debug mode
  #if defined(__GNUC__) || defined(__clang__)
    #define ALWAYS_INLINE [[gnu::always_inline]]
  #elif defined(_MSC_VER)
    #define ALWAYS_INLINE [[msvc::forceinline]]
  #else
    #define ALWAYS_INLINE inline
  #endif
#else
  #define ALWAYS_INLINE
#endif

/**
 * Checks if a number is a power of 2
 * @param number the number to check against
 * @return true if the number is a power of 2, false otherwise
 */
[[nodiscard]] constexpr bool is_power_of_two(uint64_t number) noexcept
{
  return (number != 0) && ((number & (number - 1)) == 0);
}

/**
 * Finds the next power of 2 greater than or equal to the given input
 * @param n input number
 * @return the next power of 2
 */
template <typename T>
[[nodiscard]] T next_power_of_2(T n) noexcept
{
  if (n >= std::numeric_limits<T>::max() / 2)
  {
    return std::numeric_limits<T>::max() / 2;
  }

  if (is_power_of_two(n))
  {
    return n;
  }

  T power = 1;
  while (power < n)
  {
    power <<= 1;
  }

  return power;
}

/**
 * Aligns a pointer to the specified boundary
 * @tparam Alignment desired alignment
 * @tparam T desired return type
 * @param pointer a pointer to an object
 * @return an aligned pointer for the given object
 */
template <uint64_t Alignment, typename T>
[[nodiscard]] constexpr T* align_to_boundary(void* pointer) noexcept
{
  static_assert(is_power_of_two(Alignment), "Alignment must be a power of two");
  return reinterpret_cast<T*>((reinterpret_cast<uintptr_t>(pointer) + (Alignment - 1u)) & ~(Alignment - 1u));
}

/**
 * Round up a value to the nearest multiple of a specified size.
 * @tparam T Type of value to round.
 * @param value The value to be rounded up.
 * @param roundTo The size to which the value should be rounded.
 * @return The rounded-up value.
 */
template <typename T>
[[nodiscard]] T round_up_to_nearest(T value, T roundTo) noexcept
{
  return ((value + roundTo - 1) / roundTo) * roundTo;
}

/**
 * @brief Retrieves the ID of the current thread.
 *
 * Retrieves the ID of the current thread using system-specific calls.
 *
 * @return The ID of the current thread.
 */
[[nodiscard]] uint32_t get_thread_id() noexcept
{
  thread_local uint32_t thread_id{0};

  if (!thread_id)
  {
    thread_id = static_cast<uint32_t>(::syscall(SYS_gettid));
  }

  return thread_id;
}

/**
 * @brief Retrieves the system's page size or returns the specified memory page size.
 *
 * Retrieves the system's page size if MemoryPageSize is set to RegularPage;
 * otherwise, returns the specified memory page size.
 *
 * @param memory_page_size The specified memory page size to use
 * @return The size of the system's page or the specified memory page size.
 */
[[nodiscard]] size_t get_page_size(size_t memory_page_size) noexcept
{
  if (!memory_page_size)
  {
    thread_local uint32_t page_size{0};
    if (!page_size)
    {
      page_size = static_cast<uint32_t>(sysconf(_SC_PAGESIZE));
    }
    return page_size;
  }
  else
  {
    return memory_page_size;
  }
}
} // namespace bitlog

export namespace bitlog
{

enum MemoryPageSize : size_t
{
  RegularPage = 0,
  HugePage2MB = 2 * 1024 * 1024,
  HugePage1GB = 1024 * 1024 * 1024
};

enum ErrorCode : uint32_t
{
  InvalidSharedMemoryPath = 10000
};

/**
 * A bounded single producer single consumer ring buffer.
 */
template <typename T, bool X86_CACHE_COHERENCE_OPTIMIZATION = false>
class BoundedQueueImpl
{
public:
  using integer_type = T;

private:
  struct Metadata
  {
    integer_type capacity;
    integer_type mask;
    integer_type bytes_per_batch;

    alignas(CACHE_LINE_ALIGNED) std::atomic<integer_type> atomic_writer_pos;
    alignas(CACHE_LINE_ALIGNED) integer_type writer_pos;
    integer_type last_flushed_writer_pos;
    integer_type reader_pos_cache;

    alignas(CACHE_LINE_ALIGNED) std::atomic<integer_type> atomic_reader_pos;
    alignas(CACHE_LINE_ALIGNED) integer_type reader_pos;
    integer_type last_flushed_reader_pos;
    integer_type writer_pos_cache;
  };

public:
  BoundedQueueImpl() = default;
  BoundedQueueImpl(BoundedQueueImpl const&) = delete;
  BoundedQueueImpl& operator=(BoundedQueueImpl const&) = delete;

  ~BoundedQueueImpl()
  {
    if (_storage && _metadata)
    {
      ::munmap(_storage, 2ull * static_cast<uint64_t>(_metadata->capacity));
      ::munmap(_metadata, sizeof(Metadata));
    }

    if (_filelock_fd >= 0)
    {
      ::flock(_filelock_fd, LOCK_UN | LOCK_NB);
      ::close(_filelock_fd);
    }
  }

  /**
   * @brief Initializes Creates shared memory for storage, metadata, lock, and ready indicators.
   *
   * @param capacity The capacity of the shared memory storage.
   * @param shm_sub_dir The sub-directory within shared memory.
   * @param unique_id An optional unique identifier for the queue (default is an empty string).
   * @param page_size The size of memory pages (default is RegularPage).
   * @param shm_root_dir The root directory for shared memory (default is an empty path, resolving to /dev/shm or /tmp).
   * @param reader_store_percentage The percentage of memory reserved for the reader (default is 5%).
   *
   * @return An std::error_code indicating the success or failure of the initialization.
   */
  [[nodiscard]] std::error_code create(integer_type capacity, std::filesystem::path const& shm_sub_dir,
                                            std::string unique_id = std::string{},
                                            MemoryPageSize page_size = MemoryPageSize::RegularPage,
                                            std::filesystem::path const& shm_root_dir = std::filesystem::path{},
                                            integer_type reader_store_percentage = 5) noexcept
  {
    std::error_code ec{};

    // First resolve shm path
    std::expected<std::filesystem::path, std::error_code> const shm_path =
      _resolve_shm_path(shm_root_dir, shm_sub_dir);

    if (shm_path.has_value())
    {
      // capacity must be always rounded to page size otherwise mmap will fail
      capacity = round_up_to_nearest(capacity, static_cast<integer_type>(get_page_size(page_size)));

      if (unique_id.empty())
      {
        unique_id = std::to_string(get_thread_id()) + "_" +
          std::to_string(std::chrono::duration_cast<std::chrono::nanoseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count());
      }

      // 1. Create a storage file in shared memory
      std::filesystem::path shm_file_base = *shm_path / unique_id;

      shm_file_base.replace_extension(".storage");
      int storage_fd = ::open(shm_file_base.c_str(), O_CREAT | O_RDWR | O_EXCL, 0660);

      if (storage_fd >= 0)
      {
        if (ftruncate(storage_fd, static_cast<off_t>(capacity)) == 0)
        {
          ec = _memory_map_storage(storage_fd, capacity, page_size);

          if (!ec)
          {
            std::memset(_storage, 0, capacity);

            // 2. Create metadata file in shared memory
            shm_file_base.replace_extension("metadata");
            int metadata_fd = ::open(shm_file_base.c_str(), O_CREAT | O_RDWR | O_EXCL, 0660);

            if (metadata_fd >= 0)
            {
              if (ftruncate(metadata_fd, static_cast<off_t>(sizeof(Metadata))) == 0)
              {
                ec = _memory_map_metadata(metadata_fd, sizeof(Metadata), page_size);

                if (!ec)
                {
                  std::memset(_metadata, 0, sizeof(Metadata));

                  _metadata->capacity = capacity;
                  _metadata->mask = capacity - 1;
                  _metadata->bytes_per_batch =
                    static_cast<integer_type>(capacity * static_cast<double>(reader_store_percentage) / 100.0);
                  _metadata->atomic_writer_pos = 0;
                  _metadata->writer_pos = 0;
                  _metadata->last_flushed_writer_pos = 0;
                  _metadata->reader_pos_cache = 0;
                  _metadata->atomic_reader_pos = 0;
                  _metadata->reader_pos = 0;
                  _metadata->last_flushed_reader_pos = 0;
                  _metadata->writer_pos_cache = 0;

                  // 3. Create a lock file that works as a heartbeat mechanism for the reader
                  shm_file_base.replace_extension("lock");
                  _filelock_fd = ::open(shm_file_base.c_str(), O_CREAT | O_RDWR | O_EXCL, 0660);

                  if (_filelock_fd >= 0)
                  {
                    if (::flock(_filelock_fd, LOCK_EX | LOCK_NB) == 0)
                    {
                      // 4. Create a ready file, indicating everything is ready and initialised and
                      // the reader can start loading the files
                      shm_file_base.replace_extension("ready");
                      int readyfile_fd = ::open(shm_file_base.c_str(), O_CREAT | O_RDWR | O_EXCL, 0660);

                      if (readyfile_fd >= 0)
                      {
                        ::close(readyfile_fd);

                        // Queue creation is done
#if defined(X86_ARCHITECTURE_ENABLED)
                        if constexpr (X86_CACHE_COHERENCE_OPTIMIZATION)
                        {
                          // remove log memory from cache
                          for (uint64_t i = 0; i < (2ull * static_cast<uint64_t>(_metadata->capacity));
                               i += CACHE_LINE_SIZE_BYTES)
                          {
                            _mm_clflush(_storage + i);
                          }

                          uint64_t constexpr prefetched_cache_lines = 16;

                          for (uint64_t i = 0; i < prefetched_cache_lines; ++i)
                          {
                            _mm_prefetch(
                              reinterpret_cast<char const*>(_storage + (CACHE_LINE_SIZE_BYTES * i)), _MM_HINT_T0);
                          }
                        }
#endif
                      }
                      else
                      {
                        ec = std::error_code{errno, std::generic_category()};
                      }
                    }
                    else
                    {
                      ec = std::error_code{errno, std::generic_category()};
                    }
                  }
                  else
                  {
                    ec = std::error_code{errno, std::generic_category()};
                  }
                }
              }
              else
              {
                ec = std::error_code{errno, std::generic_category()};
              }

              ::close(metadata_fd);
            }
            else
            {
              ec = std::error_code{errno, std::generic_category()};
            }
          }
        }
        else
        {
          ec = std::error_code{errno, std::generic_category()};
        }

        ::close(storage_fd);
      }
      else
      {
        ec = std::error_code{errno, std::generic_category()};
      }
    }
    else
    {
      ec = shm_path.error();
    }

    return ec;
  }

  /**
   * @brief Opens existing shared memory storage and metadata files associated with a specified postfix ID.
   *
   * @param unique_id The unique ID used to identify the storage and metadata files.
   * @param shm_sub_dir The sub-directory within shared memory.
   * @param page_size The size of memory pages (default is RegularPage).
   * @param shm_root_dir The root directory for shared memory (default is an empty path).
   *
   * @return An std::error_code indicating the success or failure of the initialization.
   */
  [[nodiscard]] std::error_code open(std::string const& unique_id, std::filesystem::path const& shm_sub_dir,
                                            MemoryPageSize page_size = MemoryPageSize::RegularPage,
                                            std::filesystem::path const& shm_root_dir = std::filesystem::path{}) noexcept
  {
    std::error_code ec{};

    // First resolve shm directories
    std::expected<std::filesystem::path, std::error_code> const shm_path =
      _resolve_shm_path(shm_root_dir, shm_sub_dir);

    if (shm_path.has_value())
    {
      // We will only load the storage and metadata files. The ready and lock files are handled
      // in a different function
      std::filesystem::path shm_file_base = *shm_path / unique_id;

      shm_file_base.replace_extension("storage");
      int storage_fd = ::open(shm_file_base.c_str(), O_RDWR, 0660);

      if (storage_fd >= 0)
      {
        size_t const storage_file_size = std::filesystem::file_size(shm_file_base, ec);

        if (!ec)
        {
          ec = _memory_map_storage(storage_fd, storage_file_size, page_size);

          if (!ec)
          {
            shm_file_base.replace_extension("metadata");
            int metadata_fd = ::open(shm_file_base.c_str(), O_RDWR, 0660);

            if (metadata_fd >= 0)
            {
              size_t const metadata_file_size = std::filesystem::file_size(shm_file_base, ec);

              if (!ec)
              {
                ec = _memory_map_metadata(metadata_fd, metadata_file_size, page_size);

                if (!ec)
                {
                  // finally we can also open the lockfile
                  shm_file_base.replace_extension("lock");
                  _filelock_fd = ::open(shm_file_base.c_str(), O_RDWR, 0660);

                  if (_filelock_fd == -1)
                  {
                    ec = std::error_code{errno, std::generic_category()};
                  }
                }
              }

              ::close(metadata_fd);
            }
            else
            {
              ec = std::error_code{errno, std::generic_category()};
            }
          }
        }

        ::close(storage_fd);
      }
      else
      {
        ec = std::error_code{errno, std::generic_category()};
      }
    }
    else
    {
      ec = shm_path.error();
    }

    return ec;
  }

  /**
   * @brief Searches for ready files under the provided directory and retrieves unique IDs.
   *
   * This function scans the specified directory in shared memory and gathers unique IDs
   * from filenames that match the "ready-" pattern.
   *
   * @param unique_ids A vector to store the unique IDs found in the filenames.
   * @param shm_sub_dir The sub-directory within shared memory.
   * @param shm_root_dir The root directory for shared memory (default is an empty path).
   *
   * @return An std::error_code indicating the success or failure of the discovery process.
   */
  [[nodiscard]] static std::error_code discover_writers(
    std::vector<std::string>& unique_ids, std::filesystem::path const& shm_sub_dir,
    std::filesystem::path const& shm_root_dir = std::filesystem::path{}) noexcept
  {
    std::error_code ec{};

    unique_ids.clear();

    // First resolve shm directories
    std::expected<std::filesystem::path, std::error_code> const shm_path =
      _resolve_shm_path(shm_root_dir, shm_sub_dir);

    if (shm_path.has_value())
    {
      auto di = std::filesystem::directory_iterator(*shm_path, ec);

      if (!ec)
      {
        for (auto const& entry : di)
        {
          if (entry.is_regular_file())
          {
            if (entry.path().extension().string() == ".ready")
            {
              unique_ids.emplace_back(entry.path().filename().stem());
            }
          }
        }
      }
    }
    else
    {
      ec = shm_path.error();
    }

    return ec;
  }

  [[nodiscard]] static std::expected<bool, std::error_code> is_created(std::string const& unique_id, std::filesystem::path const& shm_sub_dir,
                                                  std::filesystem::path const& shm_root_dir = std::filesystem::path{})
  {
    std::expected<bool, std::error_code> created;

      // First resolve shm directories
    std::expected<std::filesystem::path, std::error_code> const shm_path =
      _resolve_shm_path(shm_root_dir, shm_sub_dir);

    if (shm_path.has_value())
    {
      std::filesystem::path shm_file_base = *shm_path / unique_id;
      shm_file_base.replace_extension(".ready");

      std::error_code ec;
      created = std::filesystem::exists(shm_file_base, ec);

      if (ec)
      {
        created = std::unexpected{ec};
      }
    }
    else
    {
      created = std::unexpected{shm_path.error()};
    }

    return created;
  }

  /**
 * @brief Removes shared memory files associated with a unique identifier.
 *
 * This function removes shared memory files (storage, metadata, ready, lock)
 * associated with a specified unique identifier.
 *
 * @param unique_id The unique identifier used to identify the shared memory files.
 * @param shm_sub_dir The sub-directory within shared memory.
 * @param shm_root_dir The root directory for shared memory (default is an empty path, resolving to /dev/shm or /tmp).
 *
 * @return An std::error_code indicating the success or failure
   */
  [[nodiscard]] static std::error_code remove_shm_files(
    std::string const& unique_id, std::filesystem::path const& shm_sub_dir,
    std::filesystem::path const& shm_root_dir = std::filesystem::path{}) noexcept
  {
    std::error_code ec{};

    // First resolve shm directories
    std::expected<std::filesystem::path, std::error_code> const shm_path =
      _resolve_shm_path(shm_root_dir, shm_sub_dir);

    if (shm_path.has_value())
    {
      std::filesystem::path shm_file_base = *shm_path / unique_id;
      shm_file_base.replace_extension(".storage");
      std::filesystem::remove(shm_file_base, ec);

      shm_file_base.replace_extension(".metadata");
      std::filesystem::remove(shm_file_base, ec);

      shm_file_base.replace_extension(".ready");
      std::filesystem::remove(shm_file_base, ec);

      shm_file_base.replace_extension(".lock");
      std::filesystem::remove(shm_file_base, ec);
    }
    else
    {
      ec = shm_path.error();
    }

    return ec;
  }

  /**
   * Checks if the process that created the queue is currently running by attempting to acquire
   * a file lock.
   * @return An std::expected<bool, std::error_code>:
   *         - std::expected(false) if the creator process is not running and the lock is acquired successfully.
   *         - std::expected(true) if the creator process is running and the file lock is unavailable.
   *         - std::unexpected<std::error_code> containing an error code if an error occurs during the check.
   */
  [[nodiscard]] std::expected<bool, std::error_code> is_creator_process_running() const noexcept
  {
    std::expected<bool, std::error_code> is_running;

    if (::flock(_filelock_fd, LOCK_EX | LOCK_NB) == 0)
    {
      // if we can lock the file, then the process is not running
      is_running = false;

      // we also want to unlock the file in case we repeat the check
      if (::flock(_filelock_fd, LOCK_UN | LOCK_NB) == -1)
      {
        is_running = std::unexpected{std::error_code{errno, std::generic_category()}};
      }
    }
    else if (errno == EWOULDBLOCK)
    {
      // the file is still locked
      is_running = true;
    }
    else
    {
      is_running = std::unexpected{std::error_code{errno, std::generic_category()}};
    }

    return is_running;
  }

  /**
   * @brief Prepares space for writing in the shared memory.
   *
   * This function checks available space in the storage to accommodate the given size for writing.
   * If there isn't enough space, it returns nullptr; otherwise, it returns the address for writing.
   *
   * @param n The size requested for writing.
   * @return A pointer to the location in memory for writing, or nullptr if insufficient space is available.
   */
  ALWAYS_INLINE [[nodiscard]] std::byte* prepare_write(integer_type n) noexcept
  {
    if ((_metadata->capacity - static_cast<integer_type>(_metadata->writer_pos - _metadata->reader_pos_cache)) < n)
    {
      // not enough space, we need to load reader and re-check
      _metadata->reader_pos_cache = _metadata->atomic_reader_pos.load(std::memory_order_acquire);

      if ((_metadata->capacity - static_cast<integer_type>(_metadata->writer_pos - _metadata->reader_pos_cache)) < n)
      {
        return nullptr;
      }
    }

    return _storage + (_metadata->writer_pos & _metadata->mask);
  }

  /**
   * @brief Marks the completion of a write operation in the shared memory.
   *
   * This function updates the writer position by the given size after finishing a write operation.
   *
   * @param n The size of the completed write operation.
   */
  ALWAYS_INLINE void finish_write(integer_type n) noexcept { _metadata->writer_pos += n; }

  /**
   * @brief Commits the finished write operation in the shared memory.
   *
   * This function sets an atomic flag to indicate that the written data is available for reading.
   * It also performs cache-related operations for optimization if the architecture allows.
   */
  ALWAYS_INLINE void commit_write() noexcept
  {
    // set the atomic flag so the reader can see write
    _metadata->atomic_writer_pos.store(_metadata->writer_pos, std::memory_order_release);

#if defined(X86_ARCHITECTURE_ENABLED)
    if constexpr (X86_CACHE_COHERENCE_OPTIMIZATION)
    {
      // flush writen cache lines
      _flush_cachelines(_metadata->last_flushed_writer_pos, _metadata->writer_pos);

      // prefetch a future cache line
      _mm_prefetch(reinterpret_cast<char const*>(_storage + (_metadata->writer_pos & _metadata->mask) +
                                                 (CACHE_LINE_SIZE_BYTES * 10)),
                   _MM_HINT_T0);
    }
#endif
  }

  /**
   * @brief Prepares for reading from the shared memory.
   *
   * This function checks if there is data available for reading in the shared memory.
   * If data is available, it returns the address for reading; otherwise, it returns nullptr.
   *
   * @return A pointer to the location in memory for reading, or nullptr if no data is available.
   */
  ALWAYS_INLINE [[nodiscard]] std::byte* prepare_read() noexcept
  {
    if (_metadata->writer_pos_cache == _metadata->reader_pos)
    {
      _metadata->writer_pos_cache = _metadata->atomic_writer_pos.load(std::memory_order_acquire);

      if (_metadata->writer_pos_cache == _metadata->reader_pos)
      {
        return nullptr;
      }
    }

    return _storage + (_metadata->reader_pos & _metadata->mask);
  }

  /**
   * @brief Marks the completion of a read operation in the shared memory.
   *
   * This function updates the reader position by the given size after finishing a read operation.
   *
   * @param n The size of the completed read operation.
   */
  ALWAYS_INLINE void finish_read(integer_type n) noexcept { _metadata->reader_pos += n; }

  /**
   * @brief Commits the finished read operation in the shared memory.
   *
   * This function updates the atomic reader position and performs cache-related operations
   * for optimization, if applicable based on the architecture.
   */
  ALWAYS_INLINE void commit_read() noexcept
  {
    if (static_cast<integer_type>(_metadata->reader_pos - _metadata->atomic_reader_pos.load(std::memory_order_relaxed)) >=
        _metadata->bytes_per_batch)
    {
      _metadata->atomic_reader_pos.store(_metadata->reader_pos, std::memory_order_release);

#if defined(X86_ARCHITECTURE_ENABLED)
      if constexpr (X86_CACHE_COHERENCE_OPTIMIZATION)
      {
        _flush_cachelines(_metadata->last_flushed_reader_pos, _metadata->reader_pos);
      }
#endif
    }
  }

  /**
   * @brief Checks if the shared memory storage is empty.
   *
   * This function is intended to be called by the reader to determine if the storage is empty.
   *
   * @return True if the storage is empty, otherwise false.
   */
  [[nodiscard]] bool empty() const noexcept
  {
    return _metadata->reader_pos == _metadata->atomic_writer_pos.load(std::memory_order_relaxed);
  }

  /**
   * @brief Retrieves the capacity of the storage.
   * @return An integer representing the storage capacity.
   */
  [[nodiscard]] integer_type capacity() const noexcept { return _metadata->capacity; }

private:
  /**
   * @brief Resolves the path for shared memory storage.
   *
   * Resolves the path for shared memory storage based on the provided root directory
   * and sub-directory within the shared memory.
   *
   * @param shm_root_dir The root directory for shared memory.
   * @param shm_sub_dir The sub-directory within shared memory.
   * @return An expected filesystem path if the resolution is successful, otherwise an error code.
   */
  [[nodiscard]] static std::expected<std::filesystem::path, std::error_code> _resolve_shm_path(
    std::filesystem::path shm_root_dir, std::filesystem::path const& shm_sub_dir) noexcept
  {
    // resolve shared memory directories
    if (shm_root_dir.empty())
    {
      if (std::filesystem::exists("/dev/shm"))
      {
        shm_root_dir = "/dev/shm";
      }
      else if (std::filesystem::exists("/tmp"))
      {
        shm_root_dir = "/tmp";
      }
    }

    std::filesystem::path shm_path = shm_root_dir / shm_sub_dir;

    if (!std::filesystem::exists(shm_path))
    {
      return std::unexpected{std::error_code{ErrorCode::InvalidSharedMemoryPath, std::generic_category()}};
    }

    return shm_path;
  }

  /**
   * @brief Resolves the flags for memory mapping based on the specified page size.
   *
   * Resolves the flags required for memory mapping according to the specified memory page size.
   *
   * @param page_size The size of memory pages.
   * @return An integer representing the resolved mapping flags.
   */
  [[nodiscard]] static int resolve_mmap_flags(MemoryPageSize page_size) noexcept
  {
    int flags = MAP_SHARED;

    if (page_size == MemoryPageSize::HugePage2MB)
    {
      flags |= MAP_HUGETLB | MAP_HUGE_2MB;
    }
    else if (page_size == MemoryPageSize::HugePage1GB)
    {
      flags |= MAP_HUGETLB | MAP_HUGE_1GB;
    }

    return flags;
  }

  /**
   * @brief Maps the storage into memory.
   *
   * Maps the storage into memory using mmap and sets the storage address.
   *
   * @param fd The file descriptor for the storage.
   * @param size The size of the storage.
   * @param memory_page_size The size of memory pages.
   * @return An error code indicating success or failure of memory mapping.
   */
  [[nodiscard]] std::error_code _memory_map_storage(int fd, size_t size, MemoryPageSize memory_page_size) noexcept
  {
    // ask mmap for a good address where we can put both virtual copies of the buffer
    auto storage_address =
      static_cast<std::byte*>(::mmap(nullptr, 2 * size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));

    if (storage_address == MAP_FAILED)
    {
      return std::error_code{errno, std::generic_category()};
    }

    // map first region
    if (auto storage_address_1 = ::mmap(storage_address, size, PROT_READ | PROT_WRITE,
                                        MAP_FIXED | resolve_mmap_flags(memory_page_size), fd, 0);
        (storage_address_1 == MAP_FAILED) || (storage_address_1 != storage_address))
    {
      ::munmap(storage_address, 2 * size);
      return std::error_code{errno, std::generic_category()};
    }

    // map second region
    if (auto storage_address_2 = ::mmap(storage_address + size, size, PROT_READ | PROT_WRITE,
                                        MAP_FIXED | resolve_mmap_flags(memory_page_size), fd, 0);
        (storage_address_2 == MAP_FAILED) || (storage_address_2 != storage_address + size))
    {
      ::munmap(storage_address, 2 * size);
      return std::error_code{errno, std::generic_category()};
    }

    _storage = storage_address;

    return std::error_code{};
  }

  /**
   * @brief Maps the metadata into memory.
   *
   * Maps the metadata into memory using mmap and sets the metadata address.
   *
   * @param fd The file descriptor for the metadata.
   * @param size The size of the metadata.
   * @param memory_page_size The size of memory pages.
   * @return An error code indicating success or failure of memory mapping.
   */
  [[nodiscard]] std::error_code _memory_map_metadata(int fd, size_t size, MemoryPageSize memory_page_size) noexcept
  {
    auto metadata_address =
      ::mmap(nullptr, size, PROT_READ | PROT_WRITE, resolve_mmap_flags(memory_page_size), fd, 0);

    if (metadata_address == MAP_FAILED)
    {
      return std::error_code{errno, std::generic_category()};
    }

    // do not use std::construct_at(), it will perform value initialization
    _metadata = static_cast<Metadata*>(metadata_address);

    return std::error_code{};
  }

#if defined(X86_ARCHITECTURE_ENABLED)
  /**
   * @brief Flushes cache lines for optimization.
   *
   * Flushes cache lines in a specific range for optimization on x86 architectures.
   *
   * @param last The last position processed.
   * @param offset The current offset position.
   */
  void _flush_cachelines(integer_type& last, integer_type offset) noexcept
  {
    integer_type last_diff = last - (last & CACHELINE_MASK);
    integer_type const cur_diff = offset - (offset & CACHELINE_MASK);

    while (cur_diff > last_diff)
    {
      _mm_clflushopt(reinterpret_cast<void*>(_storage + (last_diff & _metadata->mask)));
      last_diff += CACHE_LINE_SIZE_BYTES;
      last = last_diff;
    }
  }
#endif

private:
  static constexpr integer_type CACHELINE_MASK{CACHE_LINE_SIZE_BYTES - 1};

  Metadata* _metadata{nullptr};
  std::byte* _storage{nullptr};

  // local variables - not stored in shm
  int _filelock_fd{-1};
};

using BoundedQueue = BoundedQueueImpl<uint64_t, false>;
using BoundedQueueX86 = BoundedQueueImpl<uint64_t, true>;
} // namespace bitlog