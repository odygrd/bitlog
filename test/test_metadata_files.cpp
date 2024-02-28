#include "bundled/doctest.h"

#include "bitlog/backend/backend_impl.h"
#include "bitlog/frontend.h"
#include "bitlog/frontend/frontend_impl.h"

using frontend_options_t = bitlog::FrontendOptions<bitlog::QueueType::BoundedBlocking, true>;
using frontend_manager_t = bitlog::FrontendManager<frontend_options_t>;
using logger_t = bitlog::Logger<frontend_options_t>;

TEST_SUITE_BEGIN("MetadataFiles");

TEST_CASE("create_read_log_statement_metadata")
{
  {
    REQUIRE_EQ(bitlog::detail::marco_metadata_node<"test_macro_metadata.cpp", "macro_metadata_node_1", 32, bitlog::LogLevel::Info, "hello {} {} {}", int, long int, double>.id, 0);
    REQUIRE_EQ(bitlog::detail::marco_metadata_node<"test_macro_metadata.cpp",  "macro_metadata_node_1", 345, bitlog::LogLevel::Debug, "foo {} {}", int, long int>.id, 1);
    REQUIRE_EQ(bitlog::detail::marco_metadata_node<"test_macro_metadata.cpp",  "macro_metadata_node_1", 1000, bitlog::LogLevel::Critical, "test">.id, 2);
  }

  // Write all metadata to a file
  frontend_manager_t frontend_manager{"create_read_log_statement_metadata_test"};

  // Read the file
  auto const [log_statements_metadata_vec, process_id] =
    bitlog::detail::read_log_statement_metadata_file(frontend_manager.run_dir());

  REQUIRE_EQ(process_id, std::to_string(::getpid()));
  REQUIRE_EQ(log_statements_metadata_vec.size(), 3);

  REQUIRE_EQ(log_statements_metadata_vec[0].line, "32");
  REQUIRE_EQ(log_statements_metadata_vec[0].level, bitlog::LogLevel::Info);
  REQUIRE_EQ(log_statements_metadata_vec[0].log_format, "hello {} {} {}");
  REQUIRE_EQ(log_statements_metadata_vec[0].function, "macro_metadata_node_1");
  REQUIRE_EQ(log_statements_metadata_vec[0].file, "test_macro_metadata.cpp");
  REQUIRE_EQ(log_statements_metadata_vec[0].type_descriptors.size(), 3);
  REQUIRE_EQ(log_statements_metadata_vec[0].type_descriptors[0], bitlog::detail::TypeDescriptorName::Int);
  REQUIRE_EQ(log_statements_metadata_vec[0].type_descriptors[1], bitlog::detail::TypeDescriptorName::LongInt);
  REQUIRE_EQ(log_statements_metadata_vec[0].type_descriptors[2], bitlog::detail::TypeDescriptorName::Double);

  REQUIRE_EQ(log_statements_metadata_vec[1].line, "345");
  REQUIRE_EQ(log_statements_metadata_vec[1].level, bitlog::LogLevel::Debug);
  REQUIRE_EQ(log_statements_metadata_vec[1].log_format, "foo {} {}");
  REQUIRE_EQ(log_statements_metadata_vec[1].function, "macro_metadata_node_1");
  REQUIRE_EQ(log_statements_metadata_vec[1].file, "test_macro_metadata.cpp");
  REQUIRE_EQ(log_statements_metadata_vec[1].type_descriptors.size(), 2);
  REQUIRE_EQ(log_statements_metadata_vec[1].type_descriptors[0], bitlog::detail::TypeDescriptorName::Int);
  REQUIRE_EQ(log_statements_metadata_vec[1].type_descriptors[1], bitlog::detail::TypeDescriptorName::LongInt);

  REQUIRE_EQ(log_statements_metadata_vec[2].line, "1000");
  REQUIRE_EQ(log_statements_metadata_vec[2].level, bitlog::LogLevel::Critical);
  REQUIRE_EQ(log_statements_metadata_vec[2].log_format, "test");
  REQUIRE_EQ(log_statements_metadata_vec[2].function, "macro_metadata_node_1");
  REQUIRE_EQ(log_statements_metadata_vec[2].file, "test_macro_metadata.cpp");
  REQUIRE_EQ(log_statements_metadata_vec[2].type_descriptors.size(), 0);

  std::filesystem::remove_all(frontend_manager.application_dir());
}

TEST_CASE("create_read_loggers_metadata")
{
  frontend_manager_t frontend_manager{"create_read_loggers_metadata_test"};

  // File has no loggers yet
  std::vector<bitlog::detail::LoggerMetadata> logger_metadata;
  logger_metadata = bitlog::detail::read_loggers_metadata_file(frontend_manager.run_dir());
  REQUIRE_EQ(logger_metadata.size(), 0);

  // Add a logger
  logger_t* logger_main = frontend_manager.logger("logger_main");
  REQUIRE_EQ(logger_main->name(), "logger_main");
  REQUIRE_EQ(logger_main->id, 0);

  // Check loggers
  logger_metadata = bitlog::detail::read_loggers_metadata_file(frontend_manager.run_dir());
  REQUIRE_EQ(logger_metadata.size(), 1);
  REQUIRE_EQ(logger_metadata[0].name, "logger_main");

  // Append another logger
  logger_t* another_logger = frontend_manager.logger("another_logger");
  REQUIRE_EQ(another_logger->name(), "another_logger");
  REQUIRE_EQ(another_logger->id, 1);

  // Check loggers
  logger_metadata = bitlog::detail::read_loggers_metadata_file(frontend_manager.run_dir());
  REQUIRE_EQ(logger_metadata.size(), 2);
  REQUIRE_EQ(logger_metadata[0].name, "logger_main");
  REQUIRE_EQ(logger_metadata[1].name, "another_logger");

  // Try to add existing logger
  logger_t* existing_logger = frontend_manager.logger("logger_main");
  REQUIRE_EQ(existing_logger->name(), "logger_main");
  REQUIRE_EQ(existing_logger->id, 0);

  // Check loggers - nothing is appended
  logger_metadata = bitlog::detail::read_loggers_metadata_file(frontend_manager.run_dir());
  REQUIRE_EQ(logger_metadata.size(), 2);
  REQUIRE_EQ(logger_metadata[0].name, "logger_main");
  REQUIRE_EQ(logger_metadata[1].name, "another_logger");

  std::filesystem::remove_all(frontend_manager.application_dir());
}
TEST_SUITE_END();