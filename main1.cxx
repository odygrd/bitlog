#include <iostream>
#include <filesystem>
#include <system_error>
#include <thread>

#include "bitlog.h"

int main()
{
//  std::filesystem::remove("/dev/shm/testlol.storage");
//  std::filesystem::remove("/dev/shm/testlol.metadata");
//  std::filesystem::remove("/dev/shm/testlol.ready");
//  std::filesystem::remove("/dev/shm/testlol.lock");
//
//  BoundedQueue q {};
//  std::error_code res = q.create(4096, std::filesystem::path{}, "testlol");
//
//  if (res)
//  {
//    std::cout << " error " << res.message() << std::endl;
//    return 0;
//  }
//
//  std::cout << " lol " << std::endl;
//
//  size_t i = 0;
//  while(true)
//  {
//    std::byte* buffer = q.prepare_write(sizeof(size_t));
//
//    if (buffer)
//    {
//      ++i;
//      std::memcpy(buffer, &i, sizeof(size_t));
//      q.finish_write(sizeof(size_t));
//      q.commit_write();
//
//      std::cout << "pushed item " << i << std::endl;
//    }
//
//    std::this_thread::sleep_for(std::chrono::seconds{1});
//  }

const unsigned int i = 32;
unsigned int const& j = i;

LOG_INFO("test {} lol test {}", i, j);
LOG_INFO("test {} lol test {}", 12u);
LOG_INFO("test {} lol test {}", 33.21);

for (int i = 0; i < 3; ++i)
{
  LOG_INFO("test {} lol test {}", 'a');
}

    return 0;
}