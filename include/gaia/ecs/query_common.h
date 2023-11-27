#pragma once

#include <type_traits>

#include "../cnt/darray.h"
#include "../cnt/sarray.h"
#include "../cnt/sarray_ext.h"
#include "../core/hashing_policy.h"
#include "component.h"
#include "component_utils.h"

namespace gaia {
	namespace ecs {
		//! Number of items that can be a part of Query
		static constexpr uint32_t MAX_ITEMS_IN_QUERY = 8U;

		//! Operation type
		enum class QueryOp : uint8_t { All, Any, Not, Count };
		//! Access type
		enum class QueryAccess : uint8_t { None, Read, Write };

		using QueryId = uint32_t;
		using QueryLookupHash = core::direct_hash_key<uint64_t>;
		using QueryEntityArray = cnt::sarray_ext<Entity, MAX_ITEMS_IN_QUERY>;
		using QueryOpArray = cnt::sarray_ext<QueryOp, MAX_ITEMS_IN_QUERY>;

		static constexpr QueryId QueryIdBad = (QueryId)-1;

		struct QueryItem {
			Entity entity;
			QueryOp op;
			QueryAccess access = QueryAccess::Read;
		};

		struct QueryCtx {
			ComponentCache* cc{};
			//! Lookup hash for this query
			QueryLookupHash hashLookup{};
			//! Query id
			QueryId queryId = QueryIdBad;

			struct Data {
				//! List of querried ids
				QueryEntityArray ids;
				//! Query operation types
				QueryOpArray ops;
				//! Sorted queried ids grouped by op
				QueryEntityArray ops_ids[(uint32_t)QueryOp::Count];
				//! Index of the last checked archetype in the component-to-archetype map
				uint32_t lastMatchedArchetypeIdx;
				//! List of filtered components
				QueryEntityArray withChanged;
				//! Read-write mask. Bit 0 stands for component 0 in component arrays.
				//! A set bit means write access is requested.
				uint8_t readWriteMask;
			} data{};
			// Make sure that MAX_ITEMS_IN_QUERY can fit into data.readWriteMask
			static_assert(MAX_ITEMS_IN_QUERY == 8);

			GAIA_NODISCARD bool operator==(const QueryCtx& other) const {
				// Comparison expected to be done only the first time the query is set up
				GAIA_ASSERT(queryId == QueryIdBad);
				// Fast path when cache ids are set
				// if (queryId != QueryIdBad && queryId == other.queryId)
				// 	return true;

				// Lookup hash must match
				if (hashLookup != other.hashLookup)
					return false;

				const auto& left = data;
				const auto& right = other.data;

				// Check array sizes first
				if (left.ids.size() != right.ids.size())
					return false;
				if (left.ops.size() != right.ops.size())
					return false;
				if (left.withChanged.size() != right.withChanged.size())
					return false;
				if (left.readWriteMask != right.readWriteMask)
					return false;

				// Components need to be the same
				if (left.ids != right.ids)
					return false;

				// Rules need to be the same
				if (left.ops != right.ops)
					return false;

				// Filters need to be the same
				if (left.withChanged != right.withChanged)
					return false;

				return true;
			}

			GAIA_NODISCARD bool operator!=(const QueryCtx& other) const {
				return !operator==(other);
			};
		};

		//! Sorts internal component arrays
		inline void sort(QueryCtx& ctx) {
			auto& data = ctx.data;

			// Sort data. Necessary for correct hash calculation.
			// Without sorting query.all<XXX, YYY> would be different than query.all<YYY, XXX>.
			core::sort(data.ids, SortComponentCond{}, [&](uint32_t left, uint32_t right) {
				core::swap(data.ids[left], data.ids[right]);
				core::swap(data.ops[left], data.ops[right]);

				// Make sure the read-write mask remains correct after sorting
				{
					// Swap the bits in the read-write mask
					const uint32_t b0 = (data.readWriteMask >> left) & 1U;
					const uint32_t b1 = (data.readWriteMask >> right) & 1U;
					// XOR the two bits
					const uint32_t bxor = b0 ^ b1;
					// Put the XOR bits back to their original positions
					const uint32_t mask = (bxor << left) | (bxor << right);
					// XOR mask with the original one effectivelly swapping the bits
					data.readWriteMask = data.readWriteMask ^ (uint8_t)mask;
				}
			});

			// Ids per op need to be sorted
			GAIA_FOR((uint32_t)QueryOp::Count) {
				core::sort(data.ops_ids[i], SortComponentCond{});
			}
		}

		inline void calc_lookup_hash(QueryCtx& ctx) {
			GAIA_ASSERT(ctx.cc != nullptr);
			// Make sure we don't calculate the hash twice
			GAIA_ASSERT(ctx.hashLookup.hash == 0);

			QueryLookupHash::Type hashLookup = 0;

			auto& data = ctx.data;

			// Components
			{
				QueryLookupHash::Type hash = 0;

				const auto& ids = data.ids;
				for (const auto comp: ids)
					hash = core::hash_combine(hash, (QueryLookupHash::Type)comp.value());
				hash = core::hash_combine(hash, (QueryLookupHash::Type)ids.size());

				hash = core::hash_combine(hash, (QueryLookupHash::Type)data.readWriteMask);
				hashLookup = core::hash_combine(hashLookup, hash);
			}

			// Operations
			{
				QueryLookupHash::Type hash = 0;

				const auto& ops = data.ops;
				for (auto op: ops)
					hash = core::hash_combine(hash, (QueryLookupHash::Type)op);
				hash = core::hash_combine(hash, (QueryLookupHash::Type)ops.size());

				hashLookup = core::hash_combine(hashLookup, hash);
			}

			// Filters
			{
				QueryLookupHash::Type hash = 0;

				const auto& withChanged = data.withChanged;
				for (auto comp: withChanged)
					hash = core::hash_combine(hash, (QueryLookupHash::Type)comp.value());
				hash = core::hash_combine(hash, (QueryLookupHash::Type)withChanged.size());

				hashLookup = core::hash_combine(hashLookup, hash);
			}

			ctx.hashLookup = {core::calculate_hash64(hashLookup)};
		}
	} // namespace ecs
} // namespace gaia