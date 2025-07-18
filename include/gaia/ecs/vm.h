#pragma once
#include "../config/config.h"

#include <cstdint>
#include <type_traits>

#include "../cnt/darray.h"
#include "../cnt/set.h"
#include "../config/profiler.h"
#include "../core/utility.h"
#include "api.h"
#include "archetype.h"
#include "archetype_common.h"
#include "data_buffer.h"
#include "id.h"
#include "query_common.h"
#include "query_mask.h"

namespace gaia {
	namespace ecs {
		using EntityToArchetypeMap = cnt::map<EntityLookupKey, ArchetypeDArray>;

		namespace vm {

			enum class MatchingStyle { Simple, Wildcard, Complex };

			struct MatchingCtx {
				// Setup up externally
				//////////////////////////////////

				//! World
				const World* pWorld;
				//! entity -> archetypes mapping
				const EntityToArchetypeMap* pEntityToArchetypeMap;
				//! Array of all archetypes in the world
				const ArchetypeDArray* pAllArchetypes;
				//! Set of already matched archetypes. Reset before each exec().
				cnt::set<Archetype*>* pMatchesSet;
				//! Array of already matches archetypes. Reset before each exec().
				ArchetypeDArray* pMatchesArr;
				//! Idx of the last matched archetype against the ALL opcode
				QueryArchetypeCacheIndexMap* pLastMatchedArchetypeIdx_All;
				//! Idx of the last matched archetype against the ANY opcode
				QueryArchetypeCacheIndexMap* pLastMatchedArchetypeIdx_Any;
				//! Idx of the last matched archetype against the NOT opcode
				QueryArchetypeCacheIndexMap* pLastMatchedArchetypeIdx_Not;
				//! Mask for speeding up simple query matching
				QueryMask queryMask;
				//! Mask for items with Is relationship pair.
				//! If the id is a pair, the first part (id) is written here.
				uint32_t as_mask_0;
				//! Mask for items with Is relationship pair.
				//! If the id is a pair, the second part (gen) is written here.
				uint32_t as_mask_1;
				//! Flags copied over from QueryCtx::Data
				uint8_t flags;

				// For the opcode compiler to modify
				//////////////////////////////////////

				//! Entity to match
				Entity ent;
				//! List of entity ids in a query to consider
				EntitySpan idsToMatch;
				//! Current stack position (program counter)
				uint32_t pc;
			};

			inline const ArchetypeDArray* fetch_archetypes_for_select(
					const EntityToArchetypeMap& map, const ArchetypeDArray& arr, Entity ent, const EntityLookupKey& key) {
				GAIA_ASSERT(key != EntityBadLookupKey);

				if (ent == Pair(All, All))
					return &arr;

				const auto it = map.find(key);
				if (it == map.end() || it->second.empty())
					return nullptr;

				return &it->second;
			}

			inline const ArchetypeDArray*
			fetch_archetypes_for_select(const EntityToArchetypeMap& map, const ArchetypeDArray& arr, Entity ent, Entity src) {
				GAIA_ASSERT(src != EntityBad);

				return fetch_archetypes_for_select(map, arr, ent, EntityLookupKey(src));
			}

			namespace detail {
				enum class EOpcode : uint8_t {
					//! X
					All_Simple,
					All_Wildcard,
					All_Complex,
					//! ?X
					Any_NoAll,
					Any_WithAll,
					//! !X
					Not_Simple,
					Not_Wildcard,
					Not_Complex
				};

				using VmLabel = uint16_t;

				struct CompiledOp {
					//! Opcode to execute
					EOpcode opcode;
					//! Stack position to go to if the opcode returns true
					VmLabel pc_ok;
					//! Stack position to go to if the opcode returns false
					VmLabel pc_fail;
				};

				struct QueryCompileCtx {
					cnt::darray<CompiledOp> ops;
					//! Array of ops that can be evaluated with a ALL opcode
					cnt::sarray_ext<Entity, MAX_ITEMS_IN_QUERY> ids_all;
					//! Array of ops that can be evaluated with a ANY opcode
					cnt::sarray_ext<Entity, MAX_ITEMS_IN_QUERY> ids_any;
					//! Array of ops that can be evaluated with a NOT opcode
					cnt::sarray_ext<Entity, MAX_ITEMS_IN_QUERY> ids_not;
				};

				inline uint32_t handle_last_archetype_match(
						QueryArchetypeCacheIndexMap* pCont, EntityLookupKey entityKey, const ArchetypeDArray* pSrcArchetype) {
					const auto cache_it = pCont->find(entityKey);
					uint32_t lastMatchedIdx = 0;
					if (cache_it == pCont->end())
						pCont->emplace(entityKey, pSrcArchetype->size());
					else {
						lastMatchedIdx = cache_it->second;
						cache_it->second = pSrcArchetype->size();
					}
					return lastMatchedIdx;
				}

