#pragma once

// Include always common first as it defines FMTBITLOG_HEADER_ONLY
#include "bitlog/common/common.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "bitlog/bundled/fmt/format.h"
#include "bitlog/common/bounded_queue.h"
#include "encode.h"

namespace bitlog::detail
{
/**
 * @brief Generates unique IDs for instances of the specified template parameter type.
 *
 * This class is designed to produce unique identifiers, which can be utilized for various purposes
 * such as managing metadata, loggers, thread contexts.
 */
template <typename TDerived>
struct UniqueId
{
public:
  UniqueId() : id(_gen_id()) {}
  uint32_t const id;

private:
  [[nodiscard]] static uint32_t _gen_id() noexcept
  {
    static std::atomic<uint32_t> unique_id{0};
    return unique_id.fetch_add(1u, std::memory_order_relaxed);
  }
};

/**
 * Stores macro metadata
 */
struct MacroMetadata
{
  constexpr MacroMetadata(std::string_view file, std::string_view function, uint32_t line, LogLevel level,
                          std::string_view log_format, std::span<TypeDescriptorName const> type_descriptors)
    : type_descriptors(type_descriptors), file(file), function(function), log_format(log_format), line(line), level(level)
  {
  }

  std::span<TypeDescriptorName const> type_descriptors;
  std::string_view file;
  std::string_view function;
  std::string_view log_format;
  uint32_t line;
  LogLevel level;
};

/**
 * @brief Function to retrieve a reference to macro metadata.
 * @return Reference to the macro metadata.
 */
template <StringLiteral File, StringLiteral Function, uint32_t Line, LogLevel Level, StringLiteral LogFormat, typename... Args>
[[nodiscard]] MacroMetadata const& get_macro_metadata() noexcept
{
  static constexpr std::array<TypeDescriptorName, sizeof...(Args)> type_descriptors{
    GetTypeDescriptor<Args>::value...};

  static constexpr MacroMetadata macro_metadata{
    File.value, Function.value,  Line,
    Level,      LogFormat.value, std::span<TypeDescriptorName const>{type_descriptors}};

  return macro_metadata;
}

/** Forward declaration **/
struct MacroMetadataNode;

/**
 * @brief Function returning a reference to the head of the macro metadata nodes.
 * @return Reference to the head of macro metadata nodes.
 */
[[nodiscard]] MacroMetadataNode*& macro_metadata_head_node() noexcept
{
  static MacroMetadataNode* head{nullptr};
  return head;
}

/**
 * Node to store macro metadata
 */
struct MacroMetadataNode : public UniqueId<MacroMetadataNode>
{
  explicit MacroMetadataNode(MacroMetadata const& macro_metadata)
    : macro_metadata(macro_metadata), next(std::exchange(macro_metadata_head_node(), this))
  {
  }

