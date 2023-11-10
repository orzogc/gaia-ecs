#pragma once
#include "../config/config.h"

#include <type_traits>

#include "../cnt/darray.h"
#include "../cnt/map.h"
#include "../cnt/sarray_ext.h"
#include "../config/profiler.h"
#include "../core/hashing_policy.h"
#include "../core/utility.h"
#include "../mt/jobhandle.h"
#include "../ser/serialization.h"
#include "archetype.h"
#include "archetype_common.h"
#include "chunk.h"
#include "chunk_iterator.h"
#include "common.h"
#include "component.h"
#include "component_cache.h"
#include "component_utils.h"
#include "data_buffer.h"
#include "query_cache.h"
#include "query_common.h"
#include "query_info.h"

namespace gaia {
	namespace ecs {
		namespace detail {
			template <bool Cached>
			struct QueryImplStorage {
				//! QueryImpl cache id
				QueryId m_queryId = QueryIdBad;
				//! QueryImpl cache (stable pointer to parent world's query cache)
				QueryCache* m_entityQueryCache{};
			};

			template <>
			struct QueryImplStorage<false> {
				QueryInfo m_queryInfo;
			};

			template <bool UseCaching = true>
			class QueryImpl final {
				static constexpr uint32_t ChunkBatchSize = 16;
				using ChunkSpan = std::span<const Chunk*>;
				using ChunkSpanMut = std::span<Chunk*>;
				using ChunkBatchedList = cnt::sarray_ext<Chunk*, ChunkBatchSize>;
				using CmdBufferCmdFunc = void (*)(SerializationBuffer& buffer, QueryCtx& ctx);

			private:
				//! Command buffer command type
				enum CommandBufferCmd : uint8_t { ADD_COMPONENT, ADD_FILTER };

				struct Command_AddComponent {
					static constexpr CommandBufferCmd Id = CommandBufferCmd::ADD_COMPONENT;

					Component comp;
					ComponentKind compKind;
					QueryListType listType;
					bool isReadWrite;

					void exec(QueryCtx& ctx) const {
						auto& data = ctx.data[compKind];
						auto& comps = data.comps;
						auto& lastMatchedArchetypeIdx = data.lastMatchedArchetypeIdx;
						auto& rules = data.rules;

						// Unique component ids only
						GAIA_ASSERT(!core::has(comps, comp));

#if GAIA_DEBUG
						// There's a limit to the amount of components which we can store
						if (comps.size() >= MAX_COMPONENTS_IN_QUERY) {
							GAIA_ASSERT(false && "Trying to create an ECS query with too many components!");

							const auto& cc = ComponentCache::get();
							auto componentName = cc.comp_desc(comp.id()).name;
							GAIA_LOG_E(
									"Trying to add ECS component '%.*s' to an already full ECS query!", (uint32_t)componentName.size(),
									componentName.data());

							return;
						}
#endif

						data.readWriteMask |= (uint8_t)isReadWrite << (uint8_t)comps.size();
						comps.push_back(comp);
						lastMatchedArchetypeIdx.push_back(0);
						rules.push_back(listType);

						if (listType == QueryListType::LT_All)
							++data.rulesAllCount;
					}
				};

				struct Command_Filter {
					static constexpr CommandBufferCmd Id = CommandBufferCmd::ADD_FILTER;

					Component comp;
					ComponentKind compKind;

