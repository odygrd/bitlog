#pragma once

// Include always common first as it defines FMTBITLOG_HEADER_ONLY
#include "bitlog/detail/common.h"

#include <span>
#include <cstdint>
#include <string_view>
#include <atomic>
#include <utility>

#include "bitlog/bundled/fmt/format.h"
#include "bitlog/detail/encode.h"

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
[[nodiscard]] MacroMetadataNode*& get_macro_metadata_head_node() noexcept
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
    : macro_metadata(macro_metadata), next(std::exchange(get_macro_metadata_head_node(), this))
  {
  }

  MacroMetadata const& macro_metadata;
  MacroMetadataNode const* next;
};

/**
 * @brief Template instance for macro metadata node initialization.
 */
template <StringLiteral File, StringLiteral Function, uint32_t Line, LogLevel Level, StringLiteral LogFormat, typename... Args>
MacroMetadataNode marco_metadata_node{get_macro_metadata<File, Function, Line, Level, LogFormat, Args...>()};

/**
 * TODO
 */
template <typename TConfig>
class ThreadContext : public UniqueId<ThreadContext<TConfig>>
{
public:
  using queue_t = typename TConfig::queue_t;

  ThreadContext(ThreadContext const&) = delete;
  ThreadContext& operator=(ThreadContext const&) = delete;

  explicit ThreadContext(TConfig const& config) : _config(config)
  {
    std::string const queue_file_base = fmtbitlog::format("{}.{}.ext", this->id, _queue_id++);
    std::error_code res = _queue.create(_config.queue_capacity(), _config.instance_dir() / queue_file_base);

    if (res)
    {
      // TODO:: Error handling ?
    }
  }

  [[nodiscard]] queue_t& get_queue() noexcept { return _queue; }

private:
  TConfig const& _config;
  queue_t _queue;
  uint32_t _queue_id{0}; /** Unique counter for each spawned queue by this thread context */
};

/**
 * This function retrieves the thread-specific context.
 *
 * The purpose is to ensure that only one instance of ThreadContext is created
 * per thread. Without this approach, using thread_local within a templated
 * function, e.g., log<>, could lead to multiple ThreadContext instances being
 * created for the same thread.
 */
template <typename TConfig>
ThreadContext<TConfig>& get_thread_context(TConfig const& config) noexcept
{
  thread_local ThreadContext<TConfig> thread_context{config};
  return thread_context;
}
}