#include "bundled/doctest.h"

#include "bitlog/core.h"

TEST_SUITE_BEGIN("BitlogSingleton");

TEST_CASE("initialise_bitlog_singleton")
{
  using bitlog_options_t = bitlog::BitlogOptions<bitlog::QueueType::BoundedBlocking, true>;
  using bitlog_t = bitlog::Bitlog<bitlog_options_t>;

  bitlog_options_t bitlog_options;
  bitlog_options.queue_capacity_bytes = 512u;
  REQUIRE(bitlog_t::init("bitlog_singleton_test", bitlog_options));
  REQUIRE_FALSE(bitlog_t::init("bitlog_singleton_test", bitlog_options));
  REQUIRE_FALSE(bitlog_t::init("bitlog_singleton_test", bitlog_options));

  REQUIRE_EQ(bitlog_t::instance().config().queue_capacity_bytes, 512u);
  REQUIRE(bitlog_t::instance().run_dir().parent_path().string().ends_with("bitlog_singleton_test"));
  REQUIRE(bitlog_t::instance().application_dir().string().ends_with("bitlog_singleton_test"));
}

TEST_SUITE_END();