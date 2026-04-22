#pragma once

#include "gaia/config/config.h"

#include <cstdint>

#include "gaia/mt/jobcommon.h"
#include "gaia/mt/jobhandle.h"
#include "gaia/mt/threadpool.h"

namespace gaia {
	namespace ecs {
		enum class QueryExecType : uint32_t;

		//! Opaque synchronization token returned by a scheduler.
		//! The scheduler owns the meaning of the payload.
		struct SchedToken {
			//! Scheduler-defined token payload.
			uintptr_t value[2]{};
		};

		//! Description of a single task submitted to a scheduler.
		struct SchedTaskDesc {
			//! Opaque callback context forwarded to invoke().
			void* pCtx = nullptr;
			//! Task entry point.
			void (*invoke)(void* pCtx) = nullptr;
			//! Execution hint selected by the ECS caller.
			QueryExecType execType{};
		};

		//! Description of a parallel-for submission to a scheduler.
		struct SchedParDesc {
			//! Opaque callback context forwarded to invoke().
			void* pCtx = nullptr;
			//! Parallel-for entry point receiving a half-open item range [idxStart, idxEnd).
			void (*invoke)(void* pCtx, uint32_t idxStart, uint32_t idxEnd) = nullptr;
			//! Total number of items to process.
			uint32_t itemCount = 0;
			//! Preferred group size. A value of 0 lets the scheduler choose.
			uint32_t groupSize = 0;
			//! Execution hint selected by the ECS caller.
			QueryExecType execType{};
		};

		//! Scheduler descriptor used by ECS runtime code.
		//! All callbacks may be null when the descriptor is only used as a placeholder and will be
		//! resolved through sched_def().
		struct Sched {
			//! Opaque scheduler-owned context passed back to every callback.
			void* pCtx = nullptr;
			//! Schedules one task for execution.
			//! \param pCtx Scheduler-owned context.
			//! \param pDesc Task description to execute.
			//! \return Opaque synchronization token for the scheduled work.
			SchedToken (*sched)(void* pCtx, const SchedTaskDesc* pDesc) = nullptr;
			//! Schedules a parallel-for workload for execution.
			//! \param pCtx Scheduler-owned context.
			//! \param pDesc Parallel-for description to execute.
			//! \return Opaque synchronization token for the scheduled work.
			SchedToken (*sched_par)(void* pCtx, const SchedParDesc* pDesc) = nullptr;
			//! Waits until the scheduled work referenced by @a token finishes.
			//! \param pCtx Scheduler-owned context.
			//! \param token Opaque synchronization token returned by sched() or sched_par().
			void (*wait)(void* pCtx, SchedToken token) = nullptr;
			//! Frees any scheduler-owned resources associated with @a token.
			//! \param pCtx Scheduler-owned context.
			//! \param token Opaque synchronization token returned by sched() or sched_par().
			void (*free)(void* pCtx, SchedToken token) = nullptr;
		};

		namespace detail {
			inline mt::JobPriority exec_prio(QueryExecType execType) {
				// QueryExecType::ParallelEff is encoded as value 3. Keep the scheduler bridge independent
				// from the enum definition point so this header stays C-like and forward-declarable.
				return (uint32_t)execType == 3U ? mt::JobPriority::Low : mt::JobPriority::High;
			}

			inline SchedToken sched_one_def([[maybe_unused]] void* pCtx, const SchedTaskDesc* pDesc) {
				GAIA_ASSERT(pDesc != nullptr);
				GAIA_ASSERT(pDesc->invoke != nullptr);
				if (pDesc == nullptr || pDesc->invoke == nullptr)
					return {};

				mt::Job job;
				job.priority = exec_prio(pDesc->execType);
				job.func = [pCtx = pDesc->pCtx, invoke = pDesc->invoke]() {
					invoke(pCtx);
				};

				const auto handle = mt::ThreadPool::get().sched(GAIA_MOV(job));
				SchedToken token{};
				token.value[0] = (uintptr_t)handle.value();
				return token;
			}

