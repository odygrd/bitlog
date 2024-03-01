#pragma once

#include "bitlog/backend/backend_impl.h"

namespace bitlog
{
/**
 * @brief Structure representing configuration options for the backend.
 *
 * @tparam QueueType The type of queue to be used.
 */
template <QueueTypeOption QueueType>
struct BackendOptions
{
  using queue_type =
    std::conditional_t<QueueType == QueueTypeOption::Default, bitlog::detail::BoundedQueue, bitlog::detail::BoundedQueueX86>;

  MemoryPageSize memory_page_size = MemoryPageSize::RegularPage;
};

template <typename TBackendOptions>
class Backend
{
public:
  using backend_options_t = TBackendOptions;

  /**
   * @brief Explicit constructor for Backend.
   * @param application_id Identifier for the application.
   * @param options Options for the frontend.
   * @param base_dir Base directory path.
   */
  explicit Backend(std::string_view application_id, backend_options_t options = backend_options_t{},
                   std::string_view base_dir = std::string_view{})
  {
    std::error_code ec{};
    std::filesystem::path const run_dir_base = detail::resolve_base_dir(ec, base_dir);

    if (ec)
    {
      std::abort();
    }
  }

private:
};
} // namespace bitlog