				// Operator ALL (used by query::all)
				struct OpAll {
					static bool check_mask(const QueryMask& maskArchetype, const QueryMask& maskQuery) {
						return match_entity_mask(maskArchetype, maskQuery);
					}
					static void restart([[maybe_unused]] uint32_t& idx) {}
					static bool can_continue(bool hasMatch) {
						return hasMatch;
					}
					static bool eval(uint32_t expectedMatches, uint32_t totalMatches) {
						return expectedMatches == totalMatches;
					}
					static uint32_t
					handle_last_match(MatchingCtx& ctx, EntityLookupKey entityKey, const ArchetypeDArray* pSrcArchetype) {
						return handle_last_archetype_match(ctx.pLastMatchedArchetypeIdx_All, entityKey, pSrcArchetype);
					}
				};
				// Operator OR (used by query::any)
				struct OpAny {
					static bool check_mask(const QueryMask& maskArchetype, const QueryMask& maskQuery) {
						return match_entity_mask(maskArchetype, maskQuery);
					}
					static void restart([[maybe_unused]] uint32_t& idx) {}
					static bool can_continue(bool hasMatch) {
						return hasMatch;
					}
					static bool eval(uint32_t expectedMatches, uint32_t totalMatches) {
						(void)expectedMatches;
						return totalMatches > 0;
					}
					static uint32_t
					handle_last_match(MatchingCtx& ctx, EntityLookupKey entityKey, const ArchetypeDArray* pSrcArchetype) {
						return handle_last_archetype_match(ctx.pLastMatchedArchetypeIdx_Any, entityKey, pSrcArchetype);
					}
				};
				// Operator NOT (used by query::no)
				struct OpNo {
					static bool check_mask(const QueryMask& maskArchetype, const QueryMask& maskQuery) {
						return !match_entity_mask(maskArchetype, maskQuery);
					}
					static void restart(uint32_t& idx) {
						idx = 0;
					}
					static bool can_continue(bool hasMatch) {
						return !hasMatch;
					}
					static bool eval(uint32_t expectedMatches, uint32_t totalMatches) {
						(void)expectedMatches;
						return totalMatches == 0;
					}
					static uint32_t
					handle_last_match(MatchingCtx& ctx, EntityLookupKey entityKey, const ArchetypeDArray* pSrcArchetype) {
						return handle_last_archetype_match(ctx.pLastMatchedArchetypeIdx_Not, entityKey, pSrcArchetype);
					}
				};

				template <typename OpKind>
				inline bool match_inter_eval_matches(uint32_t queryIdMarches, uint32_t& outMatches) {
					const bool hadAnyMatches = queryIdMarches > 0;

					// We finished checking matches with an id from query.
					// We need to check if we have sufficient amount of results in the run.
					if (!OpKind::can_continue(hadAnyMatches))
						return false;

					// No matter the amount of matches we only care if at least one
					// match happened with the id from query.
					outMatches += (uint32_t)hadAnyMatches;
					return true;
				}

				//! Tries to match ids in \param queryIds with ids in \param archetypeIds given
				//! the comparison function \param func bool(Entity queryId, Entity archetypeId).
				//! \return True if there is a match, false otherwise.
				template <typename OpKind, typename CmpFunc>
				GAIA_NODISCARD inline bool match_inter(EntitySpan queryIds, EntitySpan archetypeIds, CmpFunc func) {
					const auto archetypeIdsCnt = (uint32_t)archetypeIds.size();
					const auto queryIdsCnt = (uint32_t)queryIds.size();

					// Arrays are sorted so we can do linear intersection lookup
					uint32_t indices[2]{}; // 0 for query ids, 1 for archetype ids
					uint32_t matches = 0;

					// Ids in query and archetype are sorted.
					// Therefore, to match any two ids we perform a linear intersection forward loop.
					// The only exception are transitive ids in which case we need to start searching
					// form the start.
					// Finding just one match for any id in the query is enough to start checking
					// the next it. We only have 3 different operations - ALL, OR, NOT.
					//
					// Example:
					// - query #1 ------------------------
					// queryIds    : 5, 10
					// archetypeIds: 1,  3,  5,  6,  7, 10
					// - query #2 ------------------------
					// queryIds    : 1, 10, 11
					// archetypeIds: 3,  5,  6,  7, 10, 15
					// -----------------------------------
					// indices[0]  : 0,  1,  2
					// indices[1]  : 0,  1,  2,  3,  4,  5
					//
					// For query #1:
					// We start matching 5 in the query with 1 in the archetype. They do not match.
					// We continue with 3 in the archetype. No match.
					// We continue with 5 in the archetype. Match.
					// We try to match 10 in the query with 6 in the archetype. No match.
					// ... etc.

					while (indices[0] < queryIdsCnt) {
						const auto idInQuery = queryIds[indices[0]];

						// For * and transitive ids we have to search from the start.
						if (idInQuery == All || idInQuery.id() == Is.id())
							indices[1] = 0;

						uint32_t queryIdMatches = 0;
						while (indices[1] < archetypeIdsCnt) {
							const auto idInArchetype = archetypeIds[indices[1]];

							// See if we have a match
							const auto res = func(idInQuery, idInArchetype);

							// Once a match is found we start matching with the next id in query.
							if (res.matched) {
								++indices[0];
								++indices[1];
								++queryIdMatches;

								// Only continue with the next iteration unless the given Op determines it is
								// no longer needed.
								if (!match_inter_eval_matches<OpKind>(queryIdMatches, matches))
									return false;

								goto next_query_id;
							} else {
								++indices[1];
							}
						}

						if (!match_inter_eval_matches<OpKind>(queryIdMatches, matches))
							return false;

						++indices[0];
						// Make sure to continue from the right index on the archetype array.
						// Some operators can keep moving forward (AND, ANY), but NOT needs to start
						// matching from the beginning again if the previous query operator didn't find a match.
						OpKind::restart(indices[1]);

					next_query_id:
						continue;
					}

					return OpKind::eval(queryIdsCnt, matches);
				}

