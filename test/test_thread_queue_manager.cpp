#include "bundled/doctest.h"

#include "bitlog/backend.h"
#include "bitlog/common/common.h"

#include <fstream>
#include <iostream>

using backend_options_t =
  bitlog::BackendOptions<bitlog::QueueTypeOption::Default>;

class ThreadQueueManagerMock : public bitlog::detail::ThreadQueueManager<backend_options_t>
{
public:
  using base_t = bitlog::detail::ThreadQueueManager<backend_options_t>;
  using base_t::base_t;

  [[nodiscard]] std::vector<std::pair<uint32_t, uint32_t>> const& get_discovered_queues() const noexcept
  {
    return this->_get_discovered_queues();
  }

  [[nodiscard]] bool discover_queues(std::error_code& ec) { return this->_discover_queues(ec); }

  [[nodiscard]] std::optional<uint32_t> find_next_queue_sequence(uint32_t thread_num, uint32_t sequence)
  {
    return this->_find_next_sequence(thread_num, sequence);
  }
};

TEST_SUITE_BEGIN("ThreadQueueManager");

TEST_CASE("discover_ready_queues")
{
  std::error_code ec;
  std::filesystem::path const run_dir_base = bitlog::detail::resolve_base_dir(ec);
  REQUIRE_FALSE(ec);

  std::filesystem::path const application_dir = run_dir_base / "discover_ready_queues";
  std::filesystem::path const run_dir = application_dir / "1709170671490534294";
  std::filesystem::remove_all(application_dir);

  ThreadQueueManagerMock tqm{run_dir, backend_options_t {}};

  // First run on empty dir
  REQUIRE(tqm.discover_queues(ec));
  REQUIRE(tqm.get_discovered_queues().empty());

  // Add an empty application directory and retry
  std::filesystem::create_directory(application_dir);
  REQUIRE(tqm.discover_queues(ec));
  REQUIRE(tqm.get_discovered_queues().empty());

  // Add an empty run directory and retry
  std::filesystem::create_directory(run_dir);
  REQUIRE(tqm.discover_queues(ec));
  REQUIRE(tqm.get_discovered_queues().empty());

  REQUIRE_EQ(tqm.find_next_queue_sequence(0, 0), std::optional<uint32_t>{});

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
  REQUIRE(tqm.discover_queues(ec));
  REQUIRE(tqm.get_discovered_queues().empty());

  create_file("0.0.ready");

  // Check again after the .ready file
  REQUIRE(tqm.discover_queues(ec));
  REQUIRE_EQ(tqm.get_discovered_queues().size(), 1);
  REQUIRE_EQ(tqm.get_discovered_queues()[0].first, 0);
  REQUIRE_EQ(tqm.get_discovered_queues()[0].second, 0);

  REQUIRE_EQ(tqm.find_next_queue_sequence(0, 0), std::optional<uint32_t>{});

  std::filesystem::remove_all(application_dir);
}

