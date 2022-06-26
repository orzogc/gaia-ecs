// picobench v2.01
// https://github.com/iboB/picobench
//
// A micro microbenchmarking library in a single header file
//
// MIT License
//
// Copyright(c) 2017-2018 Borislav Stanimirov
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
//
//                  VERSION HISTORY
//
//  2.01 (2019-03-03) * Fixed android build when binding to a signle core
//                    * Minor doc fixes
//  2.00 (2018-10-30) * Breaking change! runner::run_benchmarks doesn't return
//                      a report anymore. The report is generated by
//                      runner::generate_report instead
//                    * Breaking change! report_output_format doesn't accept
//                      output streams as arguments. Use set_output_streams.
//                    * Potentially breaking change (gcc and clang)! Always set
//                      thread affinity to first core. Macro to turn this off.
//                    * Added runner::run which performs a full execution
//                    * Added benchmark results and results comparison
//                    * Added error enum
//                    * Macro option to allow a std::function as a benchmark
//                    * Macros for default iterations and samples
//                    * Allowing local registration of benchmarks in a runner
//                    * Added local_runner which doesn't consume registry
//                    * More force-inline functions in states
//                    * Fixed some potential compilation warnings
//                    * Removed tests from header
//                    * Anonymous namespace for impl-only classes and funcs
//                    * Added setters and getters for every config option
//  1.05 (2018-07-17) * Counting iterations of state
//                    * Optionally set thread affinity when running benchmarks
//                      so as not to miss cpu cycles with the high res clock
//  1.04 (2018-02-06) * User data for benchmarks, which can be seen from states
//                    * `add_custom_duration` to states so the user can modify time
//                    * Text table format fixes
//                    * Custom cmd opts in runner
//                    * --version CLI command
//  1.03 (2018-01-05) Added helper methods for easier browsing of reports
//  1.02 (2018-01-04) Added parsing of command line
//  1.01 (2018-01-03) * Only taking the fastest sample into account
//                    * Set default number of samples to 2
//                    * Added CSV output
//  1.00 (2018-01-01) Initial release
//  0.01 (2017-12-28) Initial prototype release
//
//
//                  EXAMPLE
//
// void my_function(); // the function you want to benchmark
//
// // write your benchmarking code in a function like this
// static void benchmark_my_function(picobench::state& state)
// {
//     // use the state in a range-based for loop to call your code
//     for (auto _ : state)
//         my_function();
// }
// // create a picobench with your benchmarking code
// PICOBENCH(benchmark_my_function);
//
//
//                  BASIC DOCUMENTATION
//
// A very brief usage guide follows. For more detailed documentation see the
// README here: https://github.com/iboB/picobench/blob/master/README.md
//
// Simply include this file wherever you need.
// You need to define PICOBENCH_IMPLEMENT_WITH_MAIN (or PICOBENCH_IMPLEMENT if
// you want to write your own main function) in one compilation unit to have
// the implementation compiled there.
//
// The benchmark code must be a `void (picobench::state&)` function which
// you have written. Benchmarks are registered using the `PICOBENCH` macro
// where the only argument is the function's name.
//
// You can have multiple benchmarks in multiple files. All will be run when the
// executable starts.
//
// Typically a benchmark has a loop. To run the loop use the state argument in
// a range-based for loop in your function. The time spent looping is measured
// for the benchmark. You can have initialization/deinitialization code outside
// of the loop and it won't be measured.
//
#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <vector>

#if defined(PICOBENCH_STD_FUNCTION_BENCHMARKS)
	#include <functional>
#endif

#define PICOBENCH_VERSION 2.01
#define PICOBENCH_VERSION_STR "2.01"

#if defined(PICOBENCH_DEBUG)
	#include <cassert>
	#define I_PICOBENCH_ASSERT assert
#else
	#define I_PICOBENCH_ASSERT(...)
#endif

#if defined(__GNUC__)
	#define PICOBENCH_INLINE __attribute__((always_inline))
#elif defined(_MSC_VER)
	#define PICOBENCH_INLINE __forceinline
#else
	#define PICOBENCH_INLINE inline
#endif

#if (!defined(__GNUC__) && !defined(__clang__)) || defined(__pnacl__) || defined(__EMSCRIPTEN__)
	#define PICOBENCH_HAS_NO_INLINE_ASSEMBLY
#endif

namespace picobench {

	namespace internal {
		inline void UseCharPointer(char const volatile* var) {
			(void)var;
		}
	} // namespace internal

