#include <filesystem>
#include <iostream>
#include <system_error>
#include <thread>

#include "bitlog/backend.h"
#include "bitlog/bitlog.h"

int main()
{
  std::thread backend{[]()
                      {
                        using backend_options_t = bitlog::BackendOptions<bitlog::QueueTypeOption::Default>;
                        using backend_t = bitlog::Backend<backend_options_t>;

                        backend_t be{};

                        while (true)
                        {
                          be.process_application_contexts();
                        }
                      }};

  backend.join();

  return 0;
}