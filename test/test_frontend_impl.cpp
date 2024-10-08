#include "bundled/doctest.h"

#include "bitlog/frontend.h"

using namespace bitlog;
using namespace bitlog::detail;

using frontend_options_t =
  FrontendOptions<QueuePolicyOption::BoundedBlocking, QueueTypeOption::Default, true>;
using frontend_manager_t = FrontendManager<frontend_options_t>;
using logger_t = Logger<frontend_options_t>;

TEST_SUITE_BEGIN("FrontendImpl");

TEST_CASE("create_get_logger_and_log")
{
  frontend_manager_t frontend_manager {"create_get_logger_and_log"};

  auto sink = frontend_manager.console_sink();
  LoggerOptions lo{};

  logger_t* test_logger = frontend_manager.create_logger("test_create_get_logger", *sink, lo);

  REQUIRE_EQ(test_logger->name(), "test_create_get_logger");
  REQUIRE_EQ(test_logger->id, 0);
  REQUIRE_EQ(test_logger->log_level(), LogLevel::Info);

  // change the log level
  test_logger->log_level(LogLevel::Debug);
  REQUIRE_EQ(test_logger->log_level(), LogLevel::Debug);

  // Request the same logger
  logger_t* logger = frontend_manager.create_logger("test_create_get_logger", *sink, lo);

  REQUIRE_EQ(logger->name(), "test_create_get_logger");
  REQUIRE_EQ(logger->id, 0);
  REQUIRE_EQ(logger->log_level(), LogLevel::Debug);

  uint32_t const a = 32;
  double b = 3.14;
  char const* c = "test_create_get_logger";
  logger->template log<__FILE_NAME__, __FUNCTION__, __LINE__, LogLevel::Info,
                       "test log {} {} {}">(a, b, c);

  std::filesystem::remove_all(frontend_manager.application_dir());
}

TEST_SUITE_END();