					void exec(QueryCtx& ctx) const {
						auto& data = ctx.data[compKind];
						auto& comps = data.comps;
						auto& withChanged = data.withChanged;
						const auto& rules = data.rules;

						GAIA_ASSERT(core::has(comps, comp));
						GAIA_ASSERT(!core::has(withChanged, comp));

#if GAIA_DEBUG
						// There's a limit to the amount of components which we can store
						if (withChanged.size() >= MAX_COMPONENTS_IN_QUERY) {
							GAIA_ASSERT(false && "Trying to create an ECS filter query with too many components!");

							const auto& cc = ComponentCache::get();
							auto componentName = cc.comp_desc(comp.id()).name;
							GAIA_LOG_E(
									"Trying to add ECS component %.*s to an already full filter query!", (uint32_t)componentName.size(),
									componentName.data());
							return;
						}
#endif

						uint32_t compIdx = 0;
						for (; compIdx < comps.size(); ++compIdx)
							if (comps[compIdx] == comp)
								break;
						// NOTE: This code bellow does technically the same as above.
						//       However, compilers can't quite optimize it as well because it does some more
						//       calculations. This is a used often so go with the custom code.
						// const auto compIdx = core::get_index_unsafe(comps, comp);

						// Component has to be present in anyList or allList.
						// NoneList makes no sense because we skip those in query processing anyway.
						if (rules[compIdx] != QueryListType::LT_None) {
							withChanged.push_back(comp);
							return;
						}

						GAIA_ASSERT(false && "SetChangeFilter trying to filter ECS component which is not a part of the query");
#if GAIA_DEBUG
						const auto& cc = ComponentCache::get();
						auto componentName = cc.comp_desc(comp.id()).name;
						GAIA_LOG_E(
								"SetChangeFilter trying to filter ECS component %.*s but "
								"it's not a part of the query!",
								(uint32_t)componentName.size(), componentName.data());
#endif
					}
				};

				static constexpr CmdBufferCmdFunc CommandBufferRead[] = {
						// Add component
						[](SerializationBuffer& buffer, QueryCtx& ctx) {
							Command_AddComponent cmd;
							ser::load(buffer, cmd);
							cmd.exec(ctx);
						},
						// Add filter
						[](SerializationBuffer& buffer, QueryCtx& ctx) {
							Command_Filter cmd;
							ser::load(buffer, cmd);
							cmd.exec(ctx);
						}};

				//! Storage for data based on whether Caching is used or not
				QueryImplStorage<UseCaching> m_storage;
				//! Buffer with commands used to fetch the QueryInfo
				SerializationBuffer m_serBuffer;
				//! World version (stable pointer to parent world's m_nextArchetypeId)
				ArchetypeId* m_nextArchetypeId{};
				//! World version (stable pointer to parent world's world version)
				uint32_t* m_worldVersion{};
				//! List of archetypes (stable pointer to parent world's archetype array)
				const cnt::map<ArchetypeId, Archetype*>* m_archetypes{};
				//! Map of component ids to archetypes (stable pointer to parent world's archetype component-to-archetype map)
				const ComponentIdToArchetypeMap* m_componentToArchetypeMap{};
				//! Execution mode
				QueryExecMode m_executionMode = QueryExecMode::Run;

				//--------------------------------------------------------------------------------
			public:
				QueryInfo& fetch_query_info() {
					if constexpr (UseCaching) {
						// Make sure the query was created by World.query()
						GAIA_ASSERT(m_storage.m_entityQueryCache != nullptr);

						// Lookup hash is present which means QueryInfo was already found
						if GAIA_LIKELY (m_storage.m_queryId != QueryIdBad) {
							auto& queryInfo = m_storage.m_entityQueryCache->get(m_storage.m_queryId);
							queryInfo.match(*m_componentToArchetypeMap, last_archetype_id());
							return queryInfo;
						}

						// No lookup hash is present which means QueryInfo needs to be fetched or created
						QueryCtx ctx;
						commit(ctx);
						auto& queryInfo = m_storage.m_entityQueryCache->goc(GAIA_MOV(ctx));
						m_storage.m_queryId = queryInfo.id();
						queryInfo.match(*m_componentToArchetypeMap, last_archetype_id());
						return queryInfo;
					} else {
						if GAIA_UNLIKELY (m_storage.m_queryInfo.id() == QueryIdBad) {
							QueryCtx ctx;
							commit(ctx);
							m_storage.m_queryInfo = QueryInfo::create(QueryId{}, GAIA_MOV(ctx));
						}
						m_storage.m_queryInfo.match(*m_componentToArchetypeMap, last_archetype_id());
						return m_storage.m_queryInfo;
					}
				}

				//--------------------------------------------------------------------------------
			private:
				ArchetypeId last_archetype_id() const {
					return *m_nextArchetypeId - 1;
				}

