#include "bundled/doctest.h"

#include "bitlog/core.h"
#include "bitlog/detail/backend.h"
#include "bitlog/detail/frontend.h"

using log_client_config_t = bitlog::Config<bitlog::detail::BoundedQueue, false>;

TEST_SUITE_BEGIN("MetadataFiles");

TEST_CASE("create_read_log_statement_metadata")
{
  {
    REQUIRE_EQ(bitlog::detail::marco_metadata_node<"test_macro_metadata.cpp", "macro_metadata_node_1", 32, bitlog::LogLevel::Info, "hello {} {} {}", int, long int, double>.id, 0);
    REQUIRE_EQ(bitlog::detail::marco_metadata_node<"test_macro_metadata.cpp",  "macro_metadata_node_1", 345, bitlog::LogLevel::Debug, "foo {} {}", int, long int>.id, 1);
    REQUIRE_EQ(bitlog::detail::marco_metadata_node<"test_macro_metadata.cpp",  "macro_metadata_node_1", 1000, bitlog::LogLevel::Critical, "test">.id, 2);
  }

  // Write all metadata to a file
  log_client_config_t log_client_config{"create_read_log_statement_metadata_test"};
  bitlog::detail::create_log_statements_metadata_file(log_client_config.instance_dir());

  // Read the file
  auto const [log_statements_metadata_vec, process_id] =
    bitlog::detail::read_log_statement_metadata_file(log_client_config.instance_dir());

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

  std::filesystem::remove_all(log_client_config.app_dir());
}

TEST_CASE("create_read_loggers_metadata")
{
  log_client_config_t log_client_config{"create_read_loggers_metadata_test"};

  // File doesn't exist
  std::vector<bitlog::detail::LoggerMetadata> logger_metadata;
  logger_metadata = bitlog::detail::read_loggers_metadata_file(log_client_config.instance_dir());
  REQUIRE_EQ(logger_metadata.size(), 0);

  // Create the file
  bitlog::detail::create_loggers_metadata_file(log_client_config.instance_dir());

  // Check loggers
  logger_metadata = bitlog::detail::read_loggers_metadata_file(log_client_config.instance_dir());
  REQUIRE_EQ(logger_metadata.size(), 0);

  // Append a logger
  bitlog::detail::append_loggers_metadata_file(log_client_config.instance_dir(), 0, "logger_main");

  // Check loggers
  logger_metadata = bitlog::detail::read_loggers_metadata_file(log_client_config.instance_dir());
  REQUIRE_EQ(logger_metadata.size(), 1);
  REQUIRE_EQ(logger_metadata[0].name, "logger_main");

  // Append another logger
  bitlog::detail::append_loggers_metadata_file(log_client_config.instance_dir(), 1, "another_logger");

  // Check loggers
  logger_metadata = bitlog::detail::read_loggers_metadata_file(log_client_config.instance_dir());
  REQUIRE_EQ(logger_metadata.size(), 2);
  REQUIRE_EQ(logger_metadata[0].name, "logger_main");
  REQUIRE_EQ(logger_metadata[1].name, "another_logger");

  std::filesystem::remove_all(log_client_config.app_dir());
}
TEST_SUITE_END();