  MacroMetadata const& macro_metadata;
  MacroMetadataNode const* next;
};

/**
 * @brief Template instance for macro metadata node initialisation.
 */
template <StringLiteral File, StringLiteral Function, uint32_t Line, LogLevel Level, StringLiteral LogFormat, typename... Args>
MacroMetadataNode marco_metadata_node{get_macro_metadata<File, Function, Line, Level, LogFormat, Args...>()};

/**
 * @brief Creates a run directory for the given application.
 *
 * Creates a run directory using the specified application ID and base directory.
 *
 * @param application_id The ID of the application.
 * @param base_dir The base directory where the run directory will be created. If empty, system defaults are used.
 * @return An optional containing the created run directory path on success, or std::nullopt on failure.
 */
[[nodiscard]] inline std::optional<std::filesystem::path> create_run_directory(
  std::string_view application_id, std::string_view base_dir = std::string_view{}) noexcept
{
  auto const now = std::chrono::system_clock::now().time_since_epoch();

  std::error_code ec{};
  std::filesystem::path const run_dir_base = detail::resolve_base_dir(ec, base_dir);

  if (ec)
  {
    return std::nullopt;
  }

  // non-const for RVO
  std::filesystem::path run_dir = run_dir_base / application_id /
    std::to_string(std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());

  if (std::filesystem::exists(run_dir, ec) || ec)
  {
    return std::nullopt;
  }

  std::filesystem::create_directories(run_dir, ec);

  if (ec)
  {
    return std::nullopt;
  }

  return run_dir;
}

/**
 * @brief Writes log statement metadata information to a YAML file.
 *
 * @param path The path where the file will be written.
 * @return True when the file was successfully created, false otherwise
 */
[[nodiscard]] inline bool create_log_statements_metadata_file(std::filesystem::path const& path) noexcept
{
  MetadataFile metadata_writer;

  if (!metadata_writer.init_writer(path / LOG_STATEMENTS_METADATA_FILENAME))
  {
    return false;
  }

  // First write some generic key/values
  std::string file_data = fmtbitlog::format("process_id: {}\n", ::getpid());

  std::error_code ec;
  if (!metadata_writer.write(file_data.data(), file_data.size(), ec))
  {
    return false;
  }

  // Then write all log statement metadata
  file_data = "log_statements:\n";
  if (!metadata_writer.write(file_data.data(), file_data.size(), ec))
  {
    return false;
  }

  // Store them in a vector to write them in reverse. It doesn't make difference just makes
  // the metadata file look consistent with the logger-metadata
  std::vector<MacroMetadataNode const*> metadata_vec;
  metadata_vec.reserve(128);

  MacroMetadataNode const* cur = macro_metadata_head_node();
  while (cur)
  {
    metadata_vec.push_back(cur);
    cur = cur->next;
  }

  for (auto metadata_node_it = metadata_vec.rbegin(); metadata_node_it != metadata_vec.rend(); ++metadata_node_it)
  {
    MacroMetadataNode const* metadata_node = *metadata_node_it;

    std::string type_descriptors_str;
    for (size_t i = 0; i < metadata_node->macro_metadata.type_descriptors.size(); ++i)
    {
      type_descriptors_str +=
        std::to_string(static_cast<uint32_t>(metadata_node->macro_metadata.type_descriptors[i]));

      if (i != metadata_node->macro_metadata.type_descriptors.size() - 1)
      {
        type_descriptors_str += " ";
      }
    }

    if (type_descriptors_str.empty())
    {
      file_data = fmtbitlog::format(
        "  - id: {}\n    file: {}\n    line: {}\n    function: {}\n    log_format: {}\n    "
        "log_level: {}\n",
        metadata_node->id, metadata_node->macro_metadata.file, metadata_node->macro_metadata.line,
        metadata_node->macro_metadata.function, metadata_node->macro_metadata.log_format,
        static_cast<uint32_t>(metadata_node->macro_metadata.level));
    }
    else
    {
      file_data = fmtbitlog::format(
        "  - id: {}\n    file: {}\n    line: {}\n    function: {}\n    log_format: {}\n    "
        "type_descriptors: {}\n    log_level: {}\n",
        metadata_node->id, metadata_node->macro_metadata.file, metadata_node->macro_metadata.line,
        metadata_node->macro_metadata.function, metadata_node->macro_metadata.log_format,
        type_descriptors_str, static_cast<uint32_t>(metadata_node->macro_metadata.level));
    }

    if (!metadata_writer.write(file_data.data(), file_data.size(), ec))
    {
      return false;
    }
  }

  return true;
}

/**
 * @brief Writes loggers metadata information to a YAML file.
 *
 * @param path The path where the file will be written.
 * @return True when the file was successfully created, false otherwise
 */
[[nodiscard]] inline bool create_loggers_metadata_file(std::filesystem::path const& path) noexcept
{
  MetadataFile metadata_writer;

  if (!metadata_writer.init_writer(path / LOGGERS_METADATA_FILENAME))
  {
    return false;
  }

  // First write some generic key/values
  std::string file_data = "loggers:\n";

  std::error_code ec;
  if (!metadata_writer.write(file_data.data(), file_data.size(), ec))
  {
    return false;
  }

  return true;
}

/**
 * @brief Appends loggers metadata information to a YAML file.
 *
 * @param path The path where the file will be written.
 * @return True when the file was successfully created, false otherwise
 */
[[nodiscard]] inline bool append_loggers_metadata_file(std::filesystem::path const& path, uint32_t logger_id,
                                                       std::string const& logger_name) noexcept
{
  MetadataFile metadata_writer;

  if (!metadata_writer.init_writer(path / LOGGERS_METADATA_FILENAME))
  {
    return false;
  }

  std::string const file_data = fmtbitlog::format("  - id: {}\n    name: {}\n", logger_id, logger_name);

  std::error_code ec;
  if (!metadata_writer.write(file_data.data(), file_data.size(), ec))
  {
    return false;
  }

  return true;
}

/**
 * @brief Represents a thread-local queue associated with a specific frontend configuration.
 *
 * This class inherits from UniqueId to ensure that each thread has a unique identifier.
 * It is essential for scenarios where a thread is spawned, dies, and then respawns. In such cases,
 * the operating system might reuse the same thread ID, but UniqueId guarantees a distinct
 * identifier for each thread instance, preventing potential misidentification.
 *
 * @tparam TFrontendOptions The type of frontend options.
 */
template <typename TFrontendOptions>
class ThreadLocalQueue : public UniqueId<ThreadLocalQueue<TFrontendOptions>>
{
public:
  using frontend_options_t = TFrontendOptions;