				template <typename T>
				void add_inter(QueryListType listType) {
					constexpr auto compKind = component_kind_v<T>;
					constexpr auto isReadWrite = is_arg_mut_v<T>;

					// Make sure the component is always registered
					auto& cc = ComponentCache::get();
					const auto& desc = cc.goc_comp_desc<T>();

					Command_AddComponent cmd{desc.comp, compKind, listType, isReadWrite};
					ser::save(m_serBuffer, Command_AddComponent::Id);
					ser::save(m_serBuffer, cmd);
				}

				template <typename T>
				void changed_inter() {
					static_assert(is_raw_v<T>, "Use changed() with raw types only");

					constexpr auto compKind = component_kind_v<T>;

					// Make sure the component is always registered
					auto& cc = ComponentCache::get();
					const auto& desc = cc.goc_comp_desc<T>();

					Command_Filter cmd{desc.comp, compKind};
					ser::save(m_serBuffer, Command_Filter::Id);
					ser::save(m_serBuffer, cmd);
				}

				//--------------------------------------------------------------------------------

				void commit(QueryCtx& ctx) {
#if GAIA_ASSERT_ENABLED
					if constexpr (UseCaching) {
						GAIA_ASSERT(m_storage.m_queryId == QueryIdBad);
					} else {
						GAIA_ASSERT(m_storage.m_queryInfo.id() == QueryIdBad);
					}
#endif

					// Read data from buffer and execute the command stored in it
					m_serBuffer.seek(0);
					while (m_serBuffer.tell() < m_serBuffer.bytes()) {
						CommandBufferCmd id{};
						ser::load(m_serBuffer, id);
						CommandBufferRead[id](m_serBuffer, ctx);
					}

					// Calculate the lookup hash from the provided context
					calc_lookup_hash(ctx);

					// We can free all temporary data now
					m_serBuffer.reset();
				}

				//--------------------------------------------------------------------------------

				//! Unpacks the parameter list \param types into query \param query and performs All for each of them
				template <typename... T>
				void unpack_args_into_query_All(QueryImpl& query, [[maybe_unused]] core::func_type_list<T...> types) const {
					static_assert(sizeof...(T) > 0, "Inputs-less functors can not be unpacked to query");
					query.all<T...>();
				}

				//! Unpacks the parameter list \param types into query \param query and performs has_all for each of them
				template <typename... T>
				GAIA_NODISCARD bool unpack_args_into_query_has_all(
						const QueryInfo& queryInfo, [[maybe_unused]] core::func_type_list<T...> types) const {
					if constexpr (sizeof...(T) > 0)
						return queryInfo.has_all<T...>();
					else
						return true;
				}

				//--------------------------------------------------------------------------------

				GAIA_NODISCARD static bool match_filters(const Chunk& chunk, const QueryInfo& queryInfo) {
					GAIA_ASSERT(!chunk.empty() && "match_filters called on an empty chunk");

					const auto queryVersion = queryInfo.world_version();

					// See if any generic component has changed
					{
						const auto& filtered = queryInfo.filters(ComponentKind::CK_Gen);
						for (const auto comp: filtered) {
							const auto compIdx = chunk.comp_idx(ComponentKind::CK_Gen, comp.id());
							if (chunk.changed(ComponentKind::CK_Gen, queryVersion, compIdx))
								return true;
						}
					}

					// See if any unique component has changed
					{
						const auto& filtered = queryInfo.filters(ComponentKind::CK_Uni);
						for (const auto comp: filtered) {
							const uint32_t compIdx = chunk.comp_idx(ComponentKind::CK_Uni, comp.id());
							if (chunk.changed(ComponentKind::CK_Uni, queryVersion, compIdx))
								return true;
						}
					}

					// Skip unchanged chunks.
					return false;
				}