				struct IdCmpResult {
					bool matched;
				};

				GAIA_NODISCARD inline IdCmpResult cmp_ids(Entity idInQuery, Entity idInArchetype) {
					return {idInQuery == idInArchetype};
				}

				GAIA_NODISCARD inline IdCmpResult cmp_ids_pairs(Entity idInQuery, Entity idInArchetype) {
					if (idInQuery.pair()) {
						// all(Pair<All, All>) aka "any pair"
						if (idInQuery == Pair(All, All))
							return {true};

						// all(Pair<X, All>):
						//   X, AAA
						//   X, BBB
						//   ...
						//   X, ZZZ
						if (idInQuery.gen() == All.id())
							return {idInQuery.id() == idInArchetype.id()};

						// all(Pair<All, X>):
						//   AAA, X
						//   BBB, X
						//   ...
						//   ZZZ, X
						if (idInQuery.id() == All.id())
							return {idInQuery.gen() == idInArchetype.gen()};
					}

					// 1:1 match needed for non-pairs
					return cmp_ids(idInQuery, idInArchetype);
				}

				GAIA_NODISCARD inline IdCmpResult
				cmp_ids_is(const World& w, const Archetype& archetype, Entity idInQuery, Entity idInArchetype) {
					// all(Pair<Is, X>)
					if (idInQuery.pair() && idInQuery.id() == Is.id()) {
						auto archetypeIds = archetype.ids_view();
						return {
								idInQuery.gen() == idInArchetype.id() || // X vs Id
								as_relations_trav_if(w, idInQuery, [&](Entity relation) {
									const auto idx = core::get_index(archetypeIds, relation);
									// Stop at the first match
									return idx != BadIndex;
								})};
					}

					// 1:1 match needed for non-pairs
					return cmp_ids(idInQuery, idInArchetype);
				}

				GAIA_NODISCARD inline IdCmpResult
				cmp_ids_is_pairs(const World& w, const Archetype& archetype, Entity idInQuery, Entity idInArchetype) {
					if (idInQuery.pair()) {
						// all(Pair<All, All>) aka "any pair"
						if (idInQuery == Pair(All, All))
							return {true};

						// all(Pair<Is, X>)
						if (idInQuery.id() == Is.id()) {
							// (Is, X) in archetype == (Is, X) in query
							if (idInArchetype == idInQuery)
								return {true};

							auto archetypeIds = archetype.ids_view();
							const auto eQ = entity_from_id(w, idInQuery.gen());
							if (eQ == idInArchetype)
								return {true};

							// If the archetype entity is an (Is, X) pair treat Is as X and try matching it with
							// entities inheriting from e.
							if (idInArchetype.id() == Is.id()) {
								const auto eA = entity_from_id(w, idInArchetype.gen());
								if (eA == eQ)
									return {true};

								return {as_relations_trav_if(w, eQ, [eA](Entity relation) {
									return eA == relation;
								})};
							}

							// Archetype entity is generic, try matching it with entities inheriting from e.
							return {as_relations_trav_if(w, eQ, [&archetypeIds](Entity relation) {
								// Relation does not necessary match the sorted order of components in the archetype
								// so we need to search through all of its ids.
								const auto idx = core::get_index(archetypeIds, relation);
								// Stop at the first match
								return idx != BadIndex;
							})};
						}

						// all(Pair<All, X>):
						//   AAA, X
						//   BBB, X
						//   ...
						//   ZZZ, X
						if (idInQuery.id() == All.id()) {
							if (idInQuery.gen() == idInArchetype.gen())
								return {true};

							// If there are any Is pairs on the archetype we need to check if we match them
							if (archetype.pairs_is() > 0) {
								auto archetypeIds = archetype.ids_view();

								const auto e = entity_from_id(w, idInQuery.gen());
								return {as_relations_trav_if(w, e, [&](Entity relation) {
									// Relation does not necessary match the sorted order of components in the archetype
									// so we need to search through all of its ids.
									const auto idx = core::get_index(archetypeIds, relation);
									// Stop at the first match
									return idx != BadIndex;
								})};
							}

							// No match found
							return {false};
						}

						// all(Pair<X, All>):
						//   X, AAA
						//   X, BBB
						//   ...
						//   X, ZZZ
						if (idInQuery.gen() == All.id()) {
							return {idInQuery.id() == idInArchetype.id()};
						}
					}

					// 1:1 match needed for non-pairs
					return cmp_ids(idInQuery, idInArchetype);
				}

