#include "bundled/doctest.h"

#include "bitlog/backend/backend_impl.h"
#include "bitlog/common/common.h"

#include <fstream>
#include <iostream>

TEST_SUITE_BEGIN("BackendImpl");

TEST_CASE("discover_ready_queues")
{
  std::error_code ec;
  std::filesystem::path const run_dir_base = bitlog::detail::resolve_base_dir(ec);
  REQUIRE_FALSE(ec);

  std::filesystem::path const application_dir = run_dir_base / "discover_ready_queues";
  std::filesystem::path const run_dir = application_dir / "1709170671490534294";
  std::filesystem::remove_all(application_dir);

  std::vector<std::pair<uint32_t, uint32_t>> ready_queues_vec;

  // First run on empty dir
  REQUIRE(bitlog::detail::discover_queues(run_dir, ready_queues_vec, ec));
  REQUIRE(ready_queues_vec.empty());

  // Add an empty application directory and retry
  std::filesystem::create_directory(application_dir);
  REQUIRE(bitlog::detail::discover_queues(run_dir, ready_queues_vec, ec));
  REQUIRE(ready_queues_vec.empty());

  // Add an empty run directory and retry
  std::filesystem::create_directory(run_dir);
  REQUIRE(bitlog::detail::discover_queues(run_dir, ready_queues_vec, ec));
  REQUIRE(ready_queues_vec.empty());

  REQUIRE_EQ(bitlog::detail::find_next_queue_sequence(0, 0, ready_queues_vec), std::optional<uint32_t>{});

  auto create_file = [&run_dir](std::string_view filename)
  {
    std::ofstream file{run_dir / filename, std::ios::out};
    REQUIRE(file.is_open());
  };

  create_file("log-statements-metadata.yaml");
  create_file("loggers-metadata.yaml");
  create_file("0.0.data");
  create_file("0.0.members");
  create_file("0.0.lock");

  // Check again - note .ready file is missing
  REQUIRE(bitlog::detail::discover_queues(run_dir, ready_queues_vec, ec));
  REQUIRE(ready_queues_vec.empty());

  create_file("0.0.ready");

  // Check again after the .ready file
  REQUIRE(bitlog::detail::discover_queues(run_dir, ready_queues_vec, ec));
  REQUIRE_EQ(ready_queues_vec.size(), 1);
  REQUIRE_EQ(ready_queues_vec[0].first, 0);
  REQUIRE_EQ(ready_queues_vec[0].second, 0);

  REQUIRE_EQ(bitlog::detail::find_next_queue_sequence(0, 0, ready_queues_vec), std::optional<uint32_t>{});

  std::filesystem::remove_all(application_dir);
}