				//! Execute functors in batches
				template <typename Func>
				static void run_func_batched(Func func, ChunkBatchedList& chunks) {
					const auto chunkCnt = chunks.size();
					GAIA_ASSERT(chunkCnt > 0);

					// This is what the function is doing:
					// for (auto *pChunk: chunks) {
					//  pChunk->lock(true);
					//	func(*pChunk);
					//  pChunk->lock(false);
					// }
					// chunks.clear();

					GAIA_PROF_SCOPE(run_func_batched);

					// We only have one chunk to process
					if GAIA_UNLIKELY (chunkCnt == 1) {
						chunks[0]->lock(true);
						func(*chunks[0]);
						chunks[0]->lock(false);
						chunks.clear();
						return;
					}

					// We have many chunks to process.
					// Chunks might be located at different memory locations. Not even in the same memory page.
					// Therefore, to make it easier for the CPU we give it a hint that we want to prefetch data
					// for the next chunk explictely so we do not end up stalling later.
					// Note, this is a micro optimization and on average it bring no performance benefit. It only
					// helps with edge cases.
					// Let us be conservative for now and go with T2. That means we will try to keep our data at
					// least in L3 cache or higher.
					gaia::prefetch(&chunks[1], PrefetchHint::PREFETCH_HINT_T2);
					chunks[0]->lock(true);
					func(*chunks[0]);
					chunks[0]->lock(false);

					uint32_t chunkIdx = 1;
					for (; chunkIdx < chunkCnt - 1; ++chunkIdx) {
						gaia::prefetch(&chunks[chunkIdx + 1], PrefetchHint::PREFETCH_HINT_T2);
						chunks[chunkIdx]->lock(true);
						func(*chunks[chunkIdx]);
						chunks[chunkIdx]->lock(false);
					}

					chunks[chunkIdx]->lock(true);
					func(*chunks[chunkIdx]);
					chunks[chunkIdx]->lock(false);

					chunks.clear();
				}

				template <bool HasFilters, typename Iter, typename Func>
				void run_query(
						const QueryInfo& queryInfo, Func func, ChunkBatchedList& chunkBatch, const cnt::darray<Chunk*>& chunks) {
					GAIA_PROF_SCOPE(run_query); // batch preparation + chunk processing

					uint32_t chunkOffset = 0;
					uint32_t itemsLeft = chunks.size();
					while (itemsLeft > 0) {
						const auto maxBatchSize = chunkBatch.max_size() - chunkBatch.size();
						const auto batchSize = itemsLeft > maxBatchSize ? maxBatchSize : itemsLeft;

						ChunkSpanMut chunkSpan((Chunk**)&chunks[chunkOffset], batchSize);
						for (auto* pChunk: chunkSpan) {
							Iter iter(*pChunk);
							if (iter.size() == 0)
								continue;

							if constexpr (HasFilters) {
								if (!match_filters(*pChunk, queryInfo))
									continue;
							}

							chunkBatch.push_back(pChunk);
						}

						if GAIA_UNLIKELY (chunkBatch.size() == chunkBatch.max_size())
							run_func_batched(func, chunkBatch);

						itemsLeft -= batchSize;
						chunkOffset += batchSize;
					}
				}

				template <typename Iter, typename Func>
				void run_query_on_chunks(QueryInfo& queryInfo, Func func) {
					// Update the world version
					update_version(*m_worldVersion);

					ChunkBatchedList chunkBatch;

					const bool hasFilters = queryInfo.has_filters();
					if (hasFilters) {
						for (auto* pArchetype: queryInfo)
							run_query<true, Iter>(queryInfo, func, chunkBatch, pArchetype->chunks());
					} else {
						for (auto* pArchetype: queryInfo)
							run_query<false, Iter>(queryInfo, func, chunkBatch, pArchetype->chunks());
					}

					if (!chunkBatch.empty())
						run_func_batched(func, chunkBatch);

					// Update the query version with the current world's version
					queryInfo.set_world_version(*m_worldVersion);
				}

				template <typename Iter, typename Func, typename... T>
				GAIA_FORCEINLINE void
				run_query_on_chunk(Chunk& chunk, Func func, [[maybe_unused]] core::func_type_list<T...> types) {
					if constexpr (sizeof...(T) > 0) {
						Iter iter(chunk);

						// Pointers to the respective component types in the chunk, e.g
						// 		q.each([&](Position& p, const Velocity& v) {...}
						// Translates to:
						//  	auto p = iter.view_mut_inter<Position, true>();
						//		auto v = iter.view_inter<Velocity>();
						auto dataPointerTuple = std::make_tuple(iter.template view_auto<T>()...);

						// Iterate over each entity in the chunk.
						// Translates to:
						//		GAIA_EACH(iter) func(p[i], v[i]);

						GAIA_EACH(iter) func(std::get<decltype(iter.template view_auto<T>())>(dataPointerTuple)[i]...);
					} else {
						// No functor parameters. Do an empty loop.
						Iter iter(chunk);
						GAIA_EACH(iter) func();
					}
				}

