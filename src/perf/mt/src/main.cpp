#define PICOBENCH_IMPLEMENT
#include <gaia.h>
#include <picobench/picobench.hpp>

using namespace gaia;

#define PICO_SETTINGS() iterations({64}).samples(3)
#define PICO_SETTINGS_SANI() iterations({8}).samples(1)
#define PICOBENCH_SUITE_REG(name) r.current_suite_name() = name;
#define PICOBENCH_REG(func) (void)r.add_benchmark(#func, func)

void BM_ScheduleParallel_Complex(picobench::state& state);
void BM_ScheduleParallel_Simple(picobench::state& state);
void BM_Schedule_Complex(picobench::state& state);
void BM_Schedule_ECS_Complex(picobench::state& state);
void BM_Schedule_ECS_Simple(picobench::state& state);
void BM_Schedule_Empty(picobench::state& state);
void BM_Schedule_Simple(picobench::state& state);

int main(int argc, char* argv[]) {
	picobench::runner r(true);
	r.parse_cmd_line(argc, argv);

	// If picobench encounters an unknown command line argument it returns false and sets an error.
	// Ignore this behavior.
	// We only need to make sure to provide the custom arguments after the picobench ones.
	if (r.error() == picobench::error_unknown_cmd_line_argument)
		r.set_error(picobench::no_error);

	// With profiling mode enabled we want to be able to pick what benchmark to run so it is easier
	// for us to isolate the results.
	{
		bool profilingMode = false;
		bool sanitizerMode = false;

		const gaia::cnt::darray<std::string_view> args(argv + 1, argv + argc);
		for (const auto& arg: args) {
			if (arg == "-p") {
				profilingMode = true;
				continue;
			}
			if (arg == "-s") {
				sanitizerMode = true;
				continue;
			}
		}

		GAIA_LOG_N("Profiling mode = %s", profilingMode ? "ON" : "OFF");
		GAIA_LOG_N("Sanitizer mode = %s", sanitizerMode ? "ON" : "OFF");

		static constexpr uint32_t ItemsToProcess_Trivial = 1'000;
		static constexpr uint32_t ItemsToProcess_Simple = 1'000'000;
		static constexpr uint32_t ItemsToProcess_Complex = 1'000'000;

		if (profilingMode) {
			PICOBENCH_SUITE_REG("ECS");
			PICOBENCH_REG(BM_Schedule_ECS_Simple) //
					.PICO_SETTINGS()
					.user_data(ItemsToProcess_Simple | ((uint64_t)ecs::QueryExecType::Parallel << 32))
					.label("simple");
			PICOBENCH_REG(BM_Schedule_ECS_Complex) //
					.PICO_SETTINGS()
					.user_data(ItemsToProcess_Complex | ((uint64_t)ecs::QueryExecType::Parallel << 32))
					.label("simple");
			r.run_benchmarks();
			return 0;
		}

		if (sanitizerMode) {
			PICOBENCH_SUITE_REG("ECS");
			PICOBENCH_REG(BM_Schedule_ECS_Simple) //
					.PICO_SETTINGS()
					.user_data(ItemsToProcess_Trivial | ((uint64_t)ecs::QueryExecType::Parallel << 32))
					.label("complex");
			PICOBENCH_REG(BM_Schedule_ECS_Complex) //
					.PICO_SETTINGS()
					.user_data(ItemsToProcess_Trivial | ((uint64_t)ecs::QueryExecType::Parallel << 32))
					.label("complex");
			r.run_benchmarks();
			return 0;
		}

		{
			////////////////////////////////////////////////////////////////////////////////////////////////
			// Following benchmarks should scale linearly with the number of threads added.
			// If the CPU has enough threads of equal processing power available and the performance
			// doesn't scale accordingly it is most likely due to scheduling overhead.
			// We want to make this as small as possible.
			////////////////////////////////////////////////////////////////////////////////////////////////

			const auto workersCnt = mt::ThreadPool::get().workers();

			////////////////////////////////////////////////////////////////////////////////////////////////
			// Measures job creation overhead.
			////////////////////////////////////////////////////////////////////////////////////////////////
			PICOBENCH_SUITE_REG("Schedule - Empty");
			PICOBENCH_REG(BM_Schedule_Empty).PICO_SETTINGS().user_data(1000).label("sched, 1000");
			PICOBENCH_REG(BM_Schedule_Empty).PICO_SETTINGS().user_data(5000).label("sched, 5000");
			PICOBENCH_REG(BM_Schedule_Empty).PICO_SETTINGS().user_data(10000).label("sched, 10000");

			////////////////////////////////////////////////////////////////////////////////////////////////
			// Low load most likely to show scheduling overhead.
			////////////////////////////////////////////////////////////////////////////////////////////////
			PICOBENCH_SUITE_REG("Schedule/ScheduleParallel - Trivial");
			PICOBENCH_REG(BM_Schedule_Simple) //
					.PICO_SETTINGS()
					.user_data(ItemsToProcess_Trivial | (1ll << 32))
					.label("sched, 1");
			PICOBENCH_REG(BM_Schedule_Simple) //
					.PICO_SETTINGS()
					.user_data(ItemsToProcess_Trivial | (2ll << 32))
					.label("sched, 2");
			PICOBENCH_REG(BM_Schedule_Simple) //
					.PICO_SETTINGS()
					.user_data(ItemsToProcess_Trivial | (4ll << 32))
					.label("sched, 4");
			PICOBENCH_REG(BM_Schedule_Simple) //
					.PICO_SETTINGS()
					.user_data(ItemsToProcess_Trivial | (8ll << 32))
					.label("sched, 8");
			if (workersCnt > 8) {
				PICOBENCH_REG(BM_Schedule_Simple) //
						.PICO_SETTINGS()
						.user_data(ItemsToProcess_Trivial | ((uint64_t)workersCnt) << 32)
						.label("sched, MAX");
			}
			PICOBENCH_REG(BM_ScheduleParallel_Simple) //
					.PICO_SETTINGS()
					.user_data(ItemsToProcess_Trivial)
					.label("sched_par");

			////////////////////////////////////////////////////////////////////////////////////////////////
			// Medium load. Might show scheduling overhead.
			////////////////////////////////////////////////////////////////////////////////////////////////
			PICOBENCH_SUITE_REG("Schedule/ScheduleParallel - Simple");
			PICOBENCH_REG(BM_Schedule_Simple) //
					.PICO_SETTINGS()
					.user_data(ItemsToProcess_Simple | (1ll << 32))
					.label("sched, 1");
			PICOBENCH_REG(BM_Schedule_Simple) //
					.PICO_SETTINGS()
					.user_data(ItemsToProcess_Simple | (2ll << 32))
					.label("sched, 2");
			PICOBENCH_REG(BM_Schedule_Simple) //
					.PICO_SETTINGS()
					.user_data(ItemsToProcess_Simple | (4ll << 32))
					.label("sched, 4");
			PICOBENCH_REG(BM_Schedule_Simple) //
					.PICO_SETTINGS()
					.user_data(ItemsToProcess_Simple | (8ll << 32))
					.label("sched, 8");
			if (workersCnt > 8) {
				PICOBENCH_REG(BM_Schedule_Simple) //
						.PICO_SETTINGS()
						.user_data(ItemsToProcess_Simple | ((uint64_t)workersCnt) << 32)
						.label("sched, MAX");
			}
			PICOBENCH_REG(BM_ScheduleParallel_Simple) //
					.PICO_SETTINGS()
					.user_data(ItemsToProcess_Simple)
					.label("sched_par");

			////////////////////////////////////////////////////////////////////////////////////////////////
			// Bigger load. Should not be a subject to scheduling overhead.
			////////////////////////////////////////////////////////////////////////////////////////////////
			PICOBENCH_SUITE_REG("Schedule/ScheduleParallel - Complex");
			PICOBENCH_REG(BM_Schedule_Complex) //
					.PICO_SETTINGS()
					.user_data(ItemsToProcess_Complex | (1ll << 32))
					.label("sched, 1");
			PICOBENCH_REG(BM_Schedule_Complex) //
					.PICO_SETTINGS()
					.user_data(ItemsToProcess_Complex | (2ll << 32))
					.label("sched, 2");
			PICOBENCH_REG(BM_Schedule_Complex) //
					.PICO_SETTINGS()
					.user_data(ItemsToProcess_Complex | (4ll << 32))
					.label("sched, 4");
			PICOBENCH_REG(BM_Schedule_Complex) //
					.PICO_SETTINGS()
					.user_data(ItemsToProcess_Complex | (8ll << 32))
					.label("sched, 8");
			if (workersCnt > 8) {
				PICOBENCH_REG(BM_Schedule_Complex) //
						.PICO_SETTINGS()
						.user_data(ItemsToProcess_Complex | ((uint64_t)workersCnt) << 32)
						.label("sched, MAX");
			}
			PICOBENCH_REG(BM_ScheduleParallel_Complex).PICO_SETTINGS().user_data(ItemsToProcess_Complex).label("sched_par");

			////////////////////////////////////////////////////////////////////////////////////////////////
			// ECS
			////////////////////////////////////////////////////////////////////////////////////////////////
			PICOBENCH_SUITE_REG("ECS");
			PICOBENCH_REG(BM_Schedule_ECS_Simple) //
					.PICO_SETTINGS()
					.user_data(ItemsToProcess_Trivial | ((uint64_t)ecs::QueryExecType::Parallel << 32))
					.label("simple, 1T");
			PICOBENCH_REG(BM_Schedule_ECS_Simple) //
					.PICO_SETTINGS()
					.user_data(ItemsToProcess_Simple | ((uint64_t)ecs::QueryExecType::Parallel << 32))
					.label("simple, 1M");
			PICOBENCH_REG(BM_Schedule_ECS_Complex) //
					.PICO_SETTINGS()
					.user_data(ItemsToProcess_Trivial | ((uint64_t)ecs::QueryExecType::Parallel << 32))
					.label("complex, 1T");
			PICOBENCH_REG(BM_Schedule_ECS_Complex) //
					.PICO_SETTINGS()
					.user_data(ItemsToProcess_Complex | ((uint64_t)ecs::QueryExecType::Parallel << 32))
					.label("complex, 1M");
		}
	}

	return r.run(0);
}