	// Force the compiler to flush pending writes to global memory. Acts as an
	// effective read/write barrier
	inline PICOBENCH_INLINE void ClobberMemory() {
		std::atomic_signal_fence(std::memory_order_acq_rel);
	}

// The DoNotOptimize(...) function can be used to prevent a value or
// expression from being optimized away by the compiler. This function is
// intended to add little to no overhead.
// See: https://youtu.be/nXaxk27zwlk?t=2441
#ifndef PICOBENCH_HAS_NO_INLINE_ASSEMBLY
	template <class Tp>
	inline PICOBENCH_INLINE void DoNotOptimize(Tp const& value) {
		asm volatile("" : : "r,m"(value) : "memory");
	}

	template <class Tp>
	inline PICOBENCH_INLINE void DoNotOptimize(Tp& value) {
	#if defined(__clang__)
		asm volatile("" : "+r,m"(value) : : "memory");
	#else
		asm volatile("" : "+m,r"(value) : : "memory");
	#endif
	}

#elif defined(_MSC_VER)
	template <class Tp>
	inline PICOBENCH_INLINE void DoNotOptimize(Tp const& value) {
		internal::UseCharPointer(&reinterpret_cast<char const volatile&>(value));
		_ReadWriteBarrier();
	}
#else
	template <class Tp>
	inline PICOBENCH_INLINE void DoNotOptimize(Tp const& value) {
		internal::UseCharPointer(&reinterpret_cast<char const volatile&>(value));
	}
#endif

#if defined(_MSC_VER) || defined(__MINGW32__) || defined(PICOBENCH_TEST)
	struct high_res_clock {
		typedef long long rep;
		typedef std::nano period;
		typedef std::chrono::duration<rep, period> duration;
		typedef std::chrono::time_point<high_res_clock> time_point;
		static const bool is_steady = true;

		static time_point now();
	};
#else
	using high_res_clock = std::chrono::high_resolution_clock;
#endif

	using result_t = intptr_t;

	class state {
	public:
		explicit state(int num_iterations, uintptr_t user_data = 0): _user_data(user_data), _iterations(num_iterations) {
			I_PICOBENCH_ASSERT(_iterations > 0);
		}

		int iterations() const {
			return _iterations;
		}

		int64_t duration_ns() const {
			return _duration_ns;
		}
		void add_custom_duration(int64_t duration_ns) {
			_duration_ns += duration_ns;
		}

		uintptr_t user_data() const {
			return _user_data;
		}

		// optionally set result of benchmark
		// this can be used as a value sync to prevent optimizations
		// or a way to check whether benchmarks produce the same results
		void set_result(uintptr_t data) {
			_result = data;
		}
		result_t result() const {
			return _result;
		}

		PICOBENCH_INLINE
		void start_timer() {
			_start = high_res_clock::now();
		}

		PICOBENCH_INLINE
		void stop_timer() {
			auto duration = high_res_clock::now() - _start;
			_duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
		}

		struct iterator {
			PICOBENCH_INLINE
			iterator(state* parent): _counter(0), _lim(parent->iterations()), _state(parent) {
				I_PICOBENCH_ASSERT(_counter < _lim);
			}

			PICOBENCH_INLINE
			iterator(): _counter(0), _lim(0), _state(nullptr) {}

			PICOBENCH_INLINE
			iterator& operator++() {
				I_PICOBENCH_ASSERT(_counter < _lim);
				++_counter;
				return *this;
			}

			PICOBENCH_INLINE
			bool operator!=(const iterator&) const {
				if (_counter < _lim)
					return true;
				_state->stop_timer();
				return false;
			}

			PICOBENCH_INLINE
			int operator*() const {
				return _counter;
			}

		private:
			int _counter;
			const int _lim;
			state* _state;
		};

		PICOBENCH_INLINE
		iterator begin() {
			start_timer();
			return iterator(this);
		}

		PICOBENCH_INLINE
		iterator end() {
			return iterator();
		}

	private:
		high_res_clock::time_point _start;
		int64_t _duration_ns = 0;
		uintptr_t _user_data;
		int _iterations;
		result_t _result = 0;
	};

	// this can be used for manual measurement
	class scope {
	public:
		PICOBENCH_INLINE
		scope(state& s): _state(s) {
			_state.start_timer();
		}

		PICOBENCH_INLINE
		~scope() {
			_state.stop_timer();
		}

	private:
		state& _state;
	};

#if defined(PICOBENCH_STD_FUNCTION_BENCHMARKS)
	using benchmark_proc = std::function<void(state&)>;
#else
	using benchmark_proc = void (*)(state&);
#endif

	class benchmark {
	public:
		const char* name() const {
			return _name;
		}

		benchmark& iterations(std::vector<int> data) {
			_state_iterations = std::move(data);
			return *this;
		}
		benchmark& samples(int n) {
			_samples = n;
			return *this;
		}
		benchmark& label(const char* label) {
			_name = label;
			return *this;
		}
		benchmark& baseline(bool b = true) {
			_baseline = b;
			return *this;
		}
		benchmark& user_data(uintptr_t data) {
			_user_data = data;
			return *this;
		}