				void invalidate() {
					if constexpr (UseCaching)
						m_storage.m_queryId = QueryIdBad;
				}

				template <bool UseFilters, typename Iter>
				GAIA_NODISCARD bool empty_inter(const QueryInfo& queryInfo, const cnt::darray<Chunk*>& chunks) {
					return core::has_if(chunks, [&](Chunk* pChunk) {
						Iter iter(*pChunk);
						if constexpr (UseFilters)
							return iter.size() > 0 && match_filters(*pChunk, queryInfo);
						else
							return iter.size() > 0;
					});
				}

				template <bool UseFilters, typename Iter>
				GAIA_NODISCARD uint32_t count_inter(const QueryInfo& queryInfo, const cnt::darray<Chunk*>& chunks) {
					uint32_t cnt = 0;

					for (auto* pChunk: chunks) {
						Iter iter(*pChunk);
						const auto entityCnt = iter.size();
						if (entityCnt == 0)
							continue;

						// Filters
						if constexpr (UseFilters) {
							if (!match_filters(*pChunk, queryInfo))
								continue;
						}

						// Entity count
						cnt += entityCnt;
					}

					return cnt;
				}

				template <bool UseFilters, typename Iter, typename ContainerOut>
				void arr_inter(const QueryInfo& queryInfo, const cnt::darray<Chunk*>& chunks, ContainerOut& outArray) {
					using ContainerItemType = typename ContainerOut::value_type;

					for (auto* pChunk: chunks) {
						Iter iter(*pChunk);
						if (iter.size() == 0)
							continue;

						// Filters
						if constexpr (UseFilters) {
							if (!match_filters(*pChunk, queryInfo))
								continue;
						}

						const auto dataView = iter.template view<ContainerItemType>();
						GAIA_EACH(iter) outArray.push_back(dataView[i]);
					}
				}

			public:
				QueryImpl() = default;

				template <bool FuncEnabled = UseCaching>
				QueryImpl(
						QueryCache& queryCache, ArchetypeId& nextArchetypeId, uint32_t& worldVersion,
						const cnt::map<ArchetypeId, Archetype*>& archetypes,
						const ComponentIdToArchetypeMap& componentToArchetypeMap):
						m_nextArchetypeId(&nextArchetypeId),
						m_worldVersion(&worldVersion), m_archetypes(&archetypes),
						m_componentToArchetypeMap(&componentToArchetypeMap) {
					m_storage.m_entityQueryCache = &queryCache;
				}

				template <bool FuncEnabled = !UseCaching>
				QueryImpl(
						ArchetypeId& nextArchetypeId, uint32_t& worldVersion, const cnt::map<ArchetypeId, Archetype*>& archetypes,
						const ComponentIdToArchetypeMap& componentToArchetypeMap):
						m_nextArchetypeId(&nextArchetypeId),
						m_worldVersion(&worldVersion), m_archetypes(&archetypes),
						m_componentToArchetypeMap(&componentToArchetypeMap) {}

				GAIA_NODISCARD uint32_t id() const {
					static_assert(UseCaching, "id() can be used only with cached queries");
					return m_storage.m_queryId;
				}

				template <typename... T>
				QueryImpl& all() {
					// Adding new rules invalidates the query
					invalidate();
					// Add commands to the command buffer
					(add_inter<T>(QueryListType::LT_All), ...);
					return *this;
				}

				template <typename... T>
				QueryImpl& any() {
					// Adding new rules invalidates the query
					invalidate();
					// Add commands to the command buffer
					(add_inter<T>(QueryListType::LT_Any), ...);
					return *this;
				}

				template <typename... T>
				QueryImpl& none() {
					// Adding new rules invalidates the query
					invalidate();
					// Add commands to the command buffer
					(add_inter<T>(QueryListType::LT_None), ...);
					return *this;
				}

				template <typename... T>
				QueryImpl& changed() {
					// Adding new rules invalidates the query
					invalidate();
					// Add commands to the command buffer
					(changed_inter<T>(), ...);
					return *this;
				}