TEST_CASE("discover_update_active_queues")
{
  std::error_code ec;
  std::filesystem::path const run_dir_base = bitlog::detail::resolve_base_dir(ec);
  REQUIRE_FALSE(ec);

  std::filesystem::path const application_dir = run_dir_base / "discover_update_active_queues";
  std::filesystem::path const run_dir = application_dir / "1709170671490534294";
  std::filesystem::remove_all(application_dir);

  // crete the run dir
  std::filesystem::create_directories(run_dir);

  ThreadQueueManagerMock tqm{run_dir, backend_options_t {}};

  // Check empty
  REQUIRE(tqm.discover_queues(ec));
  REQUIRE(tqm.get_discovered_queues().empty());
  REQUIRE_EQ(tqm.find_next_queue_sequence(0, 0), std::optional<uint32_t>{});

  // Create first queue
  REQUIRE(tqm.discover_queues(ec));
  REQUIRE(tqm.get_discovered_queues().empty());

  bitlog::detail::BoundedQueue queue_1;
  REQUIRE(queue_1.create(run_dir / fmtbitlog::format("{}.{}.ext", 0, 0), 4096,
                         bitlog::MemoryPageSize::RegularPage, 5, ec));

  // Check
  REQUIRE(tqm.discover_queues(ec));
  REQUIRE_EQ(tqm.get_discovered_queues().size(), 1);
  REQUIRE_EQ(tqm.get_discovered_queues()[0].first, 0);
  REQUIRE_EQ(tqm.get_discovered_queues()[0].second, 0);

  REQUIRE_EQ(tqm.find_next_queue_sequence(0, 0), std::optional<uint32_t>{});

  tqm.update_active_queues();
  REQUIRE_EQ(tqm.active_queues().size(), 1);
  REQUIRE_EQ(tqm.active_queues()[0].thread_num, 0);
  REQUIRE_EQ(tqm.active_queues()[0].sequence, 0);

  // Add another queue different thread
  bitlog::detail::BoundedQueue queue_2;
  REQUIRE(queue_2.create(run_dir / fmtbitlog::format("{}.{}.ext", 1, 0), 4096,
                         bitlog::MemoryPageSize::RegularPage, 5, ec));

  // Check again
  REQUIRE(tqm.discover_queues(ec));
  REQUIRE_EQ(tqm.get_discovered_queues().size(), 2);
  REQUIRE_EQ(tqm.get_discovered_queues()[0].first, 0);
  REQUIRE_EQ(tqm.get_discovered_queues()[0].second, 0);
  REQUIRE_EQ(tqm.get_discovered_queues()[1].first, 1);
  REQUIRE_EQ(tqm.get_discovered_queues()[1].second, 0);

  REQUIRE_EQ(tqm.find_next_queue_sequence(0, 0), std::optional<uint32_t>{});
  REQUIRE_EQ(tqm.find_next_queue_sequence(1, 0), std::optional<uint32_t>{});

  tqm.update_active_queues();
  REQUIRE_EQ(tqm.active_queues().size(), 2);
  REQUIRE_EQ(tqm.active_queues()[0].thread_num, 0);
  REQUIRE_EQ(tqm.active_queues()[0].sequence, 0);
  REQUIRE_EQ(tqm.active_queues()[1].thread_num, 1);
  REQUIRE_EQ(tqm.active_queues()[1].sequence, 0);

  // Add another queue with a new sequence, previous thread
  bitlog::detail::BoundedQueue queue_3;
  REQUIRE(queue_3.create(run_dir / fmtbitlog::format("{}.{}.ext", 0, 1), 4096,
                         bitlog::MemoryPageSize::RegularPage, 5, ec));

  // Check again
  REQUIRE(tqm.discover_queues(ec));
  REQUIRE_EQ(tqm.get_discovered_queues().size(), 3);
  REQUIRE_EQ(tqm.get_discovered_queues()[0].first, 0);
  REQUIRE_EQ(tqm.get_discovered_queues()[0].second, 0);
  REQUIRE_EQ(tqm.get_discovered_queues()[1].first, 0);
  REQUIRE_EQ(tqm.get_discovered_queues()[1].second, 1);
  REQUIRE_EQ(tqm.get_discovered_queues()[2].first, 1);
  REQUIRE_EQ(tqm.get_discovered_queues()[2].second, 0);

  REQUIRE_EQ(tqm.find_next_queue_sequence(0, 0), 1);

  // Active queues should only contain two queues, one for each thread
  tqm.update_active_queues();
  REQUIRE_EQ(tqm.active_queues().size(), 2);
  REQUIRE_EQ(tqm.active_queues()[0].thread_num, 0);
  REQUIRE_EQ(tqm.active_queues()[0].sequence, 1);
  REQUIRE_EQ(tqm.active_queues()[1].thread_num, 1);
  REQUIRE_EQ(tqm.active_queues()[1].sequence, 0);

  // Add another queue with a new sequence, previous thread
  bitlog::detail::BoundedQueue queue_4;
  REQUIRE(queue_4.create(run_dir / fmtbitlog::format("{}.{}.ext", 0, 2), 4096,
                         bitlog::MemoryPageSize::RegularPage, 5, ec));

  // Check again
  REQUIRE(tqm.discover_queues(ec));
  REQUIRE_EQ(tqm.get_discovered_queues().size(), 3);
  REQUIRE_EQ(tqm.get_discovered_queues()[0].first, 0);
  REQUIRE_EQ(tqm.get_discovered_queues()[0].second, 1);
  REQUIRE_EQ(tqm.get_discovered_queues()[1].first, 0);
  REQUIRE_EQ(tqm.get_discovered_queues()[1].second, 2);
  REQUIRE_EQ(tqm.get_discovered_queues()[2].first, 1);
  REQUIRE_EQ(tqm.get_discovered_queues()[2].second, 0);

  REQUIRE_EQ(tqm.find_next_queue_sequence(0, 1), 2);
  REQUIRE_EQ(tqm.find_next_queue_sequence(0, 2), std::optional<uint32_t>{});
  REQUIRE_EQ(tqm.find_next_queue_sequence(1, 0), std::optional<uint32_t>{});

  // Active queues should only contain two queues, one for each thread
  tqm.update_active_queues();
  REQUIRE_EQ(tqm.active_queues().size(), 2);
  REQUIRE_EQ(tqm.active_queues()[0].thread_num, 0);
  REQUIRE_EQ(tqm.active_queues()[0].sequence, 2);
  REQUIRE_EQ(tqm.active_queues()[1].thread_num, 1);
  REQUIRE_EQ(tqm.active_queues()[1].sequence, 0);

  // Check again
  REQUIRE(tqm.discover_queues(ec));
  REQUIRE_EQ(tqm.get_discovered_queues().size(), 2);
  REQUIRE_EQ(tqm.get_discovered_queues()[0].first, 0);
  REQUIRE_EQ(tqm.get_discovered_queues()[0].second, 2);
  REQUIRE_EQ(tqm.get_discovered_queues()[1].first, 1);
  REQUIRE_EQ(tqm.get_discovered_queues()[1].second, 0);

  REQUIRE_EQ(tqm.find_next_queue_sequence(0, 2), std::optional<uint32_t>{});
  REQUIRE_EQ(tqm.find_next_queue_sequence(1, 0), std::optional<uint32_t>{});

  tqm.update_active_queues();
  REQUIRE_EQ(tqm.active_queues().size(), 2);
  REQUIRE_EQ(tqm.active_queues()[0].thread_num, 0);
  REQUIRE_EQ(tqm.active_queues()[0].sequence, 2);
  REQUIRE_EQ(tqm.active_queues()[1].thread_num, 1);
  REQUIRE_EQ(tqm.active_queues()[1].sequence, 0);

  // Add another queue different thread
  bitlog::detail::BoundedQueue queue_5;
  REQUIRE(queue_5.create(run_dir / fmtbitlog::format("{}.{}.ext", 2, 0), 4096,
                         bitlog::MemoryPageSize::RegularPage, 5, ec));

  // Check again
  REQUIRE(tqm.discover_queues(ec));
  REQUIRE_EQ(tqm.get_discovered_queues().size(), 3);
  REQUIRE_EQ(tqm.get_discovered_queues()[0].first, 0);
  REQUIRE_EQ(tqm.get_discovered_queues()[0].second, 2);
  REQUIRE_EQ(tqm.get_discovered_queues()[1].first, 1);
  REQUIRE_EQ(tqm.get_discovered_queues()[1].second, 0);
  REQUIRE_EQ(tqm.get_discovered_queues()[2].first, 2);
  REQUIRE_EQ(tqm.get_discovered_queues()[2].second, 0);

  REQUIRE_EQ(tqm.find_next_queue_sequence(0, 2), std::optional<uint32_t>{});
  REQUIRE_EQ(tqm.find_next_queue_sequence(1, 0), std::optional<uint32_t>{});
  REQUIRE_EQ(tqm.find_next_queue_sequence(2, 0), std::optional<uint32_t>{});

  tqm.update_active_queues();
  REQUIRE_EQ(tqm.active_queues().size(), 3);
  REQUIRE_EQ(tqm.active_queues()[0].thread_num, 0);
  REQUIRE_EQ(tqm.active_queues()[0].sequence, 2);
  REQUIRE_EQ(tqm.active_queues()[1].thread_num, 1);
  REQUIRE_EQ(tqm.active_queues()[1].sequence, 0);
  REQUIRE_EQ(tqm.active_queues()[2].thread_num, 2);
  REQUIRE_EQ(tqm.active_queues()[2].sequence, 0);

  std::filesystem::remove_all(application_dir);
}

TEST_SUITE_END();