				//! Tries to match entity ids in \param queryIds with those in \param archetype given
				//! the comparison function \param func. Does not consider Is relationships.
				//! \return True on the first match, false otherwise.
				template <typename OpKind>
				GAIA_NODISCARD inline bool match_res(const Archetype& archetype, EntitySpan queryIds) {
					// Archetype has no pairs we can compare ids directly.
					// This has better performance.
					if (archetype.pairs() == 0) {
						return match_inter<OpKind>(
								queryIds, archetype.ids_view(),
								// Cmp func
								[](Entity idInQuery, Entity idInArchetype) {
									return cmp_ids(idInQuery, idInArchetype);
								});
					}

					// Pairs are present, we have to evaluate.
					return match_inter<OpKind>(
							queryIds, archetype.ids_view(),
							// Cmp func
							[](Entity idInQuery, Entity idInArchetype) {
								return cmp_ids_pairs(idInQuery, idInArchetype);
							});
				}

				//! Tries to match entity ids in \param queryIds with those in \param archetype given
				//! the comparison function \param func. Considers Is relationships.
				//! \return True on the first match, false otherwise.
				template <typename OpKind>
				GAIA_NODISCARD inline bool match_res_as(const World& w, const Archetype& archetype, EntitySpan queryIds) {
					// Archetype has no pairs we can compare ids directly
					if (archetype.pairs() == 0) {
						return match_inter<OpKind>(
								queryIds, archetype.ids_view(),
								// cmp func
								[&](Entity idInQuery, Entity idInArchetype) {
									return cmp_ids_is(w, archetype, idInQuery, idInArchetype);
								});
					}

					return match_inter<OpKind>(
							queryIds, archetype.ids_view(),
							// cmp func
							[&](Entity idInQuery, Entity idInArchetype) {
								return cmp_ids_is_pairs(w, archetype, idInQuery, idInArchetype);
							});
				}

				template <typename OpKind, MatchingStyle Style>
				inline void
				match_archetype_inter(MatchingCtx& ctx, EntityLookupKey entityKey, const ArchetypeDArray* pSrcArchetypes) {
					const auto& archetypes = *pSrcArchetypes;
					auto& matchesArr = *ctx.pMatchesArr;
					auto& matchesSet = *ctx.pMatchesSet;

					uint32_t lastMatchedIdx = OpKind::handle_last_match(ctx, entityKey, pSrcArchetypes);
					if (lastMatchedIdx >= archetypes.size())
						return;

					auto archetypeSpan = std::span(&archetypes[lastMatchedIdx], archetypes.size() - lastMatchedIdx);

					if constexpr (Style == MatchingStyle::Complex) {
						for (auto* pArchetype: archetypeSpan) {
							if (matchesSet.contains(pArchetype))
								continue;

							if (!match_res_as<OpKind>(*ctx.pWorld, *pArchetype, ctx.idsToMatch))
								continue;

							matchesSet.emplace(pArchetype);
							matchesArr.emplace_back(pArchetype);
						}
					}
#if GAIA_USE_PARTITIONED_BLOOM_FILTER >= 0
					else if constexpr (Style == MatchingStyle::Simple) {
						for (auto* pArchetype: archetypeSpan) {
							if (matchesSet.contains(pArchetype))
								continue;

							// Try early exit
							if (!OpKind::check_mask(pArchetype->queryMask(), ctx.queryMask))
								continue;

							if (!match_res<OpKind>(*pArchetype, ctx.idsToMatch))
								continue;

							matchesSet.emplace(pArchetype);
							matchesArr.emplace_back(pArchetype);
						}
					}
#endif
					else {
						for (auto* pArchetype: archetypeSpan) {
							if (matchesSet.contains(pArchetype))
								continue;

							if (!match_res<OpKind>(*pArchetype, ctx.idsToMatch))
								continue;

							matchesSet.emplace(pArchetype);
							matchesArr.emplace_back(pArchetype);
						}
					}
				}

				template <MatchingStyle Style>
				inline void match_archetype_all(MatchingCtx& ctx) {
					if constexpr (Style == MatchingStyle::Complex) {
						// For ALL we need all the archetypes to match. We start by checking
						// if the first one is registered in the world at all.
						const auto& allArchetypes = *ctx.pAllArchetypes;

						if (ctx.ent.id() == Is.id()) {
							ctx.ent = EntityBad;
							const auto* pArchetypes = &allArchetypes;

							match_archetype_inter<OpAll, Style>(ctx, EntityBadLookupKey, pArchetypes);
						} else {
							auto entityKey = EntityLookupKey(ctx.ent);

							const auto* pArchetypes =
									fetch_archetypes_for_select(*ctx.pEntityToArchetypeMap, *ctx.pAllArchetypes, ctx.ent, entityKey);
							if (pArchetypes == nullptr)
								return;

							match_archetype_inter<OpAll, Style>(ctx, entityKey, pArchetypes);
						}
					} else {
						auto entityKey = EntityLookupKey(ctx.ent);

						// For ALL we need all the archetypes to match. We start by checking
						// if the first one is registered in the world at all.
						const auto* pArchetypes =
								fetch_archetypes_for_select(*ctx.pEntityToArchetypeMap, *ctx.pAllArchetypes, ctx.ent, entityKey);
						if (pArchetypes == nullptr)
							return;

						match_archetype_inter<OpAll, Style>(ctx, entityKey, pArchetypes);
					}
				}

