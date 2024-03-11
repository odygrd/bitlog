#include "bundled/doctest.h"

#include "bitlog/backend.h"
#include "bitlog/bitlog.h" // TODO:: move macros to another header?
#include "bitlog/frontend.h"

#include "test_utils.h"

#include <thread>

using namespace bitlog;
using namespace bitlog::detail;

TEST_SUITE_BEGIN("Log");

TEST_CASE("log_to_file_simple")
{
  std::filesystem::path const output_file{"log_to_file_simple.log"};
  std::string_view test_name{"log_to_file_simple"};

  auto backend_thread = std::jthread(
    [](std::stop_token const& st)
    {
      using backend_options_t = bitlog::BackendOptions<bitlog::QueueTypeOption::Default>;
      using backend_t = bitlog::Backend<backend_options_t>;

      backend_t be{};

      while (!st.stop_requested())
      {
        be.process_application_contexts();
      }

      while (be.has_active_application_context())
      {
        be.process_application_contexts();
      }
    });

  auto frontend_thread = std::jthread(
    [&test_name, &output_file]()
    {
      using frontend_options_t =
        FrontendOptions<QueuePolicyOption::BoundedBlocking, QueueTypeOption::Default, true>;
      using frontend_manager_t = FrontendManager<frontend_options_t>;

      frontend_options_t frontend_options{};
      frontend_manager_t fm{test_name, frontend_options};

      bitlog::SinkOptions sink_options;
      sink_options.output_file_suffix(bitlog::FileSuffix::None);
      auto fsink = fm.create_file_sink(output_file.string(), sink_options);

      LoggerOptions lo{};
      auto logger = fm.create_logger("root", *fsink, lo);

      std::string const s = "adipiscing";
      LOG_INFO(logger, "Lorem ipsum dolor sit amet, consectetur {} {} {} {}", s, "elit", 1, 3.14);
      LOG_ERROR(logger,
                "Nulla tempus, libero at dignissim viverra, lectus libero finibus ante {} {}", 2, 123);
    });

  frontend_thread.join();
  backend_thread.request_stop();
  backend_thread.join();

  // TODO:: maybe use fsync ?
  std::this_thread::sleep_for(std::chrono::milliseconds{100});

  std::vector<std::string> const file_contents = bitlog::testing::file_contents(output_file);

  REQUIRE_EQ(file_contents.size(), 2);
  REQUIRE(bitlog::testing::file_contains(
    file_contents, std::string{"LOG_INFO      root         Lorem ipsum dolor sit amet, consectetur adipiscing elit 1 3.14"}));
  REQUIRE(bitlog::testing::file_contains(
    file_contents, std::string{"LOG_ERROR     root         Nulla tempus, libero at dignissim viverra, lectus libero finibus ante 2 123"}));

  std::error_code ec;
  std::filesystem::remove(output_file);
  std::filesystem::remove_all(bitlog::detail::resolve_base_dir(ec) / test_name);
}

TEST_CASE("log_to_file_reallocate_queue")
{
  std::filesystem::path const output_file{"log_to_file_reallocate_queue.log"};
  std::string_view test_name{"log_to_file_reallocate_queue"};

  auto backend_thread = std::jthread(
    [](std::stop_token const& st)
    {
      using backend_options_t = bitlog::BackendOptions<bitlog::QueueTypeOption::Default>;
      using backend_t = bitlog::Backend<backend_options_t>;

      backend_t be{};

      while (!st.stop_requested())
      {
        be.process_application_contexts();
      }

      while (be.has_active_application_context())
      {
        be.process_application_contexts();
      }
    });

  auto frontend_thread = std::jthread(
    [&test_name, &output_file]()
    {
      using frontend_options_t =
        FrontendOptions<QueuePolicyOption::UnboundedNoLimit, QueueTypeOption::Default, true>;
      using frontend_manager_t = FrontendManager<frontend_options_t>;

      frontend_options_t frontend_options{};
      frontend_options.queue_capacity_bytes = 4096;
      frontend_manager_t fm{test_name, frontend_options};

      bitlog::SinkOptions sink_options;
      sink_options.output_file_suffix(bitlog::FileSuffix::None);
      auto fsink = fm.create_file_sink(output_file.string(), sink_options);

      LoggerOptions lo{};
      auto logger = fm.create_logger("root", *fsink, lo);

      for (size_t i = 0; i < 4096; ++i)
      {
        std::string const s = "adipiscing";
        LOG_INFO(logger, "Lorem ipsum dolor sit amet, consectetur {} {} {} {}", s, "elit", i, 3.14);
        LOG_ERROR(logger,
                  "Nulla tempus, libero at dignissim viverra, lectus libero finibus ante {} {}", i, 123);
      }
    });

  frontend_thread.join();
  backend_thread.request_stop();
  backend_thread.join();

  // TODO:: maybe use fsync ?
  std::this_thread::sleep_for(std::chrono::milliseconds{100});

  std::vector<std::string> const file_contents = bitlog::testing::file_contents(output_file);

  REQUIRE_EQ(file_contents.size(), 4096 * 2);

  std::error_code ec;
  std::filesystem::remove(output_file);
  std::filesystem::remove_all(bitlog::detail::resolve_base_dir(ec) / test_name);
}

TEST_SUITE_END();