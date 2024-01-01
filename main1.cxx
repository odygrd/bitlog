#include <filesystem>
#include <iostream>
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

  // print all metadata ids !
  // TODO write this to a file
  bitlog::MacroMetadataNode const* head = bitlog::macro_metadata_head();
  while (head)
  {
    std::cout << "metadata_id: " << head->id << ", file: " << head->macro_metadata.file
              << ", line: " << head->macro_metadata.line
              << ", format: " << head->macro_metadata.log_format << ", first_arg_type_id: "
              << static_cast<uint16_t>(head->macro_metadata.type_descriptors.front()) << std::endl;

    head = head->next;
  }

  // Start of logging
  std::cout << std::endl;

  bitlog::Logger<bitlog::BoundedQueue> logger{"testing"};

  LOG_INFO(logger, "hello world {}", 12u);
  LOG_INFO(logger, "hello doubles {}", 123.3);

  for (int i = 0; i < 3; ++i)
  {
    LOG_INFO(logger, "hello char loop {}", char(i));
  }

  return 0;
}