				inline void match_archetype_one(MatchingCtx& ctx) {
					EntityLookupKey entityKey(ctx.ent);

					// For ANY we need at least one archetype to match.
					// However, because any of them can match, we need to check them all.
					// Iterating all of them is caller's responsibility.
					const auto* pArchetypes =
							fetch_archetypes_for_select(*ctx.pEntityToArchetypeMap, *ctx.pAllArchetypes, ctx.ent, entityKey);
					if (pArchetypes == nullptr)
						return;

					// TODO: Support MatchingStyle::Simple too
					match_archetype_inter<OpAny, MatchingStyle::Wildcard>(ctx, entityKey, pArchetypes);
				}

				inline void match_archetype_one_as(MatchingCtx& ctx) {
					const auto& allArchetypes = *ctx.pAllArchetypes;

					// For ANY we need at least one archetype to match.
					// However, because any of them can match, we need to check them all.
					// Iterating all of them is caller's responsibility.

					const ArchetypeDArray* pSrcArchetypes = nullptr;

					EntityLookupKey entityKey = EntityBadLookupKey;

					if (ctx.ent.id() == Is.id()) {
						ctx.ent = EntityBad;
						pSrcArchetypes = &allArchetypes;
					} else {
						entityKey = EntityLookupKey(ctx.ent);

						pSrcArchetypes =
								fetch_archetypes_for_select(*ctx.pEntityToArchetypeMap, *ctx.pAllArchetypes, ctx.ent, entityKey);
						if (pSrcArchetypes == nullptr)
							return;
					}

					match_archetype_inter<OpAny, MatchingStyle::Complex>(ctx, entityKey, pSrcArchetypes);
				}

				template <MatchingStyle Style>
				inline void match_archetype_no_2(MatchingCtx& ctx) {
					// We had some matches already (with ALL or ANY). We need to remove those
					// that match with the NO list.

					if constexpr (Style == MatchingStyle::Complex) {
						for (uint32_t i = 0; i < ctx.pMatchesArr->size();) {
							auto* pArchetype = (*ctx.pMatchesArr)[i];

							if (match_res_as<OpNo>(*ctx.pWorld, *pArchetype, ctx.idsToMatch)) {
								++i;
								continue;
							}

							core::swap_erase(*ctx.pMatchesArr, i);
						}
					}
#if GAIA_USE_PARTITIONED_BLOOM_FILTER >= 0
					else if constexpr (Style == MatchingStyle::Simple) {
						for (uint32_t i = 0; i < ctx.pMatchesArr->size();) {
							auto* pArchetype = (*ctx.pMatchesArr)[i];

							// Try early exit
							if (OpNo::check_mask(pArchetype->queryMask(), ctx.queryMask))
								continue;

							if (match_res<OpNo>(*pArchetype, ctx.idsToMatch)) {
								++i;
								continue;
							}

							core::swap_erase(*ctx.pMatchesArr, i);
						}
					}
#endif
					else {
						for (uint32_t i = 0; i < ctx.pMatchesArr->size();) {
							auto* pArchetype = (*ctx.pMatchesArr)[i];

							if (match_res<OpNo>(*pArchetype, ctx.idsToMatch)) {
								++i;
								continue;
							}

							core::swap_erase(*ctx.pMatchesArr, i);
						}
					}
				}

				struct OpcodeBaseData {
					EOpcode id;
				};

				struct OpcodeAll_Simple {
					static constexpr EOpcode Id = EOpcode::All_Simple;

					bool exec(const QueryCompileCtx& comp, MatchingCtx& ctx) {
						GAIA_PROF_SCOPE(vm::op_and_simple);

						ctx.ent = comp.ids_all[0];
						ctx.idsToMatch = std::span{comp.ids_all.data(), comp.ids_all.size()};

						match_archetype_all<MatchingStyle::Simple>(ctx);

						// If no ALL matches were found, we can quit right away.
						return !ctx.pMatchesArr->empty();
					}
				};

				struct OpcodeAll_Wildcard {
					static constexpr EOpcode Id = EOpcode::All_Wildcard;