  ThreadLocalQueue(ThreadLocalQueue const&) = delete;
  ThreadLocalQueue& operator=(ThreadLocalQueue const&) = delete;

  /**
   * @brief Construct a ThreadLocalQueue with the specified frontend options.
   *
   * @param run_dir Current running dir
   * @param options The frontend options for the thread-local queue.
   */
  explicit ThreadLocalQueue(std::filesystem::path const& run_dir, frontend_options_t const& options)
    : _run_dir(run_dir), _options(options)
  {
    reset();
  };

  /**
   * @brief Retrieve the thread-local queue.
   *
   * @return A reference to the thread-local queue.
   */
  [[nodiscard]] std::optional<typename frontend_options_t::queue_type>& queue() noexcept
  {
    return _queue;
  }

  /**
   * @brief Reset the thread-local queue, creating a new one
   *
   * The user can call this when the existing queue is full to switch to a new queue.
   */
  void reset() noexcept
  {
    // user can call this when the existing queue() is full to switch to a new queue
    std::string const next_queue_base_filename = fmtbitlog::format("{}.{}.ext", this->id, _queue_seq++);
    _queue.emplace();

    std::error_code ec;
    if (!_queue->create(_run_dir / next_queue_base_filename, _options.queue_capacity_bytes,
                        _options.memory_page_size, 5, ec))
    {
      _queue.reset();
    }
  }

private:
  frontend_options_t const& _options;
  std::filesystem::path const& _run_dir;
  std::optional<typename frontend_options_t::queue_type> _queue; ///< Using optional to destroy and reset the queue to a new one
  uint32_t _queue_seq{0}; ///< Unique counter for each spawned queue by this thread
};

/**
 * @brief Retrieve the thread-local queue for a specific frontend configuration.
 *
 * The purpose is to ensure that only one instance of ThreadLocalQueue is created
 * per thread. Without this approach, using thread_local within a templated
 * function, e.g., log<>, could lead to multiple ThreadLocalQueue instances being
 * created for the same thread.
 *
 * @param run_dir Current running dir
 * @param options The frontend options.
 * @return A reference to the thread-local queue.
 */
template <typename FrontendOptions>
ThreadLocalQueue<FrontendOptions>& get_thread_local_queue(std::filesystem::path const& run_dir,
                                                          FrontendOptions const& options) noexcept
{
  thread_local ThreadLocalQueue<FrontendOptions> thread_local_queue{run_dir, options};
  return thread_local_queue;
}
} // namespace bitlog::detail