#include "bundled/doctest.h"

#include "bitlog/frontend.h"

using namespace bitlog;
using namespace bitlog::detail;

using frontend_options_t = FrontendOptions<QueuePolicyOption::BoundedBlocking, QueueTypeOption::Default, true>;
using frontend_t = Frontend<frontend_options_t>;

TEST_SUITE_BEGIN("FrontendSingleton");

TEST_CASE("initialise_frontend_singleton")
{
  frontend_options_t frontend_options;
  frontend_options.queue_capacity_bytes = 512u;
  REQUIRE(frontend_t::init("frontend_singleton_test", frontend_options));
  REQUIRE_FALSE(frontend_t::init("frontend_singleton_test", frontend_options));
  REQUIRE_FALSE(frontend_t::init("frontend_singleton_test", frontend_options));

  REQUIRE_EQ(frontend_t::instance().options().queue_capacity_bytes, 512u);
  REQUIRE(
    frontend_t::instance().run_dir().parent_path().string().ends_with("frontend_singleton_test"));
  REQUIRE(frontend_t::instance().application_dir().string().ends_with("frontend_singleton_test"));

  std::filesystem::remove_all(frontend_t::instance().application_dir());
}

TEST_SUITE_END();