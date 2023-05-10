#pragma once
#include "../config/config_core_end.h"

#include "../containers/darray.h"
#include "../containers/map.h"
#include "query_common.h"
#include "query_info.h"

namespace gaia {
	namespace ecs {
		class QueryCache {
			using QueryCacheLookupArray = containers::darray<uint32_t>;

			containers::map<query::LookupHash, QueryCacheLookupArray> m_queryCache;
			containers::darray<query::QueryInfo> m_queryArr;

		public:
			QueryCache() {
				m_queryArr.reserve(256);
			}

			~QueryCache() = default;

			QueryCache(QueryCache&&) = delete;
			QueryCache(const QueryCache&) = delete;
			QueryCache& operator=(QueryCache&&) = delete;
			QueryCache& operator=(const QueryCache&) = delete;

			//! Returns an already existing query info from the provided \param queryId.
			//! \warning It is expected that the query has already been registered. Undefined behavior otherwise.
			//! \param queryId Query used to search for query info
			//! \return Query info
			query::QueryInfo& Get(query::QueryId queryId) {
				GAIA_ASSERT(queryId != query::QueryIdBad);

				return m_queryArr[queryId];
			};

			//! Registers the provided query lookup context \param ctx. If it already exists it is returned.
			//! \return Query id
			uint32_t GetOrCreate(query::LookupCtx&& ctx) {
				GAIA_ASSERT(ctx.hashLookup.hash != 0);

				// Check if the query info exists first
				auto ret = m_queryCache.try_emplace(ctx.hashLookup, QueryCacheLookupArray{});
				if (!ret.second) {
					const auto& queryIds = ret.first->second;

					// Record with the query info lookup hash exists but we need to check if the query itself is a part of it.
					if GAIA_LIKELY (ctx.queryId != (int32_t)-1) {
						// Make sure the same hash gets us to the proper query
						for (const auto queryId: queryIds) {
							const auto& queryInfo = m_queryArr[queryId];
							if (queryInfo != ctx)
								continue;

							return queryId;
						}

						GAIA_ASSERT(false && "QueryInfo not found despite having its lookupHash and cacheId set!");
						return query::QueryIdBad;
					}
				}

				const auto queryId = (query::QueryId)m_queryArr.size();
				m_queryArr.push_back(query::QueryInfo::Create(queryId, std::move(ctx)));
				ret.first->second.push_back(queryId);
				return queryId;
			};
		};
	} // namespace ecs
} // namespace gaia