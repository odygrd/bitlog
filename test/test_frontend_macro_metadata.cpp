#include "bundled/doctest.h"

#include "bitlog/frontend/frontend_impl.h"

using namespace bitlog;
using namespace bitlog::detail;

TEST_SUITE_BEGIN("FrontendMacroMetadata");

TEST_CASE("macro_metadata_node")
{
  // Generate some metadata
  {
    REQUIRE_EQ(marco_metadata_node<"test_macro_metadata.cpp", "macro_metadata_node_1", 32, LogLevel::Info, "hello {} {} {}", int, long int, double>.id, 0);
    REQUIRE_EQ(marco_metadata_node<"test_macro_metadata.cpp",  "macro_metadata_node_1", 345, LogLevel::Debug, "foo {} {}", int, long int>.id, 1);
    REQUIRE_EQ(marco_metadata_node<"test_macro_metadata.cpp",  "macro_metadata_node_1", 1000, LogLevel::Critical, "test">.id, 2);
  }

  // Iterate through all the generated metadata
  size_t cnt{0};
  MacroMetadataNode const* cur = macro_metadata_head_node();

  while (cur)
  {
    REQUIRE_EQ(cur->macro_metadata.full_source_path, "test_macro_metadata.cpp");
    REQUIRE_EQ(cur->macro_metadata.caller_function, "macro_metadata_node_1");

    switch (cur->id)
    {
    case 0:
      REQUIRE_EQ(cur->macro_metadata.source_line, 32);
      REQUIRE_EQ(cur->macro_metadata.log_level, LogLevel::Info);
      REQUIRE_EQ(cur->macro_metadata.message_format, "hello {} {} {}");
      REQUIRE_EQ(cur->macro_metadata.type_descriptors.size(), 3);
      REQUIRE_EQ(cur->macro_metadata.type_descriptors[0], TypeDescriptorName::Int);
      REQUIRE_EQ(cur->macro_metadata.type_descriptors[1], TypeDescriptorName::LongInt);
      REQUIRE_EQ(cur->macro_metadata.type_descriptors[2], TypeDescriptorName::Double);
      break;
    case 1:
      REQUIRE_EQ(cur->macro_metadata.source_line, 345);
      REQUIRE_EQ(cur->macro_metadata.log_level, LogLevel::Debug);
      REQUIRE_EQ(cur->macro_metadata.message_format, "foo {} {}");
      REQUIRE_EQ(cur->macro_metadata.type_descriptors.size(), 2);
      REQUIRE_EQ(cur->macro_metadata.type_descriptors[0], TypeDescriptorName::Int);
      REQUIRE_EQ(cur->macro_metadata.type_descriptors[1], TypeDescriptorName::LongInt);
      break;
    case 2:
      REQUIRE_EQ(cur->macro_metadata.source_line, 1000);
      REQUIRE_EQ(cur->macro_metadata.log_level, LogLevel::Critical);
      REQUIRE_EQ(cur->macro_metadata.message_format, "test");
      REQUIRE_EQ(cur->macro_metadata.type_descriptors.size(), 0);
      break;
    }

    cur = cur->next;
    ++cnt;
  }

  REQUIRE_EQ(cnt, 3);
}

TEST_SUITE_END();