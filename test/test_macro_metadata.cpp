#include "bundled/doctest.h"

#include "bitlog/detail/frontend.h"

TEST_SUITE_BEGIN("MacroMetadata");

TEST_CASE("macro_metadata_node")
{
  // Generate some metadata
  {
    REQUIRE_EQ(bitlog::detail::marco_metadata_node<"test_macro_metadata.cpp", "macro_metadata_node_1", 32, bitlog::LogLevel::Info, "hello {} {} {}", int, long int, double>.id, 0);
    REQUIRE_EQ(bitlog::detail::marco_metadata_node<"test_macro_metadata.cpp",  "macro_metadata_node_1", 345, bitlog::LogLevel::Debug, "foo {} {}", int, long int>.id, 1);
    REQUIRE_EQ(bitlog::detail::marco_metadata_node<"test_macro_metadata.cpp",  "macro_metadata_node_1", 1000, bitlog::LogLevel::Critical, "test">.id, 2);
  }

  // Iterate through all the generated metadata
  size_t cnt{0};
  bitlog::detail::MacroMetadataNode const* cur = bitlog::detail::macro_metadata_head_node();

  while (cur)
  {
    REQUIRE_EQ(cur->macro_metadata.file, "test_macro_metadata.cpp");
    REQUIRE_EQ(cur->macro_metadata.function, "macro_metadata_node_1");

    switch (cur->id)
    {
    case 0:
      REQUIRE_EQ(cur->macro_metadata.line, 32);
      REQUIRE_EQ(cur->macro_metadata.level, bitlog::LogLevel::Info);
      REQUIRE_EQ(cur->macro_metadata.log_format, "hello {} {} {}");
      REQUIRE_EQ(cur->macro_metadata.type_descriptors.size(), 3);
      REQUIRE_EQ(cur->macro_metadata.type_descriptors[0], bitlog::detail::TypeDescriptorName::Int);
      REQUIRE_EQ(cur->macro_metadata.type_descriptors[1], bitlog::detail::TypeDescriptorName::LongInt);
      REQUIRE_EQ(cur->macro_metadata.type_descriptors[2], bitlog::detail::TypeDescriptorName::Double);
      break;
    case 1:
      REQUIRE_EQ(cur->macro_metadata.line, 345);
      REQUIRE_EQ(cur->macro_metadata.level, bitlog::LogLevel::Debug);
      REQUIRE_EQ(cur->macro_metadata.log_format, "foo {} {}");
      REQUIRE_EQ(cur->macro_metadata.type_descriptors.size(), 2);
      REQUIRE_EQ(cur->macro_metadata.type_descriptors[0], bitlog::detail::TypeDescriptorName::Int);
      REQUIRE_EQ(cur->macro_metadata.type_descriptors[1], bitlog::detail::TypeDescriptorName::LongInt);
      break;
    case 2:
      REQUIRE_EQ(cur->macro_metadata.line, 1000);
      REQUIRE_EQ(cur->macro_metadata.level, bitlog::LogLevel::Critical);
      REQUIRE_EQ(cur->macro_metadata.log_format, "test");
      REQUIRE_EQ(cur->macro_metadata.type_descriptors.size(), 0);
      break;
    }

    cur = cur->next;
    ++cnt;
  }

  REQUIRE_EQ(cnt, 3);
}

TEST_SUITE_END();