#pragma once

#include "gaia/config/config.h"

#include "gaia/cnt/map.h"
#include "gaia/ecs/id.h"
#include "gaia/ecs/query.h"
#include "gaia/util/move_func.h"

#if GAIA_SYSTEMS_ENABLED
namespace gaia {
	namespace ecs {
		//! Runtime payload for systems kept out-of-line from ECS component storage.
		struct SystemRuntimeData {
			using TSystemExecFunc = util::MoveFunc<void(Query&, QueryExecType)>;

			//! Called every time the system is allowed to tick.
			TSystemExecFunc on_each_func;
		};

		//! Runtime storage for system callbacks kept out-of-line from ECS component storage.
		class SystemRegistry {
			cnt::map<EntityLookupKey, SystemRuntimeData> m_system_data;

		public:
			void teardown() {
				for (auto& it: m_system_data)
					it.second.on_each_func = {};

				m_system_data = {};
			}

			SystemRuntimeData& data_add(Entity system) {
				return m_system_data[EntityLookupKey(system)];
			}

			GAIA_NODISCARD SystemRuntimeData* data_try(Entity system) {
				const auto it = m_system_data.find(EntityLookupKey(system));
				if (it == m_system_data.end())
					return nullptr;
				return &it->second;
			}

			GAIA_NODISCARD const SystemRuntimeData* data_try(Entity system) const {
				const auto it = m_system_data.find(EntityLookupKey(system));
				if (it == m_system_data.end())
					return nullptr;
				return &it->second;
			}

			GAIA_NODISCARD SystemRuntimeData& data(Entity system) {
				auto* pData = data_try(system);
				GAIA_ASSERT(pData != nullptr);
				return *pData;
			}

			GAIA_NODISCARD const SystemRuntimeData& data(Entity system) const {
				const auto* pData = data_try(system);
				GAIA_ASSERT(pData != nullptr);
				return *pData;
			}

			void del(Entity system) {
				const auto it = m_system_data.find(EntityLookupKey(system));
				if (it == m_system_data.end())
					return;

				it->second.on_each_func = {};
				m_system_data.erase(it);
			}
		};
	} // namespace ecs
} // namespace gaia
#endif