					bool exec(const QueryCompileCtx& comp, MatchingCtx& ctx) {
						GAIA_PROF_SCOPE(vm::op_and_wildcard);

						ctx.ent = comp.ids_all[0];
						ctx.idsToMatch = std::span{comp.ids_all.data(), comp.ids_all.size()};

						match_archetype_all<MatchingStyle::Wildcard>(ctx);

						// If no ALL matches were found, we can quit right away.
						return !ctx.pMatchesArr->empty();
					}
				};

				struct OpcodeAll_Complex {
					static constexpr EOpcode Id = EOpcode::All_Complex;

					bool exec(const QueryCompileCtx& comp, MatchingCtx& ctx) {
						GAIA_PROF_SCOPE(vm::op_and_complex);

						ctx.ent = comp.ids_all[0];
						ctx.idsToMatch = std::span{comp.ids_all.data(), comp.ids_all.size()};

						match_archetype_all<MatchingStyle::Complex>(ctx);

						// If no ALL matches were found, we can quit right away.
						return !ctx.pMatchesArr->empty();
					}
				};

				struct OpcodeAny_NoAll {
					static constexpr EOpcode Id = EOpcode::Any_NoAll;

					bool exec(const QueryCompileCtx& comp, MatchingCtx& ctx) {
						GAIA_PROF_SCOPE(vm::op_any);

						const auto cnt = comp.ids_any.size();
						ctx.idsToMatch = std::span{comp.ids_any.data(), cnt};

						// Try find matches with optional components.
						GAIA_FOR(cnt) {
							ctx.ent = comp.ids_any[i];

							// First viable item is not related to an Is relationship
							if (ctx.as_mask_0 + ctx.as_mask_1 == 0U) {
								match_archetype_one(ctx);
							}
							// First viable item is related to an Is relationship.
							// In this case we need to gather all related archetypes.
							else {
								match_archetype_one_as(ctx);
							}
						}

						return true;
					}
				};

				struct OpcodeAny_WithAll {
					static constexpr EOpcode Id = EOpcode::Any_WithAll;

					bool exec(const QueryCompileCtx& comp, MatchingCtx& ctx) {
						GAIA_PROF_SCOPE(vm::op_any);

						ctx.idsToMatch = std::span{comp.ids_any.data(), comp.ids_any.size()};

						const bool hasNoAsRelationships = ctx.as_mask_0 + ctx.as_mask_1 == 0U;
#if GAIA_USE_PARTITIONED_BLOOM_FILTER >= 0
						const bool isSimple = (ctx.flags & QueryCtx::QueryFlags::Complex) == 0U;
#endif

						// We tried to match ALL items. Only search among archetypes we already found.
						// No last matched idx update is necessary because all ids were already checked
						// during the ALL pass.
						if (hasNoAsRelationships) {
#if GAIA_USE_PARTITIONED_BLOOM_FILTER >= 0
							if (isSimple) {
								for (uint32_t i = 0; i < ctx.pMatchesArr->size();) {
									auto* pArchetype = (*ctx.pMatchesArr)[i];

									GAIA_FOR_((uint32_t)comp.ids_any.size(), j) {
										// Try early exit
										if (!OpAny::check_mask(pArchetype->queryMask(), ctx.queryMask))
											goto checkNextArchetype3;

										// First viable item is not related to an Is relationship
										if (match_res<OpAny>(*pArchetype, ctx.idsToMatch))
											goto checkNextArchetype3;
									}

									// No match found among ANY. Remove the archetype from the matching ones
									core::swap_erase(*ctx.pMatchesArr, i);
									continue;

								checkNextArchetype3:
									++i;
								}
							} else
#endif
							{
								for (uint32_t i = 0; i < ctx.pMatchesArr->size();) {
									auto* pArchetype = (*ctx.pMatchesArr)[i];

									GAIA_FOR_((uint32_t)comp.ids_any.size(), j) {
										// First viable item is not related to an Is relationship
										if (match_res<OpAny>(*pArchetype, ctx.idsToMatch))
											goto checkNextArchetype1;

										// First viable item is related to an Is relationship.
										// In this case we need to gather all related archetypes.
										if (match_res_as<OpAny>(*ctx.pWorld, *pArchetype, ctx.idsToMatch))
											goto checkNextArchetype1;
									}

									// No match found among ANY. Remove the archetype from the matching ones
									core::swap_erase(*ctx.pMatchesArr, i);
									continue;

								checkNextArchetype1:
									++i;
								}
							}
						} else {
							for (uint32_t i = 0; i < ctx.pMatchesArr->size();) {
								auto* pArchetype = (*ctx.pMatchesArr)[i];

								GAIA_FOR_((uint32_t)comp.ids_any.size(), j) {
									// First viable item is related to an Is relationship.
									// In this case we need to gather all related archetypes.
									if (match_res_as<OpAny>(*ctx.pWorld, *pArchetype, ctx.idsToMatch))
										goto checkNextArchetype2;
								}

								// No match found among ANY. Remove the archetype from the matching ones
								core::swap_erase(*ctx.pMatchesArr, i);
								continue;

							checkNextArchetype2:
								++i;
							}
						}

						return true;
					}
				};

				struct OpcodeNot_Simple {
					static constexpr EOpcode Id = EOpcode::Not_Simple;