				QueryImpl& sched() {
					m_executionMode = QueryExecMode::Single;
					return *this;
				}

				QueryImpl& sched_par() {
					m_executionMode = QueryExecMode::Parallel;
					return *this;
				}

				template <typename Func>
				void each(QueryInfo& queryInfo, Func func) {
					using InputArgs = decltype(core::func_args(&Func::operator()));

#if GAIA_DEBUG
					// Make sure we only use components specified in the query.
					// Constness is respected. Therefore, if a type is const when registered to query,
					// it has to be const (or immutable) also in each().
					// in query.
					// Example 1:
					//   auto q = w.query().all<MyType>(); // immutable access requested
					//   q.each([](MyType val)) {}); // okay
					//   q.each([](const MyType& val)) {}); // okay
					//   q.each([](MyType& val)) {}); // error
					// Example 2:
					//   auto q = w.query().all<MyType&>(); // mutable access requested
					//   q.each([](MyType val)) {}); // error
					//   q.each([](const MyType& val)) {}); // error
					//   q.each([](MyType& val)) {}); // okay
					GAIA_ASSERT(unpack_args_into_query_has_all(queryInfo, InputArgs{}));
#endif

					run_query_on_chunks<Iterator>(queryInfo, [&](Chunk& chunk) {
						run_query_on_chunk<Iterator>(chunk, func, InputArgs{});
					});
				}

				template <typename Func>
				void each(Func func) {
					auto& queryInfo = fetch_query_info();

					if constexpr (std::is_invocable_v<Func, IteratorAll>)
						run_query_on_chunks<IteratorAll>(queryInfo, [&](Chunk& chunk) {
							func(IteratorAll(chunk));
						});
					else if constexpr (std::is_invocable_v<Func, Iterator>)
						run_query_on_chunks<Iterator>(queryInfo, [&](Chunk& chunk) {
							func(Iterator(chunk));
						});
					else if constexpr (std::is_invocable_v<Func, IteratorDisabled>)
						run_query_on_chunks<IteratorDisabled>(queryInfo, [&](Chunk& chunk) {
							func(IteratorDisabled(chunk));
						});
					else
						each(queryInfo, func);
				}

				template <typename Func, bool FuncEnabled = UseCaching, typename std::enable_if<FuncEnabled>::type* = nullptr>
				void each(QueryId queryId, Func func) {
					// Make sure the query was created by World.query()
					GAIA_ASSERT(m_storage.m_entityQueryCache != nullptr);
					GAIA_ASSERT(queryId != QueryIdBad);

					auto& queryInfo = m_storage.m_entityQueryCache->get(queryId);
					each(queryInfo, func);
				}

				/*!
					Returns true or false depending on whether there are any entities matching the query.
					\warning Only use if you only care if there are any entities matching the query.
									 The result is not cached and repeated calls to the function might be slow.
									 If you already called arr(), checking if it is empty is preferred.
									 Use empty() instead of calling count()==0.
					\return True if there are any entites matchine the query. False otherwise.
					*/
				bool empty(Constraints constraints = Constraints::EnabledOnly) {
					auto& queryInfo = fetch_query_info();
					const bool hasFilters = queryInfo.has_filters();

					if (hasFilters) {
						switch (constraints) {
							case Constraints::EnabledOnly: {
								for (auto* pArchetype: queryInfo)
									if (empty_inter<true, Iterator>(queryInfo, pArchetype->chunks()))
										return false;
							} break;
							case Constraints::DisabledOnly: {
								for (auto* pArchetype: queryInfo)
									if (empty_inter<true, IteratorDisabled>(queryInfo, pArchetype->chunks()))
										return false;
							} break;
							case Constraints::AcceptAll: {
								for (auto* pArchetype: queryInfo)
									if (empty_inter<true, IteratorAll>(queryInfo, pArchetype->chunks()))
										return false;
							} break;
						}
					} else {
						switch (constraints) {
							case Constraints::EnabledOnly: {
								for (auto* pArchetype: queryInfo)
									if (empty_inter<false, Iterator>(queryInfo, pArchetype->chunks()))
										return false;
							} break;
							case Constraints::DisabledOnly: {
								for (auto* pArchetype: queryInfo)
									if (empty_inter<false, IteratorDisabled>(queryInfo, pArchetype->chunks()))
										return false;
							} break;
							case Constraints::AcceptAll: {
								for (auto* pArchetype: queryInfo)
									if (empty_inter<false, IteratorAll>(queryInfo, pArchetype->chunks()))
										return false;
							} break;
						}
					}

					return true;
				}

