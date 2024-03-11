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
private:
  enum class DirectoryType
  {
    Base,
    App,
    Run
  };

public:
  using backend_options_t = TBackendOptions;
  using application_context_t = detail::ApplicationContext<backend_options_t>;

  /**
   * @brief Explicit constructor for Backend.
   *
   * @param application_id Identifier for the application.
   * @param start_ts       Start timestamp for the application.
   * @param options        Options for the backend.
   * @param base_dir       Base directory path.
   */
  explicit Backend(std::string_view application_id = std::string_view{},
                   std::string_view start_ts = std::string_view{},
                   backend_options_t options = backend_options_t{},
                   std::string_view base_dir = std::string_view{})
  {
    if (application_id.empty() && !start_ts.empty())
    {
      std::abort();
    }

    std::error_code ec{};
    _base_dir = detail::resolve_base_dir(ec, base_dir);

    if (ec)
    {
      std::abort();
    }

    if (!application_id.empty())
    {
      _base_dir /= application_id;
      _directory_type = DirectoryType::App;

      if (!start_ts.empty())
      {
        _base_dir /= application_id;
        _directory_type = DirectoryType::Run;
      }
    }
  }

  /**
   * @brief Processes application contexts by discovering and handling run directories.
   */
  void process_application_contexts()
  {
    std::error_code ec{};
    switch (_directory_type)
    {
    case DirectoryType::Base:
      _process_application_directories(_base_dir, ec);
      break;
    case DirectoryType::App:
      _process_run_directories(_base_dir, ec);
      break;
    case DirectoryType::Run:
      _process_single_run(_base_dir, ec);
      break;
    }

    // Process application contexts and log
    for (auto& app_ctx : _application_contexts)
    {
      app_ctx->process_queues_and_log();
    }

    // remove any inactive ones, deleting the unused files
    // TODO:: Maybe do that on a timer
    _remove_inactive_application_contexts();
  }

private:
  /**
   * @brief Processes all application directories for a given base directory.
   *
   * @param base_dir The base directory to process.
   * @param ec       An output parameter for error codes.
   */
  void _process_application_directories(std::filesystem::path const& base_dir, std::error_code& ec)
  {
    auto const base_directory_it = std::filesystem::directory_iterator(base_dir, ec);

    if (ec) [[unlikely]]
    {
      // TODO:: Handle error
      return;
    }

    for (auto const& application_directory_entry : base_directory_it)
    {
      _process_run_directories(application_directory_entry, ec);
    }
  }

  /**
   * @brief Processes run directories for a given base directory.
   *
   * @param application_directory_entry The base directory entry to process.
   * @param ec                          An output parameter for error codes.
   */
  void _process_run_directories(std::filesystem::path const& application_directory_entry, std::error_code& ec)
  {
    auto const run_directory_it = std::filesystem::directory_iterator(application_directory_entry, ec);

    if (ec) [[unlikely]]
    {
      // TODO:: Handle error
      return;
    }

    for (auto const& run_directory_entry : run_directory_it)
    {
      _process_single_run(run_directory_entry.path(), ec);
    }
  }

  /**
   * @brief Processes a single run directory.
   *
   * @param run_directory_entry The run directory full path.
   * @param ec                 An output parameter for error codes.
   */
  void _process_single_run(std::filesystem::path const& run_directory_entry, std::error_code& ec)
  {
    if (ec) [[unlikely]]
    {
      // TODO:: Handle error
      return;
    }

    std::string const start_ts = run_directory_entry.stem();
    std::string const app_id = run_directory_entry.parent_path().stem();

    auto it = std::find_if(
      std::cbegin(_application_contexts), std::cend(_application_contexts),
      [&app_id, &start_ts](const auto& app_ctx)
      { return (app_ctx->application_id() == app_id) && (app_ctx->start_ts() == start_ts); });

    if (it == std::cend(_application_contexts))
    {
      // App doesn't exist, we can add it if it has initialised
      std::filesystem::path const app_ready_path = run_directory_entry / detail::APP_READY_FILENAME;
      bool const app_ready = std::filesystem::exists(app_ready_path, ec);

      if (!app_ready)
      {
        // App is still initializing
        return;
      }

      auto app_ctx = std::make_unique<application_context_t>(run_directory_entry, _options);

      if (!app_ctx->init(ec)) [[unlikely]]
      {
        // TODO:: Handle error
        return;
      }

      _application_contexts.push_back(std::move(app_ctx));
    }
  }

  /**
   * @brief Removes inactive application contexts that are no longer running.
   */
  void _remove_inactive_application_contexts()
  {
    for (auto it = std::begin(_application_contexts); it != std::end(_application_contexts);)
    {
      auto& app_ctx = *it;

      if (!app_ctx->is_running())
      {
        // save the run dir before erasing
        std::filesystem::path const run_dir = app_ctx->run_dir();

        it = _application_contexts.erase(it);

        std::error_code ec;
        std::filesystem::remove_all(run_dir, ec);

        if (ec)
        {
          // TODO:: Handle error
        }
      }
      else
      {
        ++it;
      }
    }
  }

private:
  std::filesystem::path _base_dir;
  std::vector<std::unique_ptr<application_context_t>> _application_contexts;
  backend_options_t _options;
  DirectoryType _directory_type{DirectoryType::Base};
};
} // namespace bitlog