					bool exec(const QueryCompileCtx& comp, MatchingCtx& ctx) {
						GAIA_PROF_SCOPE(vm::op_not);

						ctx.idsToMatch = std::span{comp.ids_not.data(), comp.ids_not.size()};
						ctx.pMatchesSet->clear();

						// We searched for nothing more than NOT matches
						if (ctx.pMatchesArr->empty()) {
							// If there are no previous matches (no ALL or ANY matches),
							// we need to search among all archetypes.
							match_archetype_inter<detail::OpNo, MatchingStyle::Simple>(ctx, EntityBadLookupKey, ctx.pAllArchetypes);
						} else {
							match_archetype_no_2<MatchingStyle::Simple>(ctx);
						}

						return true;
					}
				};

				struct OpcodeNot_Wildcard {
					static constexpr EOpcode Id = EOpcode::Not_Wildcard;

					bool exec(const QueryCompileCtx& comp, MatchingCtx& ctx) {
						GAIA_PROF_SCOPE(vm::op_not);

						ctx.idsToMatch = std::span{comp.ids_not.data(), comp.ids_not.size()};
						ctx.pMatchesSet->clear();

						// We searched for nothing more than NOT matches
						if (ctx.pMatchesArr->empty()) {
							// If there are no previous matches (no ALL or ANY matches),
							// we need to search among all archetypes.
							match_archetype_inter<detail::OpNo, MatchingStyle::Wildcard>(ctx, EntityBadLookupKey, ctx.pAllArchetypes);
						} else {
							match_archetype_no_2<MatchingStyle::Wildcard>(ctx);
						}

						return true;
					}
				};

				struct OpcodeNot_Complex {
					static constexpr EOpcode Id = EOpcode::Not_Complex;

					bool exec(const QueryCompileCtx& comp, MatchingCtx& ctx) {
						GAIA_PROF_SCOPE(vm::op_not);

						ctx.idsToMatch = std::span{comp.ids_not.data(), comp.ids_not.size()};
						ctx.pMatchesSet->clear();

						// We searched for nothing more than NOT matches
						if (ctx.pMatchesArr->empty()) {
							// If there are no previous matches (no ALL or ANY matches),
							// we need to search among all archetypes.
							match_archetype_inter<detail::OpNo, MatchingStyle::Complex>(ctx, EntityBadLookupKey, ctx.pAllArchetypes);
						} else {
							match_archetype_no_2<MatchingStyle::Complex>(ctx);
						}

						return true;
					}
				};
			} // namespace detail

			class VirtualMachine {
				using OpcodeFunc = bool (*)(const detail::QueryCompileCtx& comp, MatchingCtx& ctx);
				struct Opcodes {
					OpcodeFunc exec;
				};

				static constexpr OpcodeFunc OpcodeFuncs[] = {
						// OpcodeAll_Simple
						[](const detail::QueryCompileCtx& comp, MatchingCtx& ctx) {
							detail::OpcodeAll_Simple op;
							return op.exec(comp, ctx);
						},
						// OpcodeAll_Wildcard
						[](const detail::QueryCompileCtx& comp, MatchingCtx& ctx) {
							detail::OpcodeAll_Wildcard op;
							return op.exec(comp, ctx);
						},
						// OpcodeAll_Complex
						[](const detail::QueryCompileCtx& comp, MatchingCtx& ctx) {
							detail::OpcodeAll_Complex op;
							return op.exec(comp, ctx);
						},
						// OpcodeAny_NoAll
						[](const detail::QueryCompileCtx& comp, MatchingCtx& ctx) {
							detail::OpcodeAny_NoAll op;
							return op.exec(comp, ctx);
						},
						// OpcodeAny_WithAll
						[](const detail::QueryCompileCtx& comp, MatchingCtx& ctx) {
							detail::OpcodeAny_WithAll op;
							return op.exec(comp, ctx);
						},
						// OpcodeNot_Simple
						[](const detail::QueryCompileCtx& comp, MatchingCtx& ctx) {
							detail::OpcodeNot_Simple op;
							return op.exec(comp, ctx);
						},
						// OpcodeNot_Wildcard
						[](const detail::QueryCompileCtx& comp, MatchingCtx& ctx) {
							detail::OpcodeNot_Wildcard op;
							return op.exec(comp, ctx);
						},
						// OpcodeNot_Complex
						[](const detail::QueryCompileCtx& comp, MatchingCtx& ctx) {
							detail::OpcodeNot_Complex op;
							return op.exec(comp, ctx);
						},
				};

				detail::QueryCompileCtx m_compCtx;

			private:
				GAIA_NODISCARD detail::VmLabel add_op(detail::CompiledOp&& op) {
					const auto cnt = (detail::VmLabel)m_compCtx.ops.size();
					op.pc_ok = cnt + 1;
					op.pc_fail = cnt - 1;
					m_compCtx.ops.push_back(GAIA_MOV(op));
					return cnt;
				}