			inline SchedToken sched_par_def([[maybe_unused]] void* pCtx, const SchedParDesc* pDesc) {
				GAIA_ASSERT(pDesc != nullptr);
				GAIA_ASSERT(pDesc->invoke != nullptr);
				if (pDesc == nullptr || pDesc->invoke == nullptr || pDesc->itemCount == 0)
					return {};

				mt::JobParallelRef job{};
				job.pCtx = const_cast<SchedParDesc*>(pDesc);
				job.priority = exec_prio(pDesc->execType);
				job.invoke = [](void* pCtx, const mt::JobArgs& args) {
					auto& desc = *(const SchedParDesc*)pCtx;
					desc.invoke(desc.pCtx, args.idxStart, args.idxEnd);
				};

				const auto handle = mt::ThreadPool::get().sched_par(job, pDesc->itemCount, pDesc->groupSize);
				SchedToken token{};
				token.value[0] = (uintptr_t)handle.value();
				return token;
			}

			inline void sched_wait_def([[maybe_unused]] void* pCtx, SchedToken token) {
				mt::ThreadPool::get().wait(mt::JobHandle((uint32_t)token.value[0]));
			}

			inline void sched_free_def([[maybe_unused]] void* pCtx, [[maybe_unused]] SchedToken token) {}
		} // namespace detail

		//! Returns the default ECS scheduler backed by gaia::mt::ThreadPool.
		//! \return Default scheduler descriptor.
		GAIA_NODISCARD inline const Sched& sched_def() {
			static const Sched sched = [] {
				Sched b{};
				b.sched = &detail::sched_one_def;
				b.sched_par = &detail::sched_par_def;
				b.wait = &detail::sched_wait_def;
				b.free = &detail::sched_free_def;
				return b;
			}();
			return sched;
		}

		//! Resolves @a sched to the default scheduler when it has no callbacks installed.
		//! \param sched Scheduler descriptor to resolve.
		//! \return Either @a sched or the default scheduler when @a sched is empty.
		GAIA_NODISCARD inline const Sched& sched_resolve(const Sched& sched) {
			if (sched.sched == nullptr && sched.sched_par == nullptr && sched.wait == nullptr && sched.free == nullptr)
				return sched_def();
			return sched;
		}

		//! Schedules one task through @a sched.
		//! \param sched Scheduler descriptor.
		//! \param desc Task description.
		//! \return Opaque synchronization token for the scheduled work.
		GAIA_NODISCARD inline SchedToken sched_one(const Sched& sched, const SchedTaskDesc& desc) {
			const auto& resolved = sched_resolve(sched);
			GAIA_ASSERT(resolved.sched != nullptr);
			return resolved.sched != nullptr ? resolved.sched(resolved.pCtx, &desc) : SchedToken{};
		}

		//! Schedules a parallel-for workload through @a sched.
		//! \param sched Scheduler descriptor.
		//! \param desc Parallel-for description.
		//! \return Opaque synchronization token for the scheduled work.
		GAIA_NODISCARD inline SchedToken sched_par(const Sched& sched, const SchedParDesc& desc) {
			const auto& resolved = sched_resolve(sched);
			GAIA_ASSERT(resolved.sched_par != nullptr);
			return resolved.sched_par != nullptr ? resolved.sched_par(resolved.pCtx, &desc) : SchedToken{};
		}

		//! Waits until the scheduled work referenced by @a token finishes.
		//! \param sched Scheduler descriptor.
		//! \param token Opaque synchronization token returned by sched_one() or sched_par().
		inline void sched_wait(const Sched& sched, SchedToken token) {
			const auto& resolved = sched_resolve(sched);
			if (resolved.wait != nullptr)
				resolved.wait(resolved.pCtx, token);
		}

		//! Frees any scheduler-owned resources associated with @a token.
		//! \param sched Scheduler descriptor.
		//! \param token Opaque synchronization token returned by sched_one() or sched_par().
		inline void sched_free(const Sched& sched, SchedToken token) {
			const auto& resolved = sched_resolve(sched);
			if (resolved.free != nullptr)
				resolved.free(resolved.pCtx, token);
		}
	} // namespace ecs
} // namespace gaia
