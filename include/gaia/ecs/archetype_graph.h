#pragma once

#include <cinttypes>

#include "../cnt/map.h"
#include "../config/logging.h"
#include "archetype_common.h"
#include "component.h"
#include "component_cache.h"

namespace gaia {
	namespace ecs {
		class ArchetypeGraph {
			struct ArchetypeGraphEdge {
				ArchetypeId archetypeId;
			};

			//! Map of edges in the archetype graph when adding components
			cnt::map<ComponentId, ArchetypeGraphEdge> m_edgesAdd[ComponentKind::CK_Count];
			//! Map of edges in the archetype graph when removing components
			cnt::map<ComponentId, ArchetypeGraphEdge> m_edgesDel[ComponentKind::CK_Count];

		public:
			//! Creates an edge in the graph leading to the target archetype \param archetypeId via component \param
			//! compId of type \param compKind.
			void add_edge_right(ComponentKind compKind, ComponentId compId, ArchetypeId archetypeId) {
				[[maybe_unused]] const auto ret = m_edgesAdd[compKind].try_emplace({compId}, ArchetypeGraphEdge{archetypeId});
				GAIA_ASSERT(ret.second);
			}

			//! Creates an edge in the graph leading to the target archetype \param archetypeId via component \param
			//! compId of type \param compKind.
			void add_edge_left(ComponentKind compKind, ComponentId compId, ArchetypeId archetypeId) {
				[[maybe_unused]] const auto ret = m_edgesDel[compKind].try_emplace({compId}, ArchetypeGraphEdge{archetypeId});
				GAIA_ASSERT(ret.second);
			}

			//! Checks if the graph edge for component type \param compKind contains the component \param compId.
			//! \return Archetype id of the target archetype if the edge is found. ArchetypeIdBad otherwise.
			GAIA_NODISCARD ArchetypeId find_edge_right(ComponentKind compKind, const ComponentId compId) const {
				const auto& edges = m_edgesAdd[compKind];
				const auto it = edges.find({compId});
				return it != edges.end() ? it->second.archetypeId : ArchetypeIdBad;
			}

			//! Checks if the graph edge for component type \param compKind contains the component \param compId.
			//! \return Archetype id of the target archetype if the edge is found. ArchetypeIdBad otherwise.
			GAIA_NODISCARD ArchetypeId find_edge_left(ComponentKind compKind, const ComponentId compId) const {
				const auto& edges = m_edgesDel[compKind];
				const auto it = edges.find({compId});
				return it != edges.end() ? it->second.archetypeId : ArchetypeIdBad;
			}

			void diag() const {
				// const auto& cc = ComponentCache::get();

				// // Add edges (movement towards the leafs)
				// {
				// 	const auto& edgesG = m_edgesAdd[ComponentKind::CK_Generic];
				// 	const auto& edgesC = m_edgesAdd[ComponentKind::CK_Chunk];
				// 	const auto edgeCount = (uint32_t)(edgesG.size() + edgesC.size());
				// 	if (edgeCount > 0) {
				// 		GAIA_LOG_N("  Add edges - count:%u", edgeCount);

				// 		if (!edgesG.empty()) {
				// 			GAIA_LOG_N("    Generic - count:%u", (uint32_t)edgesG.size());
				// 			for (const auto& edge: edgesG) {
				// 				const auto& info = cc.comp_info(edge.first);
				// 				const auto& infoCreate = cc.comp_desc(info.compId);
				// 				GAIA_LOG_N(
				// 						"      %.*s (--> Archetype ID:%u)", (uint32_t)infoCreate.name.size(), infoCreate.name.data(),
				// 						edge.second.archetypeId);
				// 			}
				// 		}

				// 		if (!edgesC.empty()) {
				// 			GAIA_LOG_N("    Chunk - count:%u", (uint32_t)edgesC.size());
				// 			for (const auto& edge: edgesC) {
				// 				const auto& info = cc.comp_info(edge.first);
				// 				const auto& infoCreate = cc.comp_desc(info.compId);
				// 				GAIA_LOG_N(
				// 						"      %.*s (--> Archetype ID:%u)", (uint32_t)infoCreate.name.size(), infoCreate.name.data(),
				// 						edge.second.archetypeId);
				// 			}
				// 		}
				// 	}
				// }

				// // Delete edges (movement towards the root)
				// {
				// 	const auto& edgesG = m_edgesDel[ComponentKind::CK_Generic];
				// 	const auto& edgesC = m_edgesDel[ComponentKind::CK_Chunk];
				// 	const auto edgeCount = (uint32_t)(edgesG.size() + edgesC.size());
				// 	if (edgeCount > 0) {
				// 		GAIA_LOG_N("  del edges - count:%u", edgeCount);

				// 		if (!edgesG.empty()) {
				// 			GAIA_LOG_N("    Generic - count:%u", (uint32_t)edgesG.size());
				// 			for (const auto& edge: edgesG) {
				// 				const auto& info = cc.comp_info(edge.first);
				// 				const auto& infoCreate = cc.comp_desc(info.compId);
				// 				GAIA_LOG_N(
				// 						"      %.*s (--> Archetype ID:%u)", (uint32_t)infoCreate.name.size(), infoCreate.name.data(),
				// 						edge.second.archetypeId);
				// 			}
				// 		}

				// 		if (!edgesC.empty()) {
				// 			GAIA_LOG_N("    Chunk - count:%u", (uint32_t)edgesC.size());
				// 			for (const auto& edge: edgesC) {
				// 				const auto& info = cc.comp_info(edge.first);
				// 				const auto& infoCreate = cc.comp_desc(info.compId);
				// 				GAIA_LOG_N(
				// 						"      %.*s (--> Archetype ID:%u)", (uint32_t)infoCreate.name.size(), infoCreate.name.data(),
				// 						edge.second.archetypeId);
				// 			}
				// 		}
				// 	}
				// }
			}
		};
	} // namespace ecs
} // namespace gaia