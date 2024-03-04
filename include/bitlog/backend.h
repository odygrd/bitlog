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
  using application_context_t = detail::ApplicationContext<backend_options_t>;

  /**
   * @brief Explicit constructor for Backend.
   * @param application_id Identifier for the application.
   * @param options Options for the frontend.
   * @param base_dir Base directory path.
   */
  explicit Backend(std::string_view application_id, backend_options_t options = backend_options_t{},
                   std::string_view base_dir = std::string_view{})
  {
    // TODO:: use application_id as filter, or read all if it is empty
    std::error_code ec{};
    _base_dir = detail::resolve_base_dir(ec, base_dir);

    if (ec)
    {
      std::abort();
    }
  }

  /**
   * @brief Processes application contexts by discovering and handling run directories.
   */
  void process_application_contexts()
  {
    std::error_code ec{};
    auto const base_dir_di = std::filesystem::directory_iterator(_base_dir, ec);

    if (ec)
    {
      // TODO:: Handle error
      return;
    }

    for (auto const& entry_base : base_dir_di)
    {
      if (!entry_base.is_directory())
      {
        continue;
      }

      process_run_directories(entry_base, ec);
    }

    remove_inactive_application_contexts();
  }

  /**
   * @brief Processes run directories for a given base directory.
   *
   * @param entry_base The base directory entry to process.
   * @param ec An output parameter for error codes.
   */
  void process_run_directories(std::filesystem::directory_entry const& entry_base, std::error_code& ec)
  {
    auto const run_dir_di = std::filesystem::directory_iterator(entry_base, ec);

    if (ec)
    {
      // TODO:: Handle error
      return;
    }

    for (auto const& entry_run : run_dir_di)
    {
      if (!entry_run.is_directory())
      {
        continue;
      }

      std::filesystem::path const app_ready_path = entry_run.path() / detail::APP_READY_FILENAME;
      bool const app_ready = std::filesystem::exists(app_ready_path, ec);

      if (ec)
      {
        // TODO:: Handle error
        continue;
      }

      if (!app_ready)
      {
        // App is still initializing
        continue;
      }

      std::string const start_ts = entry_run.path().stem();
      std::string const app_id = entry_base.path().stem();

      auto it = std::find_if(
        std::cbegin(_application_contexts), std::cend(_application_contexts),
        [&app_id, &start_ts](const auto& app_ctx)
        { return (app_ctx->application_id() == app_id) && (app_ctx->start_ts() == start_ts); });

      if (it == std::cend(_application_contexts))
      {
        auto app_ctx = std::make_unique<application_context_t>(entry_run, _options);
        app_ctx->init(ec);

        if (!ec)
        {
          _application_contexts.push_back(std::move(app_ctx));
        }
        else
        {
          // TODO:: Handle error
        }
      }
    }
  }

  /**
   * @brief Removes inactive application contexts that are no longer running.
   */
  void remove_inactive_application_contexts()
  {
    for (auto it = std::begin(_application_contexts); it != std::end(_application_contexts);)
    {
      auto& app_ctx = *it;

      if (!app_ctx->is_running())
      {
        std::filesystem::path const run_dir = app_ctx->run_dir();
        it = _application_contexts.erase(it);

        std::error_code ec;
        std::filesystem::remove_all(run_dir, ec);

        if (ec)
        {
          // TODO:: Handle error
        }
        continue;
      }

      app_ctx->process_queues_and_log();
      ++it;
    }
  }

private:
  std::filesystem::path _base_dir;
  std::vector<std::unique_ptr<application_context_t>> _application_contexts;
  backend_options_t _options;
};
} // namespace bitlog