import bitlog;

#include <iostream>
#include <filesystem>
#include <system_error>
#include <thread>

using namespace bitlog;

int main()
{
  std::filesystem::remove("/dev/shm/testlol.storage");
  std::filesystem::remove("/dev/shm/testlol.metadata");
  std::filesystem::remove("/dev/shm/testlol.ready");
  std::filesystem::remove("/dev/shm/testlol.lock");

  BoundedQueue q {};
  std::error_code res = q.create(4096, std::filesystem::path{}, "testlol");

  if (res)
  {
    std::cout << " error " << res.message() << std::endl;
    return 0;
  }

  std::cout << " lol " << std::endl;

  size_t i = 0;
  while(true)
  {
    std::byte* buffer = q.prepare_write(sizeof(size_t));

    if (buffer)
    {
      ++i;
      std::memcpy(buffer, &i, sizeof(size_t));
      q.finish_write(sizeof(size_t));
      q.commit_write();

      std::cout << "pushed item " << i << std::endl;
    }

    std::this_thread::sleep_for(std::chrono::seconds{1});
  }

    return 0;
}