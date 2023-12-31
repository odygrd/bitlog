import bitlog;

#include <iostream>
#include <filesystem>
#include <system_error>

using namespace bitlog;

int main()
{
  std::error_code ec;

  std::vector<std::string> postfix_ids;
  ec = BoundedQueue::discover_writers(postfix_ids, std::filesystem::path{});

  if (ec)
  {
    std::cout << " error " << ec.message() << std::endl;
    return 0;
  }

  for (auto const& postfix_id : postfix_ids)
  {
    std::cout << postfix_id << std::endl;
  }

  BoundedQueue q {};
  ec = q.open(postfix_ids[0], std::filesystem::path{});

  if (ec)
  {
    std::cout << " error " << ec.message() << std::endl;
    return 0;
  }


  while(true)
  {
    std::byte* buffer = q.prepare_read();
    if (buffer)
    {
      size_t res;
      std::memcpy(&res, buffer, sizeof(size_t));

      std::cout << "popped item: " << res << std::endl;

      q.finish_read(sizeof(size_t));
      q.commit_read();
    }
  }
    return 0;
}