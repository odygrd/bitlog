/**
 * Adding a benchmark for a another logger should be straight forward by duplicating and modifying
 * this file.
 */

#include <thread>

#include "hot_path_bench.h"

#include "bitlog/backend.h"
#include "bitlog/bitlog.h"

/***/
void quill_benchmark(std::vector<int32_t> const& thread_count_array,
                     size_t num_iterations_per_thread, size_t messages_per_iteration)
{
  /** - MAIN THREAD START - Logger setup if any **/

  /** - Setup Backend **/
  auto backend_thread = std::jthread(
    [](std::stop_token const& st)
    {
      bitlog::detail::set_cpu_affinity(5);

      using backend_options_t = bitlog::BackendOptions<bitlog::QueueTypeOption::Default>;
      using backend_t = bitlog::Backend<backend_options_t>;

      backend_t be{};

      while (!st.stop_requested())
      {
        be.process_application_contexts();
      }

      while (be.has_active_application_context())
      {
        be.process_application_contexts();
      }
    });

  /** - Setup Frontend **/
  using frontend_options_t =
    bitlog::FrontendOptions<bitlog::QueuePolicyOption::UnboundedNoLimit, bitlog::QueueTypeOption::Default, false>;
  using frontend_t = bitlog::Frontend<frontend_options_t>;

  frontend_options_t frontend_options{};
  frontend_t::init("hot_path_system_clock_bench", frontend_options);

  bitlog::SinkOptions sink_options;
  sink_options.output_file_suffix(bitlog::FileSuffix::StartDateTime);
  auto fsink = frontend_t::instance().create_file_sink("hot_path_system_clock_bench.log", sink_options);

  auto logger = frontend_t::instance().create_logger("root", fsink);

  /** LOGGING THREAD FUNCTIONS - on_start, on_exit, log_func must be implemented **/
  /** those run on a several thread(s). It can be one or multiple threads based on THREAD_LIST_COUNT config */
  auto on_start = []()
  {
    // on thread start
    frontend_t::instance().preallocate();
  };

  auto on_exit = []() {};

  // on main
  auto log_func = [logger](uint64_t k, uint64_t i, double d)
  {
    // Main logging function
    // This will get called MESSAGES_PER_ITERATION * ITERATIONS for each caller thread.
    // MESSAGES_PER_ITERATION will get averaged to a single number

    LOG_INFO(logger, "Logging iteration: {}, message: {}, double: {}", k, i, d);
  };

  /** ALWAYS REQUIRED **/
  // Run the benchmark for n threads
  for (auto thread_count : thread_count_array)
  {
    run_benchmark("Logger: Bitlog - Benchmark: Hot Path Latency / Nanoseconds", thread_count,
                  num_iterations_per_thread, messages_per_iteration, on_start, log_func, on_exit);
  }
}

/***/
int main(int, char**) { quill_benchmark(THREAD_LIST_COUNT, ITERATIONS, MESSAGES_PER_ITERATION); }