#pragma once

#include "bitlog/detail/common.h"

#include <atomic>
#include <cstring>
#include <filesystem>
#include <string>
#include <system_error>

#include <linux/mman.h>
#include <sys/file.h>
#include <sys/mman.h>

#if defined(BITLOG_X86_ARCHITECTURE_ENABLED)
  #include <immintrin.h>
  #include <x86intrin.h>
#endif

namespace bitlog::detail
{
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
      std::error_code ec;
      unlock_file(_filelock_fd, ec);
      ::close(_filelock_fd);
    }
  }

  /**
   * @brief Creates shared memory for storage, metadata, lock, and ready indicators.
   *
   * @param capacity The capacity of the shared memory storage.
   * @param path_base The path within shared memory.
   * @param memory_page_size The size of memory pages.
   * @param reader_store_percentage The percentage of memory reserved for the reader.
   * @param ec out-parameter for error reporting
   * @return true if the queue was created, false otherwise.
   */
  [[nodiscard]] bool create(integer_type capacity, std::filesystem::path path_base, MemoryPageSize memory_page_size,
                            integer_type reader_store_percentage, std::error_code& ec) noexcept
  {
    bool ret_val{true};

    // first need to figure out the page size
    uint32_t const page_size_bytes =
      (memory_page_size == MemoryPageSize::RegularPage) ? page_size() : memory_page_size;

    // capacity must be always rounded to the page size otherwise mmap will fail
    capacity = round_up_to_nearest(capacity, static_cast<integer_type>(page_size_bytes));

    // 1. Create a storage file in shared memory
    path_base.replace_extension(".data");
    int storage_fd = ::open(path_base.c_str(), O_CREAT | O_RDWR | O_EXCL, 0660);

    if (storage_fd >= 0)
    {
      if (ftruncate(storage_fd, static_cast<off_t>(capacity)) == 0)
      {
        ret_val = _memory_map_storage(storage_fd, capacity, memory_page_size, ec);
        if (ret_val)
        {
          std::memset(_storage, 0, capacity);

          // 2. Create metadata file in shared memory
          path_base.replace_extension("members");
          int metadata_fd = ::open(path_base.c_str(), O_CREAT | O_RDWR | O_EXCL, 0660);

          if (metadata_fd >= 0)
          {
            if (ftruncate(metadata_fd, static_cast<off_t>(sizeof(Metadata))) == 0)
            {
              ret_val = _memory_map_metadata(metadata_fd, sizeof(Metadata), memory_page_size, ec);
              if (ret_val)
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
                path_base.replace_extension("lock");
                _filelock_fd = ::open(path_base.c_str(), O_CREAT | O_RDWR | O_EXCL, 0660);

                if (_filelock_fd >= 0)
                {
                  if (lock_file(_filelock_fd, ec))
                  {
                    // 4. Create a ready file, indicating everything is ready and initialised and
                    // the reader can start loading the files
                    path_base.replace_extension("ready");
                    int readyfile_fd = ::open(path_base.c_str(), O_CREAT | O_RDWR | O_EXCL, 0660);

                    if (readyfile_fd >= 0)
                    {
                      ::close(readyfile_fd);

                      // Queue creation is done
#if defined(BITLOG_X86_ARCHITECTURE_ENABLED)
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
                      ret_val = false;
                    }
                  }
                  else
                  {
                    // ec already set by lock_file
                    ret_val = false;
                  }
                }
                else
                {
                  ec = std::error_code{errno, std::generic_category()};
                  ret_val = false;
                }
              }
            }
            else
            {
              ec = std::error_code{errno, std::generic_category()};
              ret_val = false;
            }

            ::close(metadata_fd);
          }
          else
          {
            ec = std::error_code{errno, std::generic_category()};
            ret_val = false;
          }
        }
      }
      else
      {
        ec = std::error_code{errno, std::generic_category()};
        ret_val = false;
      }

      ::close(storage_fd);
    }
    else
    {
      ec = std::error_code{errno, std::generic_category()};
      ret_val = false;
    }

    return ret_val;
  }

  /**
   * @brief Opens existing shared memory storage and metadata files associated with a specified postfix ID.
   *
   * @param path_base The path within shared memory.
   * @param page_size The size of memory pages (default is RegularPage).
   * @param ec out-parameter for error reporting
   * @return true if the queue was opened, false otherwise.
   */
  [[nodiscard]] bool open(std::filesystem::path path_base, MemoryPageSize page_size, std::error_code& ec) noexcept
  {
    bool ret_val{true};

    path_base.replace_extension("data");
    int storage_fd = ::open(path_base.c_str(), O_RDWR, 0660);

    if (storage_fd >= 0)
    {
      size_t const storage_file_size = std::filesystem::file_size(path_base, ec);

      if (!ec)
      {
        ret_val = _memory_map_storage(storage_fd, storage_file_size, page_size, ec);

        if (ret_val)
        {
          path_base.replace_extension("members");
          int metadata_fd = ::open(path_base.c_str(), O_RDWR, 0660);

          if (metadata_fd >= 0)
          {
            size_t const metadata_file_size = std::filesystem::file_size(path_base, ec);

            if (!ec)
            {
              ret_val = _memory_map_metadata(metadata_fd, metadata_file_size, page_size, ec);

              if (ret_val)
              {
                // finally we can also open the lockfile
                path_base.replace_extension("lock");
                _filelock_fd = ::open(path_base.c_str(), O_RDWR, 0660);

                if (_filelock_fd == -1)
                {
                  ec = std::error_code{errno, std::generic_category()};
                  ret_val = false;
                }
              }
            }

            ::close(metadata_fd);
          }
          else
          {
            ec = std::error_code{errno, std::generic_category()};
            ret_val = false;
          }
        }
      }

      ::close(storage_fd);
    }
    else
    {
      ec = std::error_code{errno, std::generic_category()};
      ret_val = false;
    }

    return ret_val;
  }

  // TODO:: Remove
  [[nodiscard]] static bool is_created(std::string const& unique_id,
                                       std::filesystem::path const& path_base, std::error_code& ec)
  {
    std::filesystem::path shm_file_base = path_base / unique_id;
    shm_file_base.replace_extension(".ready");
    return std::filesystem::exists(shm_file_base, ec);
  }

  /**
   * @brief Removes shared memory files associated with a unique identifier.
   *
   * This function removes shared memory files (storage, metadata, ready, lock)
   * associated with a specified unique identifier.
   *
   * @param unique_id The unique identifier used to identify the shared memory files.
   * @param path_base The path within shared memory.
   * @param error_code out-parameter used for error-reporting
   *
   * @return true for success or false for failure
   */
  [[nodiscard]] static bool remove_shm_files(std::string const& unique_id,
                                             std::filesystem::path const& path_base, std::error_code& ec) noexcept
  {
    bool ret_val{true};

    std::filesystem::path shm_file_base = path_base / unique_id;
    shm_file_base.replace_extension(".data");
    ret_val &= std::filesystem::remove(shm_file_base, ec);

    shm_file_base.replace_extension(".members");
    ret_val &= std::filesystem::remove(shm_file_base, ec);

    shm_file_base.replace_extension(".ready");
    ret_val &= std::filesystem::remove(shm_file_base, ec);

    shm_file_base.replace_extension(".lock");
    ret_val &= std::filesystem::remove(shm_file_base, ec);

    return ret_val;
  }

  /**
   * Checks if the process that created the queue is currently running by attempting to acquire
   * a file lock.
   * @return true if the creator process is running and the file lock is unavailable
   *         false if the creator process is not running and the lock is acquired successfully
   */
  [[nodiscard]] bool is_creator_process_running(std::error_code& ec) const noexcept
  {
    bool is_running;

    // need a different ec for the file_lock not the one that we are reporting to the user
    std::error_code file_lock_ec;
    if (lock_file(_filelock_fd, file_lock_ec))
    {
      // if we can lock the file, then the process is not running
      is_running = false;

      // we also want to unlock the file in case we repeat the check
      if (!unlock_file(_filelock_fd, file_lock_ec))
      {
        ec = std::error_code{errno, std::generic_category()};
      }
    }
    else if (file_lock_ec.value() == EWOULDBLOCK)
    {
      // the file is still locked
      is_running = true;
    }
    else
    {
      ec = std::error_code{errno, std::generic_category()};
    }

    // user needs to check !ec first before accessing the result
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
  BITLOG_ALWAYS_INLINE [[nodiscard]] uint8_t* prepare_write(integer_type n) noexcept
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
  BITLOG_ALWAYS_INLINE void finish_write(integer_type n) noexcept { _metadata->writer_pos += n; }

  /**
   * @brief Commits the finished write operation in the shared memory.
   *
   * This function sets an atomic flag to indicate that the written data is available for reading.
   * It also performs cache-related operations for optimization if the architecture allows.
   */
  BITLOG_ALWAYS_INLINE void commit_write() noexcept
  {
    // set the atomic flag so the reader can see write
    _metadata->atomic_writer_pos.store(_metadata->writer_pos, std::memory_order_release);

#if defined(BITLOG_X86_ARCHITECTURE_ENABLED)
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
  BITLOG_ALWAYS_INLINE [[nodiscard]] uint8_t* prepare_read() noexcept
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
  BITLOG_ALWAYS_INLINE void finish_read(integer_type n) noexcept { _metadata->reader_pos += n; }

  /**
   * @brief Commits the finished read operation in the shared memory.
   *
   * This function updates the atomic reader position and performs cache-related operations
   * for optimization, if applicable based on the architecture.
   */
  BITLOG_ALWAYS_INLINE void commit_read() noexcept
  {
    if (static_cast<integer_type>(_metadata->reader_pos - _metadata->atomic_reader_pos.load(std::memory_order_relaxed)) >=
        _metadata->bytes_per_batch)
    {
      _metadata->atomic_reader_pos.store(_metadata->reader_pos, std::memory_order_release);

#if defined(BITLOG_X86_ARCHITECTURE_ENABLED)
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
   * @param ec out-parameter for error reporting
   * @return true if successful, false otherwise.
   */
  [[nodiscard]] bool _memory_map_storage(int fd, size_t size, MemoryPageSize memory_page_size,
                                         std::error_code& ec) noexcept
  {
    // ask mmap for a good address where we can put both virtual copies of the buffer
    auto storage_address =
      static_cast<uint8_t*>(::mmap(nullptr, 2 * size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));

    if (storage_address == MAP_FAILED)
    {
      ec = std::error_code{errno, std::generic_category()};
      return false;
    }

    // map first region
    if (auto storage_address_1 = ::mmap(storage_address, size, PROT_READ | PROT_WRITE,
                                        MAP_FIXED | resolve_mmap_flags(memory_page_size), fd, 0);
        (storage_address_1 == MAP_FAILED) || (storage_address_1 != storage_address))
    {
      ::munmap(storage_address, 2 * size);
      ec = std::error_code{errno, std::generic_category()};
      return false;
    }

    // map second region
    if (auto storage_address_2 = ::mmap(storage_address + size, size, PROT_READ | PROT_WRITE,
                                        MAP_FIXED | resolve_mmap_flags(memory_page_size), fd, 0);
        (storage_address_2 == MAP_FAILED) || (storage_address_2 != storage_address + size))
    {
      ::munmap(storage_address, 2 * size);
      ec = std::error_code{errno, std::generic_category()};
      return false;
    }

    _storage = storage_address;

    return true;
  }

  /**
   * @brief Maps the metadata into memory.
   *
   * Maps the metadata into memory using mmap and sets the metadata address.
   *
   * @param fd The file descriptor for the metadata.
   * @param size The size of the metadata.
   * @param memory_page_size The size of memory pages.
   * @param ec out-parameter for error reporting
   * @return true if successful, false otherwise.
   */
  [[nodiscard]] bool _memory_map_metadata(int fd, size_t size, MemoryPageSize memory_page_size,
                                          std::error_code& ec) noexcept
  {
    auto metadata_address =
      ::mmap(nullptr, size, PROT_READ | PROT_WRITE, resolve_mmap_flags(memory_page_size), fd, 0);

    if (metadata_address == MAP_FAILED)
    {
      ec = std::error_code{errno, std::generic_category()};
      return false;
    }

    // do not use std::construct_at(), it will perform value initialization
    _metadata = static_cast<Metadata*>(metadata_address);

    return true;
  }

#if defined(BITLOG_X86_ARCHITECTURE_ENABLED)
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
  uint8_t* _storage{nullptr};

  // local variables - not stored in shm
  int _filelock_fd{-1};
};

using BoundedQueue = BoundedQueueImpl<uint64_t, false>;
using BoundedQueueX86 = BoundedQueueImpl<uint64_t, true>;

} // namespace bitlog::detail