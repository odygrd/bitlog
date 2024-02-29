#include "bundled/doctest.h"

#include "bitlog/backend/backend_impl.h"
#include "bitlog/common/common.h"

#include <fstream>
#include <iostream>

TEST_SUITE_BEGIN("BackendImpl");

TEST_CASE("discover_queues")
{
  std::error_code ec;
  std::filesystem::path const run_dir_base = bitlog::detail::resolve_base_dir(ec);
  REQUIRE_FALSE(ec);

  std::filesystem::path const application_dir = run_dir_base / "discover_queues_test";
  std::filesystem::path const run_dir = application_dir / "1709170671490534294";
  std::vector<std::pair<uint32_t, uint32_t>> thread_queues_vec;

  // First run on empty dir
  REQUIRE(bitlog::detail::discover_queues(run_dir, thread_queues_vec, ec));
  REQUIRE(thread_queues_vec.empty());

  // Add an empty application directory and retry
  std::filesystem::create_directory(application_dir);
  REQUIRE(bitlog::detail::discover_queues(run_dir, thread_queues_vec, ec));
  REQUIRE(thread_queues_vec.empty());

  // Add an empty run directory and retry
  std::filesystem::create_directory(run_dir);
  REQUIRE(bitlog::detail::discover_queues(run_dir, thread_queues_vec, ec));
  REQUIRE(thread_queues_vec.empty());

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
  REQUIRE(bitlog::detail::discover_queues(run_dir, thread_queues_vec, ec));
  REQUIRE(thread_queues_vec.empty());

  create_file("0.0.ready");

  // Check again
  REQUIRE(bitlog::detail::discover_queues(run_dir, thread_queues_vec, ec));
  REQUIRE_EQ(thread_queues_vec.size(), 1);
  REQUIRE_EQ(thread_queues_vec[0].first, 0);
  REQUIRE_EQ(thread_queues_vec[0].second, 0);

  // Add another queue different thread
  create_file("1.0.data");
  create_file("1.0.members");
  create_file("1.0.lock");
  create_file("1.0.ready");

  // Check again
  REQUIRE(bitlog::detail::discover_queues(run_dir, thread_queues_vec, ec));
  REQUIRE_EQ(thread_queues_vec.size(), 2);
  REQUIRE_EQ(thread_queues_vec[0].first, 0);
  REQUIRE_EQ(thread_queues_vec[0].second, 0);
  REQUIRE_EQ(thread_queues_vec[1].first, 1);
  REQUIRE_EQ(thread_queues_vec[1].second, 0);

  // Add another queue with a new sequence, previous thread
  create_file("0.1.data");
  create_file("0.1.members");
  create_file("0.1.lock");
  create_file("0.1.ready");

  // Check again
  REQUIRE(bitlog::detail::discover_queues(run_dir, thread_queues_vec, ec));
  REQUIRE_EQ(thread_queues_vec.size(), 3);
  REQUIRE_EQ(thread_queues_vec[0].first, 0);
  REQUIRE_EQ(thread_queues_vec[0].second, 0);
  REQUIRE_EQ(thread_queues_vec[1].first, 0);
  REQUIRE_EQ(thread_queues_vec[1].second, 1);
  REQUIRE_EQ(thread_queues_vec[2].first, 1);
  REQUIRE_EQ(thread_queues_vec[2].second, 0);

  // Add another queue with a new sequence, previous thread
  create_file("0.2.data");
  create_file("0.2.members");
  create_file("0.2.lock");
  create_file("0.2.ready");

  // Check again
  REQUIRE(bitlog::detail::discover_queues(run_dir, thread_queues_vec, ec));
  REQUIRE_EQ(thread_queues_vec.size(), 4);
  REQUIRE_EQ(thread_queues_vec[0].first, 0);
  REQUIRE_EQ(thread_queues_vec[0].second, 0);
  REQUIRE_EQ(thread_queues_vec[1].first, 0);
  REQUIRE_EQ(thread_queues_vec[1].second, 1);
  REQUIRE_EQ(thread_queues_vec[2].first, 0);
  REQUIRE_EQ(thread_queues_vec[2].second, 2);
  REQUIRE_EQ(thread_queues_vec[3].first, 1);
  REQUIRE_EQ(thread_queues_vec[3].second, 0);

  // Remove the previous queues
  std::filesystem::remove(run_dir / "0.0.data");
  std::filesystem::remove(run_dir / "0.0.members");
  std::filesystem::remove(run_dir / "0.0.lock");
  std::filesystem::remove(run_dir / "0.0.ready");
  std::filesystem::remove(run_dir / "0.1.data");
  std::filesystem::remove(run_dir / "0.1.members");
  std::filesystem::remove(run_dir / "0.1.lock");
  std::filesystem::remove(run_dir / "0.1.ready");

  // Check again
  REQUIRE(bitlog::detail::discover_queues(run_dir, thread_queues_vec, ec));
  REQUIRE_EQ(thread_queues_vec.size(), 2);
  REQUIRE_EQ(thread_queues_vec[0].first, 0);
  REQUIRE_EQ(thread_queues_vec[0].second, 2);
  REQUIRE_EQ(thread_queues_vec[1].first, 1);
  REQUIRE_EQ(thread_queues_vec[1].second, 0);

  std::filesystem::remove_all(application_dir);
}

TEST_SUITE_END();