TEST_CASE("discover_update_active_queues")
{
  using queue_info_t = bitlog::detail::QueueInfo<bitlog::detail::BoundedQueue>;
  std::vector<queue_info_t> active_queues;

  std::error_code ec;
  std::filesystem::path const run_dir_base = bitlog::detail::resolve_base_dir(ec);
  REQUIRE_FALSE(ec);

  std::filesystem::path const application_dir = run_dir_base / "discover_update_active_queues";
  std::filesystem::path const run_dir = application_dir / "1709170671490534294";
  std::filesystem::remove_all(application_dir);

  // crete the run dir
  std::filesystem::create_directories(run_dir);

  std::vector<std::pair<uint32_t, uint32_t>> ready_queues_vec;

  // Check empty
  REQUIRE(bitlog::detail::discover_queues(run_dir, ready_queues_vec, ec));
  REQUIRE(ready_queues_vec.empty());

  REQUIRE_EQ(bitlog::detail::find_next_queue_sequence(0, 0, ready_queues_vec), std::optional<uint32_t>{});

  // Create first queue
  bitlog::detail::update_active_queue_infos(active_queues, ready_queues_vec, run_dir);
  REQUIRE(active_queues.empty());

  bitlog::detail::BoundedQueue queue_1;
  REQUIRE(queue_1.create(run_dir / fmtbitlog::format("{}.{}.ext", 0, 0), 4096, bitlog::MemoryPageSize::RegularPage, 5, ec));

  // Check
  REQUIRE(bitlog::detail::discover_queues(run_dir, ready_queues_vec, ec));
  REQUIRE_EQ(ready_queues_vec.size(), 1);
  REQUIRE_EQ(ready_queues_vec[0].first, 0);
  REQUIRE_EQ(ready_queues_vec[0].second, 0);

  REQUIRE_EQ(bitlog::detail::find_next_queue_sequence(0, 0, ready_queues_vec), std::optional<uint32_t>{});

  bitlog::detail::update_active_queue_infos(active_queues, ready_queues_vec, run_dir);
  REQUIRE_EQ(active_queues.size(), 1);
  REQUIRE_EQ(active_queues[0].thread_num, 0);
  REQUIRE_EQ(active_queues[0].sequence, 0);

  // Add another queue different thread
  bitlog::detail::BoundedQueue queue_2;
  REQUIRE(queue_2.create(run_dir / fmtbitlog::format("{}.{}.ext", 1, 0), 4096, bitlog::MemoryPageSize::RegularPage, 5, ec));

  // Check again
  REQUIRE(bitlog::detail::discover_queues(run_dir, ready_queues_vec, ec));
  REQUIRE_EQ(ready_queues_vec.size(), 2);
  REQUIRE_EQ(ready_queues_vec[0].first, 0);
  REQUIRE_EQ(ready_queues_vec[0].second, 0);
  REQUIRE_EQ(ready_queues_vec[1].first, 1);
  REQUIRE_EQ(ready_queues_vec[1].second, 0);

  REQUIRE_EQ(bitlog::detail::find_next_queue_sequence(0, 0, ready_queues_vec), std::optional<uint32_t>{});
  REQUIRE_EQ(bitlog::detail::find_next_queue_sequence(1, 0, ready_queues_vec), std::optional<uint32_t>{});

  bitlog::detail::update_active_queue_infos(active_queues, ready_queues_vec, run_dir);
  REQUIRE_EQ(active_queues.size(), 2);
  REQUIRE_EQ(active_queues[0].thread_num, 0);
  REQUIRE_EQ(active_queues[0].sequence, 0);
  REQUIRE_EQ(active_queues[1].thread_num, 1);
  REQUIRE_EQ(active_queues[1].sequence, 0);

  // Add another queue with a new sequence, previous thread
  bitlog::detail::BoundedQueue queue_3;
  REQUIRE(queue_3.create(run_dir / fmtbitlog::format("{}.{}.ext", 0, 1), 4096, bitlog::MemoryPageSize::RegularPage, 5, ec));

  // Check again
  REQUIRE(bitlog::detail::discover_queues(run_dir, ready_queues_vec, ec));
  REQUIRE_EQ(ready_queues_vec.size(), 3);
  REQUIRE_EQ(ready_queues_vec[0].first, 0);
  REQUIRE_EQ(ready_queues_vec[0].second, 0);
  REQUIRE_EQ(ready_queues_vec[1].first, 0);
  REQUIRE_EQ(ready_queues_vec[1].second, 1);
  REQUIRE_EQ(ready_queues_vec[2].first, 1);
  REQUIRE_EQ(ready_queues_vec[2].second, 0);

  REQUIRE_EQ(bitlog::detail::find_next_queue_sequence(0, 0, ready_queues_vec).value(), 1);

  // Active queues should only contain two queues, one for each thread
  bitlog::detail::update_active_queue_infos(active_queues, ready_queues_vec, run_dir);
  REQUIRE_EQ(active_queues.size(), 2);
  REQUIRE_EQ(active_queues[0].thread_num, 0);
  REQUIRE_EQ(active_queues[0].sequence, 1);
  REQUIRE_EQ(active_queues[1].thread_num, 1);
  REQUIRE_EQ(active_queues[1].sequence, 0);

  // Add another queue with a new sequence, previous thread
  bitlog::detail::BoundedQueue queue_4;
  REQUIRE(queue_4.create(run_dir / fmtbitlog::format("{}.{}.ext", 0, 2), 4096, bitlog::MemoryPageSize::RegularPage, 5, ec));

  // Check again
  REQUIRE(bitlog::detail::discover_queues(run_dir, ready_queues_vec, ec));
  REQUIRE_EQ(ready_queues_vec.size(), 3);
  REQUIRE_EQ(ready_queues_vec[0].first, 0);
  REQUIRE_EQ(ready_queues_vec[0].second, 1);
  REQUIRE_EQ(ready_queues_vec[1].first, 0);
  REQUIRE_EQ(ready_queues_vec[1].second, 2);
  REQUIRE_EQ(ready_queues_vec[2].first, 1);
  REQUIRE_EQ(ready_queues_vec[2].second, 0);

  REQUIRE_EQ(bitlog::detail::find_next_queue_sequence(0, 1, ready_queues_vec).value(), 2);
  REQUIRE_EQ(bitlog::detail::find_next_queue_sequence(0, 2, ready_queues_vec), std::optional<uint32_t>{});
  REQUIRE_EQ(bitlog::detail::find_next_queue_sequence(1, 0, ready_queues_vec), std::optional<uint32_t>{});

  // Active queues should only contain two queues, one for each thread
  bitlog::detail::update_active_queue_infos(active_queues, ready_queues_vec, run_dir);
  REQUIRE_EQ(active_queues.size(), 2);
  REQUIRE_EQ(active_queues[0].thread_num, 0);
  REQUIRE_EQ(active_queues[0].sequence, 2);
  REQUIRE_EQ(active_queues[1].thread_num, 1);
  REQUIRE_EQ(active_queues[1].sequence, 0);

  // Check again
  REQUIRE(bitlog::detail::discover_queues(run_dir, ready_queues_vec, ec));
  REQUIRE_EQ(ready_queues_vec.size(), 2);
  REQUIRE_EQ(ready_queues_vec[0].first, 0);
  REQUIRE_EQ(ready_queues_vec[0].second, 2);
  REQUIRE_EQ(ready_queues_vec[1].first, 1);
  REQUIRE_EQ(ready_queues_vec[1].second, 0);

  REQUIRE_EQ(bitlog::detail::find_next_queue_sequence(0, 2, ready_queues_vec), std::optional<uint32_t>{});
  REQUIRE_EQ(bitlog::detail::find_next_queue_sequence(1, 0, ready_queues_vec), std::optional<uint32_t>{});

  bitlog::detail::update_active_queue_infos(active_queues, ready_queues_vec, run_dir);
  REQUIRE_EQ(active_queues.size(), 2);
  REQUIRE_EQ(active_queues[0].thread_num, 0);
  REQUIRE_EQ(active_queues[0].sequence, 2);
  REQUIRE_EQ(active_queues[1].thread_num, 1);
  REQUIRE_EQ(active_queues[1].sequence, 0);

  // Add another queue different thread
  bitlog::detail::BoundedQueue queue_5;
  REQUIRE(queue_5.create(run_dir / fmtbitlog::format("{}.{}.ext", 2, 0), 4096, bitlog::MemoryPageSize::RegularPage, 5, ec));

  // Check again
  REQUIRE(bitlog::detail::discover_queues(run_dir, ready_queues_vec, ec));
  REQUIRE_EQ(ready_queues_vec.size(), 3);
  REQUIRE_EQ(ready_queues_vec[0].first, 0);
  REQUIRE_EQ(ready_queues_vec[0].second, 2);
  REQUIRE_EQ(ready_queues_vec[1].first, 1);
  REQUIRE_EQ(ready_queues_vec[1].second, 0);
  REQUIRE_EQ(ready_queues_vec[2].first, 2);
  REQUIRE_EQ(ready_queues_vec[2].second, 0);

  REQUIRE_EQ(bitlog::detail::find_next_queue_sequence(0, 2, ready_queues_vec), std::optional<uint32_t>{});
  REQUIRE_EQ(bitlog::detail::find_next_queue_sequence(1, 0, ready_queues_vec), std::optional<uint32_t>{});
  REQUIRE_EQ(bitlog::detail::find_next_queue_sequence(2, 0, ready_queues_vec), std::optional<uint32_t>{});

  bitlog::detail::update_active_queue_infos(active_queues, ready_queues_vec, run_dir);
  REQUIRE_EQ(active_queues.size(), 3);
  REQUIRE_EQ(active_queues[0].thread_num, 0);
  REQUIRE_EQ(active_queues[0].sequence, 2);
  REQUIRE_EQ(active_queues[1].thread_num, 1);
  REQUIRE_EQ(active_queues[1].sequence, 0);
  REQUIRE_EQ(active_queues[2].thread_num, 2);
  REQUIRE_EQ(active_queues[2].sequence, 0);

  std::filesystem::remove_all(application_dir);
}

TEST_SUITE_END();