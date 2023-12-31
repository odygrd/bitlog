#include "doctest.h"

#include <filesystem>
#include <array>
#include <system_error>
#include <thread>
#include <expected>

import bitlog;

TEST_SUITE_BEGIN("BoundedQueue");

template<typename TQueue>
void bounded_queue_read_write_test(std::string const& unique_id)
{
  std::error_code res;
  std::array<uint32_t, 8> values{};

  {
    TQueue queue;

    uint32_t const queue_capacity = 4096;
    res = queue.create(queue_capacity, std::filesystem::path{}, unique_id);

    REQUIRE_EQ(res.value(), 0);

    for (uint32_t i = 0; i < queue_capacity * 25u; ++i)
    {
      values.fill(i);

      // Read and write twice
      for (uint32_t j = 0; j < 2; ++j)
      {
        {
          std::byte* write_buf = queue.prepare_write(32u);
          REQUIRE_NE(write_buf, nullptr);
          std::memcpy(write_buf, values.data(), 32u);
          queue.finish_write(32u);
          queue.commit_write();
        }
      }

      for (uint32_t j = 0; j < 2; ++j)
      {
        {
          std::byte* read_buf = queue.prepare_read();
          REQUIRE(read_buf);
          std::array<uint32_t, 8> values_check{};
          std::memcpy(values_check.data(), read_buf, 32u);
          for (auto const val : values_check)
          {
            REQUIRE_EQ(val, i);
          }
          queue.finish_read(32u);
          queue.commit_read();
        }
      }

      REQUIRE_FALSE(queue.prepare_read());
    }

    REQUIRE(queue.is_creator_process_running());
    REQUIRE_FALSE(queue.prepare_read());
  }

  res = TQueue::remove_shm_files(unique_id, std::filesystem::path{});
  REQUIRE_EQ(res.value(), 0);
}

TEST_CASE("bounded_queue_read_write_1")
{
  static std::string const unique_id = "bounded_queue_read_write_test_1";
  bounded_queue_read_write_test<bitlog::BoundedQueueImpl<uint16_t, false>>(unique_id);
}

TEST_CASE("bounded_queue_read_write_2")
{
  static std::string const unique_id = "bounded_queue_read_write_test_2";
  bounded_queue_read_write_test<bitlog::BoundedQueueImpl<uint16_t, true>>(unique_id);
}

template<typename TQueue>
void bounded_queue_read_write_threads(std::string const& unique_id)
{
  std::thread producer_thread(
    [&unique_id]()
    {
      TQueue queue;
      uint32_t const queue_capacity = 131'072;
      std::error_code res = queue.create(queue_capacity, std::filesystem::path{}, unique_id);

      REQUIRE_EQ(res.value(), 0);

      for (uint32_t wrap_cnt = 0; wrap_cnt < 20; ++wrap_cnt)
      {
        for (uint32_t i = 0; i < 8192; ++i)
        {
          std::byte* write_buffer = queue.prepare_write(sizeof(uint32_t));

          while (!write_buffer)
          {
            std::this_thread::sleep_for(std::chrono::microseconds{2});
            write_buffer = queue.prepare_write(sizeof(uint32_t));
          }

          std::memcpy(write_buffer, &i, sizeof(uint32_t));
          queue.finish_write(sizeof(uint32_t));
          queue.commit_write();
        }
      }
    });

  std::thread consumer_thread(
    [&unique_id]()
    {
      TQueue queue;

      // wait until the producer creates the queue
      std::expected<bool, std::error_code> queue_created = TQueue::is_created(unique_id, std::filesystem::path{});
      REQUIRE(queue_created.has_value());
      while (!queue_created.value())
      {
        // keep waiting
        std::this_thread::sleep_for(std::chrono::nanoseconds{100});
        queue_created = TQueue::is_created(unique_id, std::filesystem::path{});
        REQUIRE(queue_created.has_value());
      }

      std::error_code res = queue.open(unique_id, std::filesystem::path{});
      REQUIRE_EQ(res.value(), 0);

      for (uint32_t wrap_cnt = 0; wrap_cnt < 20; ++wrap_cnt)
      {
        for (uint32_t i = 0; i < 8192; ++i)
        {
          std::byte const* read_buffer = queue.prepare_read();
          while (!read_buffer)
          {
            std::this_thread::sleep_for(std::chrono::microseconds{2});
            read_buffer = queue.prepare_read();
          }

          auto const value = reinterpret_cast<uint32_t const*>(read_buffer);
          REQUIRE_EQ(*value, i);
          queue.finish_read(sizeof(uint32_t));
          queue.commit_read();
        }
      }

      // We expect the producer to finish here and we will check it
      size_t const retries = 5;
      size_t cnt{0};
      std::expected<bool, std::error_code> is_producer_running;
      do
      {
        is_producer_running = queue.is_creator_process_running();
        ++cnt;
        // while error or producer still running retry
      } while (!is_producer_running.has_value() || *is_producer_running || (cnt > retries));

      REQUIRE(is_producer_running.has_value());
      REQUIRE_FALSE(*is_producer_running);

      res = TQueue::remove_shm_files(unique_id, std::filesystem::path{});
      REQUIRE_EQ(res.value(), 0);
    });

  producer_thread.join();
  consumer_thread.join();
}

TEST_CASE("bounded_queue_read_write_threads_1")
{
  static std::string const unique_id = "bounded_queue_read_write_threads_test_1";
  bounded_queue_read_write_threads<bitlog::BoundedQueue>(unique_id);
}

TEST_CASE("bounded_queue_read_write_threads_2")
{
  static std::string const unique_id = "bounded_queue_read_write_threads_test_2";
  bounded_queue_read_write_threads<bitlog::BoundedQueueX86>(unique_id);
}

TEST_SUITE_END();