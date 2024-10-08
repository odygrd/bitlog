#include <filesystem>
#include <iostream>
#include <system_error>
#include <thread>

#include "bitlog/backend.h"
#include "bitlog/bitlog.h"

int main()
{
  using frontend_options_t =
    bitlog::FrontendOptions<bitlog::QueuePolicyOption::BoundedBlocking, bitlog::QueueTypeOption::Default, true>;
  using frontend_t = bitlog::Frontend<frontend_options_t>;

  frontend_options_t frontend_options{};

  frontend_t::init("test_app", frontend_options);

  bitlog::SinkOptions sink_options;
  sink_options.output_file_suffix(bitlog::FileSuffix::StartDateTime);
  auto fsink = frontend_t::instance().create_file_sink(
    "/home/odygrd/CLionProjects/bitlog/output/test_file.log", sink_options);

  auto logger = frontend_t::instance().create_logger("testing", fsink);

  LOG_INFO(logger, "hello world {}", 12u);
  LOG_INFO(logger, "hello doubles {}", 123.3);

  auto logger_2 = frontend_t::instance().create_logger("another_logger", fsink);

  for (int i = 0; i < 3; ++i)
  {
    LOG_INFO(logger_2, "hello char {} loop {}", i * 100, i);
  }

  LOG_INFO(logger, "Another log doubles {}", 123.3);

  std::this_thread::sleep_for(std::chrono::seconds{30});
  return 0;
}