			public:
				//! Transforms inputs into virtual machine opcodes.
				void compile(
						const EntityToArchetypeMap& entityToArchetypeMap, const ArchetypeDArray& allArchetypes,
						QueryCtx& queryCtx) {
					GAIA_PROF_SCOPE(vm::compile);

					auto& data = queryCtx.data;

					QueryTermSpan terms = data.terms_view();
					QueryTermSpan terms_all = terms.subspan(0, data.firstAny);
					QueryTermSpan terms_any = terms.subspan(data.firstAny, data.firstNot - data.firstAny);
					QueryTermSpan terms_not = terms.subspan(data.firstNot);

					// ALL
					if (!terms_all.empty()) {
						GAIA_PROF_SCOPE(vm::compile_all);

						const auto cnt = terms_all.size();
						GAIA_FOR(cnt) {
							auto& p = terms_all[i];
							if (p.src == EntityBad) {
								m_compCtx.ids_all.push_back(p.id);
								continue;
							}

							// Fixed source
							{
								p.srcArchetype = archetype_from_entity(*queryCtx.w, p.src);

								// Archetype needs to exist. If it does not we have nothing to do here.
								if (p.srcArchetype == nullptr) {
									m_compCtx.ops.clear();
									return;
								}
							}
						}
					}

					// ANY
					if (!terms_any.empty()) {
						GAIA_PROF_SCOPE(vm::compile_any);

						uint32_t archetypesWithId = 0;

						const auto cnt = terms_any.size();
						GAIA_FOR(cnt) {
							auto& p = terms_any[i];
							if (p.src != EntityBad) {
								p.srcArchetype = archetype_from_entity(*queryCtx.w, p.src);
								if (p.srcArchetype == nullptr)
									continue;
							}

							// Check if any archetype is associated with the entity id.
							// All ids must be registered in the world.
							const auto* pArchetypes =
									fetch_archetypes_for_select(entityToArchetypeMap, allArchetypes, p.id, EntityLookupKey(p.id));
							if (pArchetypes == nullptr)
								continue;

							++archetypesWithId;

							m_compCtx.ids_any.push_back(p.id);
						}

						// No archetypes with "any" entities exist. We can quit right away.
						if (archetypesWithId == 0) {
							m_compCtx.ops.clear();
							return;
						}
					}

					// NOT
					if (!terms_not.empty()) {
						GAIA_PROF_SCOPE(vm::compile_not);

						const auto cnt = terms_not.size();
						GAIA_FOR(cnt) {
							auto& p = terms_not[i];
							if (p.src != EntityBad)
								continue;

							m_compCtx.ids_not.push_back(p.id);
						}
					}

					create_opcodes(queryCtx);
				}

				void create_opcodes(QueryCtx& queryCtx) {
					const bool isSimple = (queryCtx.data.flags & QueryCtx::QueryFlags::Complex) == 0U;
					const bool isAs = (queryCtx.data.as_mask_0 + queryCtx.data.as_mask_1) != 0U;

					// Reset the list of opcodes
					m_compCtx.ops.clear();

					// ALL
					if (!m_compCtx.ids_all.empty()) {
						detail::CompiledOp op{};
						if (isSimple)
							op.opcode = detail::EOpcode::All_Simple;
						else if (isAs)
							op.opcode = detail::EOpcode::All_Complex;
						else
							op.opcode = detail::EOpcode::All_Wildcard;
						(void)add_op(GAIA_MOV(op));
					}

					// ANY
					if (!m_compCtx.ids_any.empty()) {
						detail::CompiledOp op{};
						op.opcode = m_compCtx.ids_all.empty() ? detail::EOpcode::Any_NoAll : detail::EOpcode::Any_WithAll;
						(void)add_op(GAIA_MOV(op));
					}

					// NOT
					if (!m_compCtx.ids_not.empty()) {
						detail::CompiledOp op{};
						if (isSimple)
							op.opcode = detail::EOpcode::Not_Simple;
						else if (isAs)
							op.opcode = detail::EOpcode::Not_Complex;
						else
							op.opcode = detail::EOpcode::Not_Wildcard;
						(void)add_op(GAIA_MOV(op));
					}

					// Mark as compiled
					queryCtx.data.flags &= ~QueryCtx::QueryFlags::Recompile;
				}

				GAIA_NODISCARD bool is_compiled() const {
					return !m_compCtx.ops.empty();
				}

				//! Executes compiled opcodes
				void exec(MatchingCtx& ctx) {
					GAIA_PROF_SCOPE(vm::exec);

					ctx.pc = 0;

					// Extract data from the buffer
					do {
						auto& stackItem = m_compCtx.ops[ctx.pc];
						const bool ret = OpcodeFuncs[(uint32_t)stackItem.opcode](m_compCtx, ctx);
						ctx.pc = ret ? stackItem.pc_ok : stackItem.pc_fail;
					} while (ctx.pc < m_compCtx.ops.size()); // (uint32_t)-1 falls in this category as well
				}
			};

		} // namespace vm
	} // namespace ecs
} // namespace gaia