	protected:
		friend class runner;

		benchmark(const char* name, benchmark_proc proc);

		const char* _name;
		const benchmark_proc _proc;
		bool _baseline = false;

		uintptr_t _user_data = 0;
		std::vector<int> _state_iterations;
		int _samples = 0;
	};

	// used for globally  functions
	// note that you can instantiate a runner and register local benchmarks for it alone
	class global_registry {
	public:
		static int set_bench_suite(const char* name);
		static benchmark& new_benchmark(const char* name, benchmark_proc proc);
	};

} // namespace picobench

#define I_PICOBENCH_PP_CAT(a, b) I_PICOBENCH_PP_INTERNAL_CAT(a, b)
#define I_PICOBENCH_PP_INTERNAL_CAT(a, b) a##b

#define PICOBENCH_SUITE(name)                                                                                          \
	static int I_PICOBENCH_PP_CAT(picobench_suite, __LINE__) = picobench::global_registry::set_bench_suite(name)

#define PICOBENCH(func)                                                                                                \
	static auto& I_PICOBENCH_PP_CAT(picobench, __LINE__) = picobench::global_registry::new_benchmark(#func, func)

#if defined(PICOBENCH_IMPLEMENT_WITH_MAIN)
	#define PICOBENCH_IMPLEMENT
	#define PICOBENCH_IMPLEMENT_MAIN
#endif

#if defined(PICOBENCH_IMPLEMENT)

	#include <cstdlib>
	#include <cstring>
	#include <fstream>
	#include <iomanip>
	#include <iostream>
	#include <map>
	#include <memory>
	#include <random>

	#if defined(_WIN32)
		#define WIN32_LEAN_AND_MEAN
		#include <Windows.h>
	#else
		#if !defined(PICOBENCH_DONT_BIND_TO_ONE_CORE)
			#if defined(__APPLE__)
				#include <mach/mach.h>
			#else
				#include <sched.h>
			#endif
		#endif
	#endif

namespace picobench {

	// namespace
	// {

	enum error_t {
		no_error,
		error_bad_cmd_line_argument, // ill-formed command-line argument
		error_unknown_cmd_line_argument, // command argument looks like a picobench one, but isn't
		error_sample_compare, // benchmark produced different results across samples
		error_benchmark_compare, // two benchmarks of the same suite and dimension produced different results
	};

	class report {
	public:
		struct benchmark_problem_space {
			int dimension; // number of iterations for the problem space
			int samples; // number of samples taken
			int64_t total_time_ns; // fastest sample!!!
			result_t result; // result of fastest sample
		};
		struct benchmark {
			const char* name;
			bool is_baseline;
			std::vector<benchmark_problem_space> data;
		};

		struct suite {
			const char* name;
			std::vector<benchmark> benchmarks; // benchmark view

			const benchmark* find_benchmark(const char* name) const {
				for (auto& b: benchmarks) {
					if (strcmp(b.name, name) == 0)
						return &b;
				}

				return nullptr;
			}

			const benchmark* find_baseline() const {
				for (auto& b: benchmarks) {
					if (b.is_baseline)
						return &b;
				}

				return nullptr;
			}
		};

		std::vector<suite> suites;
		error_t error = no_error;

		const suite* find_suite(const char* name) const {
			for (auto& s: suites) {
				if (strcmp(s.name, name) == 0)
					return &s;
			}

			return nullptr;
		}

		void to_text(std::ostream& out) const {
			using namespace std;

			line(out);
			out << "   Name (baseline is *)   |   Dim   |  Total ms |  ns/op  |Baseline| Ops/second\n";
			line(out);

			for (auto& suite: suites) {
				if (suite.name) {
					out << suite.name << ":\n";
					line(out);
				}

				auto problem_space_view = get_problem_space_view(suite);
				for (auto& ps: problem_space_view) {
					const problem_space_benchmark* baseline = nullptr;
					for (auto& bm: ps.second) {
						if (bm.is_baseline) {
							baseline = &bm;
							break;
						}
					}

					for (auto& bm: ps.second) {
						out << std::left;

						if (bm.is_baseline) {
							out << setw(23) << bm.name << " *";
						} else {
							out << setw(25) << bm.name;
						}

						out << std::right;

						out << " |" << setw(8) << ps.first << " |" << setw(10) << fixed << setprecision(3)
								<< double(bm.total_time_ns) / 1000000.0 << " |";

						auto ns_op = (bm.total_time_ns / ps.first);
						if (ns_op > 99999999) {
							int e = 0;
							while (ns_op > 999999) {
								++e;
								ns_op /= 10;
							}
							out << ns_op << 'e' << e;
						} else {
							out << setw(8) << ns_op;
						}

						out << " |";

						if (baseline == &bm) {
							out << "      - |";
						} else if (baseline) {
							out << setw(7) << fixed << setprecision(3) << double(bm.total_time_ns) / double(baseline->total_time_ns)
									<< " |";
						} else {
							// no baseline to compare to
							out << "    ??? |";
						}

						auto ops_per_sec = ps.first * (1000000000.0 / double(bm.total_time_ns));
						out << setw(11) << fixed << setprecision(1) << ops_per_sec << "\n";
					}
				}
				line(out);
			}
		}

		void to_text_concise(std::ostream& out) {
			using namespace std;

			line(out);
			out << "   Name (baseline is *)   |  ns/op  | Baseline |  Ops/second\n";
			line(out);

			for (auto& suite: suites) {
				if (suite.name) {
					out << suite.name << ":\n";
					line(out);
				}

				const benchmark* baseline = nullptr;
				for (auto& bm: suite.benchmarks) {
					if (bm.is_baseline) {
						baseline = &bm;
						break;
					}
				}
				I_PICOBENCH_ASSERT(baseline);
				int64_t baseline_total_time = 0;
				int baseline_total_iterations = 0;
				for (auto& d: baseline->data) {
					baseline_total_time += d.total_time_ns;
					baseline_total_iterations += d.dimension;
				}
				int64_t baseline_ns_per_op = baseline_total_time / baseline_total_iterations;

				for (auto& bm: suite.benchmarks) {
					out << std::left;

					if (bm.is_baseline) {
						out << setw(23) << bm.name << " *";
					} else {
						out << setw(25) << bm.name;
					}

					out << std::right;

					int64_t total_time = 0;
					int total_iterations = 0;
					for (auto& d: bm.data) {
						total_time += d.total_time_ns;
						total_iterations += d.dimension;
					}
					int64_t ns_per_op = total_time / total_iterations;

					out << " |" << setw(8) << ns_per_op << " |";

					if (&bm == baseline) {
						out << "        - |";
					} else {
						out << setw(9) << fixed << setprecision(3) << double(ns_per_op) / double(baseline_ns_per_op) << " |";
					}

					auto ops_per_sec = total_iterations * (1000000000.0 / double(total_time));
					out << setw(12) << fixed << setprecision(1) << ops_per_sec << "\n";
				}

				line(out);
			}
		}

		void to_csv(std::ostream& out, bool header = true) const {
			using namespace std;

			if (header) {
				out << "Suite,Benchmark,b,D,S,\"Total ns\",Result,\"ns/op\",Baseline\n";
			}

			for (auto& suite: suites) {
				const benchmark* baseline = nullptr;
				for (auto& bm: suite.benchmarks) {
					if (bm.is_baseline) {
						baseline = &bm;
						break;
					}
				}
				I_PICOBENCH_ASSERT(baseline);

				for (auto& bm: suite.benchmarks) {
					for (auto& d: bm.data) {
						if (suite.name) {
							out << '"' << suite.name << '"';
							;
						}
						out << ",\"" << bm.name << "\",";
						if (&bm == baseline) {
							out << '*';
						}
						out << ',' << d.dimension << ',' << d.samples << ',' << d.total_time_ns << ',' << d.result << ','
								<< (d.total_time_ns / d.dimension) << ',';

						if (baseline) {
							for (auto& bd: baseline->data) {
								if (bd.dimension == d.dimension) {
									out << fixed << setprecision(3) << (double(d.total_time_ns) / double(bd.total_time_ns));
								}
							}
						}

						out << '\n';
					}
				}
			}
		}

		struct problem_space_benchmark {
			const char* name;
			bool is_baseline;
			int64_t total_time_ns; // fastest sample!!!
			result_t result; // result of fastest sample
		};

		static std::map<int, std::vector<problem_space_benchmark>> get_problem_space_view(const suite& s) {
			std::map<int, std::vector<problem_space_benchmark>> res;
			for (auto& bm: s.benchmarks) {
				for (auto& d: bm.data) {
					auto& pvbs = res[d.dimension];
					pvbs.push_back({bm.name, bm.is_baseline, d.total_time_ns, d.result});
				}
			}
			return res;
		}

	private:
		static void line(std::ostream& out) {
			for (int i = 0; i < 79; ++i)
				out.put('=');
			out.put('\n');
		}
	};

	class benchmark_impl: public benchmark {
	public:
		benchmark_impl(const char* name, benchmark_proc proc): benchmark(name, proc) {}

	private:
		friend class runner;

		// state
		std::vector<state> _states; // length is _samples * _state_iterations.size()
		std::vector<state>::iterator _istate;
	};

	class picostring {
	public:
		picostring() = default;
		explicit picostring(const char* text) {
			str = text;
			len = int(strlen(text));
		}

		const char* str;
		int len = 0;

		// checks whether other begins with this string
		bool cmp(const char* other) const {
			return strncmp(str, other, size_t(len)) == 0;
		}
	};

	class null_streambuf: public std::streambuf {
	public:
		virtual int overflow(int c) override {
			return c;
		}
	};

	struct null_stream: public std::ostream {
		null_stream(): std::ostream(&_buf) {}

	private:
		null_streambuf _buf;
	} cnull;

	enum class report_output_format { text, concise_text, csv };

	#if !defined(PICOBENCH_DEFAULT_ITERATIONS)
		#define PICOBENCH_DEFAULT_ITERATIONS                                                                               \
			{ 8, 64, 512, 4096, 8192 }
	#endif

	#if !defined(PICOBENCH_DEFAULT_SAMPLES)
		#define PICOBENCH_DEFAULT_SAMPLES 2
	#endif

	using benchmarks_vector = std::vector<std::unique_ptr<benchmark_impl>>;
	struct rsuite {
		const char* name;
		benchmarks_vector benchmarks;
	};

	class registry {
	public:
		benchmark& add_benchmark(const char* name, benchmark_proc proc) {
			auto b = new benchmark_impl(name, proc);
			benchmarks_for_current_suite().emplace_back(b);
			return *b;
		}

		void set_suite(const char* name) {
			_current_suite_name = name;
		}

		const char*& current_suite_name() {
			return _current_suite_name;
		}

		benchmarks_vector& benchmarks_for_current_suite() {
			for (auto& s: _suites) {
				if (s.name == _current_suite_name)
					return s.benchmarks;

				if (s.name && _current_suite_name && strcmp(s.name, _current_suite_name) == 0)
					return s.benchmarks;
			}
			_suites.push_back({_current_suite_name, {}});
			return _suites.back().benchmarks;
		}

	protected:
		friend class runner;
		const char* _current_suite_name = nullptr;
		std::vector<rsuite> _suites;
	};

	registry& g_registry() {
		static registry r;
		return r;
	}

	class runner: public registry {
	public:
		runner(bool local = false):
				_default_state_iterations(PICOBENCH_DEFAULT_ITERATIONS), _default_samples(PICOBENCH_DEFAULT_SAMPLES) {
			if (!local) {
				_suites = std::move(g_registry()._suites);
			}
		}

		int run(int benchmark_random_seed = -1) {
			if (should_run()) {
				run_benchmarks(benchmark_random_seed);
				auto report = generate_report();
				std::ostream* out = _stdout;
				std::ofstream fout;
				if (preferred_output_filename()) {
					fout.open(preferred_output_filename());
					if (!fout.is_open()) {
						std::cerr << "Error: Could not open output file `" << preferred_output_filename() << "`\n";
						return 1;
					}
					out = &fout;
				}

				switch (preferred_output_format()) {
					case picobench::report_output_format::text:
						report.to_text(*out);
						break;
					case picobench::report_output_format::concise_text:
						report.to_text_concise(*out);
						break;
					case picobench::report_output_format::csv:
						report.to_csv(*out);
						break;
				}
			}
			return error();
		}

		void run_benchmarks(int random_seed = -1) {
			I_PICOBENCH_ASSERT(_error == no_error && _should_run);

			if (random_seed == -1) {
				random_seed = int(std::random_device()());
			}

			std::minstd_rand rnd(random_seed);

			// vector of all benchmarks
			std::vector<benchmark_impl*> benchmarks;
			for (auto& suite: _suites) {
				// also identify a baseline in this loop
				// if there is no explicit one, set the first one as a baseline
				bool found_baseline = false;
				for (auto irb = suite.benchmarks.begin(); irb != suite.benchmarks.end(); ++irb) {
					auto& rb = *irb;
					rb->_states.clear(); // clear states so we can safely call run_benchmarks multiple times
					benchmarks.push_back(rb.get());
					if (rb->_baseline) {
						found_baseline = true;
					}

	#if !defined(PICOBENCH_STD_FUNCTION_BENCHMARKS)
					// check for same func
					for (auto ib = irb + 1; ib != suite.benchmarks.end(); ++ib) {
						auto& b = *ib;
						if (rb->_proc == b->_proc) {
							*_stdwarn << "Warning: " << rb->name() << " and " << b->name()
												<< " are benchmarks of the same function.\n";
						}
					}
	#endif
				}

				if (!found_baseline && !suite.benchmarks.empty()) {
					suite.benchmarks.front()->_baseline = true;
				}
			}

			// initialize benchmarks
			for (auto b: benchmarks) {
				const std::vector<int>& state_iterations =
						b->_state_iterations.empty() ? _default_state_iterations : b->_state_iterations;

				if (b->_samples == 0)
					b->_samples = _default_samples;

				b->_states.reserve(state_iterations.size() * size_t(b->_samples));

				// fill states while random shuffling them
				for (auto iters: state_iterations) {
					for (int i = 0; i < b->_samples; ++i) {
						auto index = rnd() % (b->_states.size() + 1);
						auto pos = b->_states.begin() + long(index);
						b->_states.emplace(pos, iters, b->_user_data);
					}
				}

				b->_istate = b->_states.begin();
			}

	#if !defined(PICOBENCH_DONT_BIND_TO_ONE_CORE)
			// set thread affinity to first cpu
			// so the high resolution clock doesn't miss cycles
			{
		#if defined(_WIN32)
				SetThreadAffinityMask(GetCurrentThread(), 1);
		#elif defined(__APPLE__)
				thread_affinity_policy_data_t policy = {0};
				thread_policy_set(pthread_mach_thread_np(pthread_self()), THREAD_AFFINITY_POLICY, (thread_policy_t)&policy, 1);
		#else
				cpu_set_t cpuset;
				CPU_ZERO(&cpuset);
				CPU_SET(0, &cpuset);

				sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);
		#endif
			}
	#endif

			// we run a random benchmark from it incrementing _istate for each
			// when _istate reaches _states.end(), we erase the benchmark
			// when the vector becomes empty, we're done
			while (!benchmarks.empty()) {
				auto i = benchmarks.begin() + long(rnd() % benchmarks.size());
				auto& b = *i;

				b->_proc(*b->_istate);

				++b->_istate;

				if (b->_istate == b->_states.end()) {
					benchmarks.erase(i);
				}
			}
		}

		// function to compare results
		template <typename CompareResult = std::equal_to<result_t>>
		report generate_report(CompareResult cmp = std::equal_to<result_t>()) const {
			report rpt;

			rpt.suites.resize(_suites.size());
			auto rpt_suite = rpt.suites.begin();

			for (auto& suite: _suites) {
				rpt_suite->name = suite.name;

				// build benchmark view
				rpt_suite->benchmarks.resize(suite.benchmarks.size());
				auto rpt_benchmark = rpt_suite->benchmarks.begin();

				for (auto& b: suite.benchmarks) {
					rpt_benchmark->name = b->_name;
					rpt_benchmark->is_baseline = b->_baseline;

					const std::vector<int>& state_iterations =
							b->_state_iterations.empty() ? _default_state_iterations : b->_state_iterations;

					rpt_benchmark->data.reserve(state_iterations.size());
					for (auto d: state_iterations) {
						rpt_benchmark->data.push_back({d, 0, 0, 0});
					}

					for (auto& state: b->_states) {
						for (auto& d: rpt_benchmark->data) {
							if (state.iterations() == d.dimension) {
								if (d.total_time_ns == 0 || d.total_time_ns > state.duration_ns()) {
									d.total_time_ns = state.duration_ns();
									d.result = state.result();
								}

								if (_compare_results_across_samples) {
									if (d.result != state.result() && !cmp(d.result, state.result())) {
										*_stderr << "Error: Two samples of " << b->name() << " @" << d.dimension
														 << " produced different results: " << d.result << " and " << state.result() << '\n';
										_error = error_sample_compare;
									}
								}

								++d.samples;
							}
						}
					}

	#if defined(PICOBENCH_DEBUG)
					for (auto& d: rpt_benchmark->data) {
						I_PICOBENCH_ASSERT(d.samples == b->_samples);
					}
	#endif

					++rpt_benchmark;
				}

				++rpt_suite;
			}

			if (_compare_results_across_benchmarks) {
				for (auto& suite: rpt.suites) {
					auto psview = report::get_problem_space_view(suite);

					for (auto& space: psview) {
						I_PICOBENCH_ASSERT(!space.second.empty());

						if (space.second.size() == 1) {
							auto& b = space.second.front();
							*_stdwarn << "Warning: Benchmark " << b.name << " @" << space.first
												<< " has a single instance and cannot be compared to others.\n";
							continue;
						}

						auto result0 = space.second.front().result;

						for (auto& b: space.second) {
							if (result0 != b.result && !cmp(result0, b.result)) {
								auto& f = space.second.front();
								*_stderr << "Error: Benchmarks " << f.name << " and " << b.name << " @" << space.first
												 << " produce different results: " << result0 << " and " << b.result << '\n';
								_error = error_benchmark_compare;
							}
						}
					}
				}
			}

			return rpt;
		}

		void set_default_state_iterations(const std::vector<int>& data) {
			_default_state_iterations = data;
		}

		const std::vector<int>& default_state_iterations() const {
			return _default_state_iterations;
		}

		void set_default_samples(int n) {
			_default_samples = n;
		}

		int default_samples() const {
			return _default_samples;
		}

		void add_cmd_opt(
				const char* cmd, const char* arg_desc, const char* cmd_desc, bool (*handler)(uintptr_t, const char*),
				uintptr_t user_data = 0) {
			cmd_line_option opt;
			opt.cmd = picostring(cmd);
			opt.arg_desc = picostring(arg_desc);
			opt.desc = cmd_desc;
			opt.handler = nullptr;
			opt.user_data = user_data;
			opt.user_handler = handler;
			_opts.push_back(opt);
		}

		// returns false if there were errors parsing the command line
		// all args starting with prefix are parsed
		// the others are ignored
		bool parse_cmd_line(int argc, const char* const argv[], const char* cmd_prefix = "-") {
			_cmd_prefix = picostring(cmd_prefix);

			if (!_has_opts) {
				_opts.emplace_back("-iters=", "<n1,n2,n3,...>", "Sets default iterations for benchmarks", &runner::cmd_iters);
				_opts.emplace_back("-samples=", "<n>", "Sets default number of samples for benchmarks", &runner::cmd_samples);
				_opts.emplace_back("-out-fmt=", "<txt|con|csv>", "Outputs text or concise or csv", &runner::cmd_out_fmt);
				_opts.emplace_back("-output=", "<filename>", "Sets output filename or `stdout`", &runner::cmd_output);
				_opts.emplace_back("-compare-results", "", "Compare benchmark results", &runner::cmd_compare_results);
				_opts.emplace_back("-no-run", "", "Doesn't run benchmarks", &runner::cmd_no_run);
				_opts.emplace_back("-version", "", "Show version info", &runner::cmd_version);
				_opts.emplace_back("-help", "", "Prints help", &runner::cmd_help);
				_has_opts = true;
			}

			for (int i = 1; i < argc; ++i) {
				if (!_cmd_prefix.cmp(argv[i]))
					continue;

				auto arg = argv[i] + _cmd_prefix.len;

				bool found = false;
				for (auto& opt: _opts) {
					if (opt.cmd.cmp(arg)) {
						found = true;
						bool success = false;
						if (opt.handler) {
							success = (this->*opt.handler)(arg + opt.cmd.len);
						} else {
							I_PICOBENCH_ASSERT(opt.user_handler);
							success = opt.user_handler(opt.user_data, arg + opt.cmd.len);
						}

						if (!success) {
							*_stderr << "Error: Bad command-line argument: " << argv[i] << "\n";
							_error = error_bad_cmd_line_argument;
							return false;
						}
						break;
					}
				}

				if (!found) {
					*_stderr << "Error: Unknown command-line argument: " << argv[i] << "\n";
					_error = error_unknown_cmd_line_argument;
					return false;
				}
			}

			return true;
		}

		void set_should_run(bool set) {
			_should_run = set;
		}
		bool should_run() const {
			return _error == no_error && _should_run;
		}
		void set_error(error_t e) {
			_error = e;
		}
		error_t error() const {
			return _error;
		}

		void set_output_streams(std::ostream& out, std::ostream& err) {
			_stdout = &out;
			_stderr = &err;
			_stdwarn = &out;
		}

		void set_preferred_output_format(report_output_format fmt) {
			_output_format = fmt;
		}
		report_output_format preferred_output_format() const {
			return _output_format;
		}

		// can be nullptr (run will interpret it as stdout)
		void set_preferred_output_filename(const char* path) {
			_output_file = path;
		}
		const char* preferred_output_filename() const {
			return _output_file;
		}

		void set_compare_results_across_samples(bool b) {
			_compare_results_across_samples = b;
		}
		bool compare_results_across_samples() const {
			return _compare_results_across_samples;
		}

		void set_compare_results_across_benchmarks(bool b) {
			_compare_results_across_benchmarks = b;
		}
		bool compare_results_across_benchmarks() const {
			return _compare_results_across_benchmarks;
		}

	private:
		// runner's suites and benchmarks come from its parent: registry

		// state and configuration
		mutable error_t _error = no_error;
		bool _should_run = true;

		bool _compare_results_across_samples = false;
		bool _compare_results_across_benchmarks = false;

		report_output_format _output_format = report_output_format::text;
		const char* _output_file = nullptr; // nullptr means stdout

		std::ostream* _stdout = &std::cout;
		std::ostream* _stderr = &std::cerr;
		std::ostream* _stdwarn = &std::cout;

		// default data

		// default iterations per state per benchmark
		std::vector<int> _default_state_iterations;

		// default samples per benchmark
		int _default_samples;

		// command line parsing
		picostring _cmd_prefix;
		typedef bool (runner::*cmd_handler)(const char*); // internal handler
		typedef bool (*ext_handler)(uintptr_t user_data, const char* cmd_line); // external (user) handler
		struct cmd_line_option {
			cmd_line_option() = default;
			cmd_line_option(const char* c, const char* a, const char* d, cmd_handler h):
					cmd(c), arg_desc(a), desc(d), handler(h), user_data(0), user_handler(nullptr) {}
			picostring cmd;
			picostring arg_desc;
			const char* desc;
			cmd_handler handler; // may be nullptr for external handlers
			uintptr_t user_data; // passed as an argument to user handlers
			ext_handler user_handler;
		};
		bool _has_opts = false; // have opts been added to list
		std::vector<cmd_line_option> _opts;

		bool cmd_iters(const char* line) {
			std::vector<int> iters;
			auto p = line;
			while (true) {
				auto i = int(strtoul(p, nullptr, 10));
				if (i <= 0)
					return false;
				iters.push_back(i);
				p = strchr(p + 1, ',');
				if (!p)
					break;
				++p;
			}
			if (iters.empty())
				return false;
			_default_state_iterations = iters;
			return true;
		}

		bool cmd_samples(const char* line) {
			int samples = int(strtol(line, nullptr, 10));
			if (samples <= 0)
				return false;
			_default_samples = samples;
			return true;
		}

		bool cmd_no_run(const char* line) {
			if (*line)
				return false;
			_should_run = false;
			return true;
		}

		bool cmd_version(const char* line) {
			if (*line)
				return false;
			*_stdout << "picobench " PICOBENCH_VERSION_STR << "\n";
			_should_run = false;
			return true;
		}

		bool cmd_help(const char* line) {
			if (*line)
				return false;
			cmd_version(line);
			auto& cout = *_stdout;
			for (auto& opt: _opts) {
				cout << ' ' << _cmd_prefix.str << opt.cmd.str << opt.arg_desc.str;
				int w = 27 - (_cmd_prefix.len + opt.cmd.len + opt.arg_desc.len);
				for (int i = 0; i < w; ++i) {
					cout.put(' ');
				}
				cout << opt.desc << "\n";
			}
			_should_run = false;
			return true;
		}

		bool cmd_out_fmt(const char* line) {
			if (strcmp(line, "txt") == 0) {
				_output_format = report_output_format::text;
			} else if (strcmp(line, "con") == 0) {
				_output_format = report_output_format::concise_text;
			} else if (strcmp(line, "csv") == 0) {
				_output_format = report_output_format::csv;
			} else {
				return false;
			}
			return true;
		}

		bool cmd_output(const char* line) {
			if (strcmp(line, "stdout") != 0) {
				_output_file = line;
			} else {
				_output_file = nullptr;
			}
			return true;
		}

		bool cmd_compare_results(const char* line) {
			if (*line)
				return false;
			_compare_results_across_samples = true;
			_compare_results_across_benchmarks = true;
			return true;
		}
	};

	class local_runner: public runner {
	public:
		local_runner(): runner(true) {}
	};

	// } // anonymous namespace

	benchmark::benchmark(const char* name, benchmark_proc proc): _name(name), _proc(proc) {}

	benchmark& global_registry::new_benchmark(const char* name, benchmark_proc proc) {
		return g_registry().add_benchmark(name, proc);
	}

	int global_registry::set_bench_suite(const char* name) {
		g_registry().current_suite_name() = name;
		return 0;
	}

	#if (defined(_MSC_VER) || defined(__MINGW32__)) && !defined(PICOBENCH_TEST)

	static const long long high_res_clock_freq = []() -> long long {
		LARGE_INTEGER frequency;
		QueryPerformanceFrequency(&frequency);
		return frequency.QuadPart;
	}();

	high_res_clock::time_point high_res_clock::now() {
		LARGE_INTEGER t;
		QueryPerformanceCounter(&t);
		return time_point(duration((t.QuadPart * rep(period::den)) / high_res_clock_freq));
	}
	#endif
} // namespace picobench

#endif

#if defined(PICOBENCH_IMPLEMENT_MAIN)
int main(int argc, char* argv[]) {
	picobench::runner r;
	r.parse_cmd_line(argc, argv);
	return r.run();
}
#endif

#if defined(PICOBENCH_TEST)

// fake time keeping functions for the tests
namespace picobench {

	void this_thread_sleep_for_ns(uint64_t ns);

	template <class Rep, class Period>
	void this_thread_sleep_for(const std::chrono::duration<Rep, Period>& duration) {
		this_thread_sleep_for_ns(std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count());
	}

	#if defined(PICOBENCH_IMPLEMENT)
	static struct fake_time { uint64_t now; } the_time;

	void this_thread_sleep_for_ns(uint64_t ns) {
		the_time.now += ns;
	}

	high_res_clock::time_point high_res_clock::now() {
		auto ret = time_point(duration(the_time.now));
		return ret;
	}
	#endif

} // namespace picobench

#endif