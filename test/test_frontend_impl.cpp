#include "bundled/doctest.h"

#include "bitlog/frontend.h"

TEST_SUITE_BEGIN("FrontendImpl");

using frontend_options_t =
  bitlog::FrontendOptions<bitlog::QueuePolicyOption::BoundedBlocking, bitlog::QueueTypeOption::Default, true>;
using frontend_manager_t = bitlog::FrontendManager<frontend_options_t>;
using logger_t = bitlog::Logger<frontend_options_t>;

TEST_CASE("create_get_logger_and_log")
{
  frontend_manager_t frontend_manager {"create_get_logger_and_log"};
  logger_t* test_logger = frontend_manager.logger("test_create_get_logger");

  REQUIRE_EQ(test_logger->name(), "test_create_get_logger");
  REQUIRE_EQ(test_logger->id, 0);
  REQUIRE_EQ(test_logger->log_level(), bitlog::LogLevel::Info);

  // change the log level
  test_logger->log_level(bitlog::LogLevel::Debug);
  REQUIRE_EQ(test_logger->log_level(), bitlog::LogLevel::Debug);

  // Request the same logger
  logger_t* logger = frontend_manager.logger("test_create_get_logger");

  REQUIRE_EQ(logger->name(), "test_create_get_logger");
  REQUIRE_EQ(logger->id, 0);
  REQUIRE_EQ(logger->log_level(), bitlog::LogLevel::Debug);

  uint32_t const a = 32;
  double b = 3.14;
  char const* c = "test_create_get_logger";
  logger->template log<__FILE_NAME__, __FUNCTION__, __LINE__, bitlog::LogLevel::Info,
                       "test log {} {} {}">(a, b, c);

  std::filesystem::remove_all(frontend_manager.application_dir());
}

TEST_SUITE_END();