				/*!
				Calculates the number of entities matching the query
				\warning Only use if you only care about the number of entities matching the query.
								 The result is not cached and repeated calls to the function might be slow.
								 If you already called arr(), use the size provided by the array.
								 Use empty() instead of calling count()==0.
				\return The number of matching entities
				*/
				uint32_t count(Constraints constraints = Constraints::EnabledOnly) {
					auto& queryInfo = fetch_query_info();
					uint32_t entCnt = 0;

					const bool hasFilters = queryInfo.has_filters();
					if (hasFilters) {
						switch (constraints) {
							case Constraints::EnabledOnly: {
								for (auto* pArchetype: queryInfo)
									entCnt += count_inter<true, Iterator>(queryInfo, pArchetype->chunks());
							} break;
							case Constraints::DisabledOnly: {
								for (auto* pArchetype: queryInfo)
									entCnt += count_inter<true, IteratorDisabled>(queryInfo, pArchetype->chunks());
							} break;
							case Constraints::AcceptAll: {
								for (auto* pArchetype: queryInfo)
									entCnt += count_inter<true, IteratorAll>(queryInfo, pArchetype->chunks());
							} break;
						}
					} else {
						switch (constraints) {
							case Constraints::EnabledOnly: {
								for (auto* pArchetype: queryInfo)
									entCnt += count_inter<false, Iterator>(queryInfo, pArchetype->chunks());
							} break;
							case Constraints::DisabledOnly: {
								for (auto* pArchetype: queryInfo)
									entCnt += count_inter<false, IteratorDisabled>(queryInfo, pArchetype->chunks());
							} break;
							case Constraints::AcceptAll: {
								for (auto* pArchetype: queryInfo)
									entCnt += count_inter<false, IteratorAll>(queryInfo, pArchetype->chunks());
							} break;
						}
					}

					return entCnt;
				}

				/*!
				Appends all components or entities matching the query to the output array
				\tparam outArray Container storing entities or components
				\param constraints QueryImpl constraints
				\return Array with entities or components
				*/
				template <typename Container>
				void arr(Container& outArray, Constraints constraints = Constraints::EnabledOnly) {
					const auto entCnt = count();
					if (entCnt == 0)
						return;

					outArray.reserve(entCnt);
					auto& queryInfo = fetch_query_info();

					const bool hasFilters = queryInfo.has_filters();
					if (hasFilters) {
						switch (constraints) {
							case Constraints::EnabledOnly:
								for (auto* pArchetype: queryInfo)
									arr_inter<true, Iterator>(queryInfo, pArchetype->chunks(), outArray);
								break;
							case Constraints::DisabledOnly:
								for (auto* pArchetype: queryInfo)
									arr_inter<true, IteratorDisabled>(queryInfo, pArchetype->chunks(), outArray);
								break;
							case Constraints::AcceptAll:
								for (auto* pArchetype: queryInfo)
									arr_inter<true, IteratorAll>(queryInfo, pArchetype->chunks(), outArray);
								break;
						}
					} else {
						switch (constraints) {
							case Constraints::EnabledOnly:
								for (auto* pArchetype: queryInfo)
									arr_inter<false, Iterator>(queryInfo, pArchetype->chunks(), outArray);
								break;
							case Constraints::DisabledOnly:
								for (auto* pArchetype: queryInfo)
									arr_inter<false, IteratorDisabled>(queryInfo, pArchetype->chunks(), outArray);
								break;
							case Constraints::AcceptAll:
								for (auto* pArchetype: queryInfo)
									arr_inter<false, IteratorAll>(queryInfo, pArchetype->chunks(), outArray);
								break;
						}
					}
				}
			};
		} // namespace detail

		using Query = typename detail::QueryImpl<true>;
		using QueryUncached = typename detail::QueryImpl<false>;
	} // namespace ecs
} // namespace gaia
