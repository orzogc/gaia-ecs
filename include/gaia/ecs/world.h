#pragma once
#include "../config/config.h"

#include <cinttypes>
#include <type_traits>

#include "../cnt/darray.h"
#include "../cnt/ilist.h"
#include "../cnt/map.h"
#include "../cnt/sarray.h"
#include "../cnt/sarray_ext.h"
#include "../cnt/set.h"
#include "../config/profiler.h"
#include "../core/hashing_policy.h"
#include "../core/span.h"
#include "../core/utility.h"
#include "../meta/type_info.h"
#include "archetype.h"
#include "archetype_common.h"
#include "chunk.h"
#include "chunk_allocator.h"
#include "common.h"
#include "component.h"
#include "component_cache.h"
#include "component_getter.h"
#include "component_setter.h"
#include "component_utils.h"
#include "entity.h"
#include "query.h"
#include "query_cache.h"
#include "query_common.h"

namespace gaia {
	namespace ecs {
		class GAIA_API World final {
			friend class ECSSystem;
			friend class ECSSystemManager;
			friend class CommandBuffer;
			friend void* AllocateChunkMemory(World& world);
			friend void ReleaseChunkMemory(World& world, void* mem);

			//! Cache of queries
			QueryCache m_queryCache;
			//! Cache of query ids to speed up ForEach
			cnt::map<component::ComponentLookupHash, query::QueryId> m_uniqueFuncQueryPairs;
			//! Map of componentId -> archetype matches.
			query::ComponentToArchetypeMap m_componentToArchetypeMap;

			//! Map of archetypes mapping to the same hash - used for lookups
			cnt::map<archetype::ArchetypeLookupKey, archetype::Archetype*> m_archetypeMap;
			//! List of archetypes - used for iteration
			archetype::ArchetypeList m_archetypes;

			//! Implicit list of entities. Used for look-ups only when searching for
			//! entities in chunks + data validation
			cnt::ilist<EntityContainer, Entity> m_entities;

			//! List of chunks to delete
			cnt::darray<archetype::Chunk*> m_chunksToRemove;
#if !GAIA_AVOID_CHUNK_FRAGMENTATION
			//! ID of the last defragmented archetype
			uint32_t m_defragLastArchetypeID = 0;
#endif

			//! With every structural change world version changes
			uint32_t m_worldVersion = 0;

		private:
			//! Remove an entity from chunk.
			//! \param pChunk Chunk we remove the entity from
			//! \param entityChunkIndex Index of entity within its chunk
			//! \tparam IsEntityReleaseWanted True if entity is to be released as well. False otherwise.
			template <bool IsEntityReleaseWanted>
			void RemoveEntity(archetype::Chunk* pChunk, uint32_t entityChunkIndex) {
				GAIA_ASSERT(
						!pChunk->IsStructuralChangesLocked() && "Entities can't be removed while their chunk is being iterated "
																										"(structural changes are forbidden during this time!)");

				const auto chunkEntityCount = pChunk->GetEntityCount();
				if GAIA_UNLIKELY (chunkEntityCount == 0)
					return;

				GAIA_PROF_SCOPE(RemoveEntity);

				const auto entity = pChunk->GetEntity(entityChunkIndex);

#if GAIA_AVOID_CHUNK_FRAGMENTATION
				auto& archetype = *m_archetypes[pChunk->GetArchetypeId()];

				// Swap the last entity in the last chunk with the entity spot we just created by moving
				// the entity to somewhere else.
				auto* pOldChunk = archetype.FindFirstNonEmptyChunk();
				if (pOldChunk == pChunk) {
					const uint32_t lastEntityIdx = chunkEntityCount - 1;
					const bool wasDisabled = m_entities[entity.id()].dis;

					// Transfer data form the last entity to the new one
					pChunk->RemoveChunkEntity(entityChunkIndex, m_entities);
					pChunk->RemoveLastEntity(m_chunksToRemove);

					// Transfer the disabled state
					if GAIA_LIKELY (chunkEntityCount > 1)
						archetype.EnableEntity(pChunk, entityChunkIndex, !wasDisabled);
				} else if (pOldChunk != nullptr && pOldChunk->HasEntities()) {
					const uint32_t lastEntityIdx = pOldChunk->GetEntityCount() - 1;
					const bool wasDisabled = m_entities[entity.id()].dis;

					// Transfer data form the old chunk to the new one
					auto lastEntity = pOldChunk->GetEntity(lastEntityIdx);
					pChunk->SetEntity(entityChunkIndex, lastEntity);
					pChunk->MoveEntityData(lastEntity, entityChunkIndex, m_entities);
					pOldChunk->RemoveLastEntity(m_chunksToRemove);
					pOldChunk->UpdateVersions();

					// Transfer the disabled state
					archetype.EnableEntity(pChunk, entityChunkIndex, !wasDisabled);

					auto& lastEntityContainer = m_entities[lastEntity.id()];
					lastEntityContainer.pChunk = pChunk;
					lastEntityContainer.idx = entityChunkIndex;
					lastEntityContainer.gen = lastEntity.gen();
				}

				pChunk->UpdateVersions();
				if constexpr (IsEntityReleaseWanted)
					ReleaseEntity(entity);

				archetype.VerifyChunksFramentation();
#else
				if (pChunk->IsEntityEnabled(entityChunkIndex)) {
					// Entity was previously enabled. Swap with the last entity
					pChunk->RemoveChunkEntity(entityChunkIndex, {m_entities.data(), m_entities.size()});
					// If this was the first enabled entity make sure to update the index
					if (pChunk->m_header.firstEnabledEntityIndex > 0 &&
							entityChunkIndex == pChunk->m_header.firstEnabledEntityIndex)
						--pChunk->m_header.firstEnabledEntityIndex;
				} else {
					// Entity was previously disabled. Swap with the last disabled entity
					const auto pivot = pChunk->GetDisabledEntityCount() - 1;
					pChunk->SwapChunkEntities(entityChunkIndex, pivot, {m_entities.data(), m_entities.size()});
					// Once swapped, try to swap with the last (enabled) entity in the chunk.
					pChunk->RemoveChunkEntity(pivot, {m_entities.data(), m_entities.size()});
					--pChunk->m_header.firstEnabledEntityIndex;
				}

				// At this point the last entity is no longer valid so remove it
				pChunk->RemoveLastEntity(m_chunksToRemove);

				pChunk->UpdateVersions();
				if constexpr (IsEntityReleaseWanted)
					ReleaseEntity(entity);
#endif
			}

#if !GAIA_AVOID_CHUNK_FRAGMENTATION
			//! Defragments chunks.
			//! \param maxEntites Maximum number of entities moved per call
			void DefragmentChunks(uint32_t maxEntities) {
				const auto maxIters = (uint32_t)m_archetypes.size();
				for (uint32_t i = 0; i < maxIters; ++i) {
					m_defragLastArchetypeID = (m_defragLastArchetypeID + i) % maxIters;

					auto* pArchetype = m_archetypes[m_defragLastArchetypeID];
					pArchetype->Defragment(maxEntities, m_chunksToRemove, {m_entities.data(), m_entities.size()});
					if (maxEntities == 0)
						return;
				}
			}
#endif

			//! Searches for archetype with a given set of components
			//! \param lookupHash Archetype lookup hash
			//! \param componentIdsGeneric Span of generic component ids
			//! \param componentIdsChunk Span of chunk component ids
			//! \return Pointer to archetype or nullptr.
			GAIA_NODISCARD archetype::Archetype* FindArchetype(
					archetype::Archetype::LookupHash lookupHash, component::ComponentIdSpan componentIdsGeneric,
					component::ComponentIdSpan componentIdsChunk) {
				auto tmpArchetype = archetype::ArchetypeLookupChecker(componentIdsGeneric, componentIdsChunk);
				archetype::ArchetypeLookupKey key(lookupHash, &tmpArchetype);

				// Search for the archetype in the map
				const auto it = m_archetypeMap.find(key);
				if (it == m_archetypeMap.end())
					return nullptr;

				auto* pArchetype = it->second;
				return pArchetype;
			}

			//! Creates a new archetype from a given set of components
			//! \param componentIdsGeneric Span of generic component infos
			//! \param componentIdsChunk Span of chunk component infos
			//! \return Pointer to the new archetype.
			GAIA_NODISCARD archetype::Archetype*
			CreateArchetype(component::ComponentIdSpan componentIdsGeneric, component::ComponentIdSpan componentIdsChunk) {
				auto* pArchetype = archetype::Archetype::Create(
						(archetype::ArchetypeId)m_archetypes.size(), m_worldVersion, componentIdsGeneric, componentIdsChunk);

				auto registerComponentToArchetypePair = [&](component::ComponentId componentId) {
					const auto it = m_componentToArchetypeMap.find(componentId);
					if (it == m_componentToArchetypeMap.end())
						m_componentToArchetypeMap.try_emplace(componentId, archetype::ArchetypeList{pArchetype});
					else if (!core::has(it->second, pArchetype))
						it->second.push_back(pArchetype);
				};

				for (const auto componentId: componentIdsGeneric)
					registerComponentToArchetypePair(componentId);
				for (const auto componentId: componentIdsChunk)
					registerComponentToArchetypePair(componentId);

				return pArchetype;
			}

			//! Registers the archetype in the world.
			//! \param pArchetype Archetype to register.
			void RegisterArchetype(archetype::Archetype* pArchetype) {
				// Make sure hashes were set already
				GAIA_ASSERT(
						(m_archetypes.empty() || pArchetype == m_archetypes[0]) ||
						(pArchetype->GetGenericHash().hash != 0 || pArchetype->GetChunkHash().hash != 0));
				GAIA_ASSERT((m_archetypes.empty() || pArchetype == m_archetypes[0]) || pArchetype->GetLookupHash().hash != 0);

				// Make sure the archetype is not registered yet
				GAIA_ASSERT(!core::has(m_archetypes, pArchetype));

				// Register the archetype
				m_archetypes.push_back(pArchetype);
				m_archetypeMap.emplace(archetype::ArchetypeLookupKey(pArchetype->GetLookupHash(), pArchetype), pArchetype);
			}

#if GAIA_DEBUG
			static void VerifyAddComponent(
					archetype::Archetype& archetype, Entity entity, component::ComponentType componentType,
					const component::ComponentInfo& infoToAdd) {
				const auto& componentIds = archetype.GetComponentIdArray(componentType);
				const auto& cc = ComponentCache::Get();

				// Make sure not to add too many infos
				if GAIA_UNLIKELY (!archetype::VerifyArchetypeComponentCount(1)) {
					GAIA_ASSERT(false && "Trying to add too many components to entity!");
					GAIA_LOG_W(
							"Trying to add a component to entity [%u.%u] but there's no space left!", entity.id(), entity.gen());
					GAIA_LOG_W("Already present:");
					const uint32_t oldInfosCount = componentIds.size();
					for (uint32_t i = 0; i < oldInfosCount; ++i) {
						const auto& info = cc.GetComponentDesc(componentIds[i]);
						GAIA_LOG_W("> [%u] %.*s", (uint32_t)i, (uint32_t)info.name.size(), info.name.data());
					}
					GAIA_LOG_W("Trying to add:");
					{
						const auto& info = cc.GetComponentDesc(infoToAdd.componentId);
						GAIA_LOG_W("> %.*s", (uint32_t)info.name.size(), info.name.data());
					}
				}

				// Don't add the same component twice
				for (uint32_t i = 0; i < componentIds.size(); ++i) {
					const auto& info = cc.GetComponentDesc(componentIds[i]);
					if (info.componentId == infoToAdd.componentId) {
						GAIA_ASSERT(false && "Trying to add a duplicate component");

						GAIA_LOG_W(
								"Trying to add a duplicate of component %s to entity [%u.%u]",
								component::ComponentTypeString[componentType], entity.id(), entity.gen());
						GAIA_LOG_W("> %.*s", (uint32_t)info.name.size(), info.name.data());
					}
				}
			}

			static void VerifyRemoveComponent(
					archetype::Archetype& archetype, Entity entity, component::ComponentType componentType,
					const component::ComponentInfo& infoToRemove) {
				const auto& componentIds = archetype.GetComponentIdArray(componentType);
				if GAIA_UNLIKELY (!core::has(componentIds, infoToRemove.componentId)) {
					GAIA_ASSERT(false && "Trying to remove a component which wasn't added");
					GAIA_LOG_W(
							"Trying to remove a component from entity [%u.%u] but it was never added", entity.id(), entity.gen());
					GAIA_LOG_W("Currently present:");

					const auto& cc = ComponentCache::Get();

					for (uint32_t k = 0; k < componentIds.size(); k++) {
						const auto& info = cc.GetComponentDesc(componentIds[k]);
						GAIA_LOG_W("> [%u] %.*s", (uint32_t)k, (uint32_t)info.name.size(), info.name.data());
					}

					{
						GAIA_LOG_W("Trying to remove:");
						const auto& info = cc.GetComponentDesc(infoToRemove.componentId);
						GAIA_LOG_W("> %.*s", (uint32_t)info.name.size(), info.name.data());
					}
				}
			}
#endif

			//! Searches for an archetype which is formed by adding \param componentType to \param pArchetypeLeft.
			//! If no such archetype is found a new one is created.
			//! \param pArchetypeLeft Archetype we originate from.
			//! \param componentType Component infos.
			//! \param infoToAdd Component we want to add.
			//! \return Pointer to archetype.
			GAIA_NODISCARD archetype::Archetype* FindOrCreateArchetype_AddComponent(
					archetype::Archetype* pArchetypeLeft, component::ComponentType componentType,
					const component::ComponentInfo& infoToAdd) {
				// We don't want to store edges for the root archetype because the more components there are the longer
				// it would take to find anything. Therefore, for the root archetype we always make a lookup.
				// Compared to an ordinary lookup this path is stripped as much as possible.
				if (pArchetypeLeft == m_archetypes[0]) {
					archetype::Archetype* pArchetypeRight = nullptr;

					if (componentType == component::ComponentType::CT_Generic) {
						const auto genericHash = infoToAdd.lookupHash;
						const auto lookupHash = archetype::Archetype::CalculateLookupHash(genericHash, {0});
						pArchetypeRight = FindArchetype(lookupHash, component::ComponentIdSpan(&infoToAdd.componentId, 1), {});
						if (pArchetypeRight == nullptr) {
							pArchetypeRight = CreateArchetype(component::ComponentIdSpan(&infoToAdd.componentId, 1), {});
							pArchetypeRight->SetHashes({genericHash}, {0}, lookupHash);
							pArchetypeRight->BuildGraphEdgesLeft(pArchetypeLeft, componentType, infoToAdd.componentId);
							RegisterArchetype(pArchetypeRight);
						}
					} else {
						const auto chunkHash = infoToAdd.lookupHash;
						const auto lookupHash = archetype::Archetype::CalculateLookupHash({0}, chunkHash);
						pArchetypeRight = FindArchetype(lookupHash, {}, component::ComponentIdSpan(&infoToAdd.componentId, 1));
						if (pArchetypeRight == nullptr) {
							pArchetypeRight = CreateArchetype({}, component::ComponentIdSpan(&infoToAdd.componentId, 1));
							pArchetypeRight->SetHashes({0}, {chunkHash}, lookupHash);
							pArchetypeRight->BuildGraphEdgesLeft(pArchetypeLeft, componentType, infoToAdd.componentId);
							RegisterArchetype(pArchetypeRight);
						}
					}

					return pArchetypeRight;
				}

				// Check if the component is found when following the "add" edges
				{
					const auto archetypeId = pArchetypeLeft->FindGraphEdgeRight(componentType, infoToAdd.componentId);
					if (archetypeId != archetype::ArchetypeIdBad)
						return m_archetypes[archetypeId];
				}

				const uint32_t a = componentType;
				const uint32_t b = (componentType + 1) & 1;
				const cnt::sarray_ext<uint32_t, archetype::MAX_COMPONENTS_PER_ARCHETYPE>* infos[2];

				cnt::sarray_ext<uint32_t, archetype::MAX_COMPONENTS_PER_ARCHETYPE> infosNew;
				infos[a] = &infosNew;
				infos[b] = &pArchetypeLeft->GetComponentIdArray((component::ComponentType)b);

				// Prepare a joint array of component infos of old + the newly added component
				{
					const auto& componentIds = pArchetypeLeft->GetComponentIdArray((component::ComponentType)a);
					const auto componentInfosSize = componentIds.size();
					infosNew.resize(componentInfosSize + 1);

					for (uint32_t j = 0; j < componentInfosSize; ++j)
						infosNew[j] = componentIds[j];
					infosNew[componentInfosSize] = infoToAdd.componentId;
				}

				// Make sure to sort the component infos so we receive the same hash no matter the order in which components
				// are provided Bubble sort is okay. We're dealing with at most MAX_COMPONENTS_PER_ARCHETYPE items.
				component::SortComponents(infosNew);

				// Once sorted we can calculate the hashes
				const archetype::Archetype::GenericComponentHash genericHash = {
						component::CalculateLookupHash({infos[0]->data(), infos[0]->size()}).hash};
				const archetype::Archetype::ChunkComponentHash chunkHash = {
						component::CalculateLookupHash({infos[1]->data(), infos[1]->size()}).hash};
				const auto lookupHash = archetype::Archetype::CalculateLookupHash(genericHash, chunkHash);

				auto* pArchetypeRight =
						FindArchetype(lookupHash, {infos[0]->data(), infos[0]->size()}, {infos[1]->data(), infos[1]->size()});
				if (pArchetypeRight == nullptr) {
					pArchetypeRight = CreateArchetype({infos[0]->data(), infos[0]->size()}, {infos[1]->data(), infos[1]->size()});
					pArchetypeRight->SetHashes(genericHash, chunkHash, lookupHash);
					pArchetypeLeft->BuildGraphEdges(pArchetypeRight, componentType, infoToAdd.componentId);
					RegisterArchetype(pArchetypeRight);
				}

				return pArchetypeRight;
			}

			//! Searches for an archetype which is formed by removing \param componentType from \param pArchetypeRight.
			//! If no such archetype is found a new one is created.
			//! \param pArchetypeRight Archetype we originate from.
			//! \param componentType Component infos.
			//! \param infoToRemove Component we want to remove.
			//! \return Pointer to archetype.
			GAIA_NODISCARD archetype::Archetype* FindOrCreateArchetype_RemoveComponent(
					archetype::Archetype* pArchetypeRight, component::ComponentType componentType,
					const component::ComponentInfo& infoToRemove) {
				// Check if the component is found when following the "del" edges
				{
					const auto archetypeId = pArchetypeRight->FindGraphEdgeLeft(componentType, infoToRemove.componentId);
					if (archetypeId != archetype::ArchetypeIdBad)
						return m_archetypes[archetypeId];
				}

				const uint32_t a = componentType;
				const uint32_t b = (componentType + 1) & 1;
				const cnt::sarray_ext<uint32_t, archetype::MAX_COMPONENTS_PER_ARCHETYPE>* infos[2];

				cnt::sarray_ext<uint32_t, archetype::MAX_COMPONENTS_PER_ARCHETYPE> infosNew;
				infos[a] = &infosNew;
				infos[b] = &pArchetypeRight->GetComponentIdArray((component::ComponentType)b);

				// Find the intersection
				for (const auto componentId: pArchetypeRight->GetComponentIdArray((component::ComponentType)a)) {
					if (componentId == infoToRemove.componentId)
						continue;

					infosNew.push_back(componentId);
				}

				// Return if there's no change
				if (infosNew.size() == pArchetypeRight->GetComponentIdArray((component::ComponentType)a).size())
					return nullptr;

				// Calculate the hashes
				const archetype::Archetype::GenericComponentHash genericHash = {
						component::CalculateLookupHash({infos[0]->data(), infos[0]->size()}).hash};
				const archetype::Archetype::ChunkComponentHash chunkHash = {
						component::CalculateLookupHash({infos[1]->data(), infos[1]->size()}).hash};
				const auto lookupHash = archetype::Archetype::CalculateLookupHash(genericHash, chunkHash);

				auto* pArchetype =
						FindArchetype(lookupHash, {infos[0]->data(), infos[0]->size()}, {infos[1]->data(), infos[1]->size()});
				if (pArchetype == nullptr) {
					pArchetype = CreateArchetype({infos[0]->data(), infos[0]->size()}, {infos[1]->data(), infos[1]->size()});
					pArchetype->SetHashes(genericHash, lookupHash, lookupHash);
					pArchetype->BuildGraphEdges(pArchetypeRight, componentType, infoToRemove.componentId);
					RegisterArchetype(pArchetype);
				}

				return pArchetype;
			}

			//! Returns an array of archetypes registered in the world
			//! \return Array or archetypes.
			const auto& GetArchetypes() const {
				return m_archetypes;
			}

			//! Returns the archetype the entity belongs to.
			//! \param entity Entity
			//! \return Reference to the archetype.
			GAIA_NODISCARD archetype::Archetype& GetArchetype(Entity entity) const {
				GAIA_ASSERT(IsValid(entity));

				const auto& entityContainer = m_entities[entity.id()];
				auto* pChunk = entityContainer.pChunk;
				return *m_archetypes[pChunk == nullptr ? archetype::ArchetypeId(0) : pChunk->GetArchetypeId()];
			}

			//! Invalidates \param entityToDelete
			void ReleaseEntity(Entity entityToDelete) {
				auto& entityContainer = m_entities.release(entityToDelete);
				entityContainer.pChunk = nullptr;
			}

			//! Associates an entity with a chunk.
			//! \param entity Entity to associate with a chunk
			//! \param pChunk Chunk the entity is to become a part of
			void StoreEntity(Entity entity, archetype::Chunk* pChunk) {
				GAIA_ASSERT(pChunk != nullptr);
				GAIA_ASSERT(
						!pChunk->IsStructuralChangesLocked() && "Entities can't be added while their chunk is being iterated "
																										"(structural changes are forbidden during this time!)");

				auto& entityContainer = m_entities[entity.id()];
				entityContainer.pChunk = pChunk;
				entityContainer.idx = pChunk->AddEntity(entity);
				entityContainer.gen = entity.gen();
				entityContainer.dis = 0;
			}

			/*!
			Moves an entity along with all its generic components from its current chunk to another one.
			\param oldEntity Entity to move
			\param targetChunk Target chunk
			*/
			void MoveEntity(Entity oldEntity, archetype::Chunk& targetChunk) {
				GAIA_PROF_SCOPE(MoveEntity);

				auto* pNewChunk = &targetChunk;

				auto& entityContainer = m_entities[oldEntity.id()];
				auto* pOldChunk = entityContainer.pChunk;

				const auto oldIndex0 = entityContainer.idx;
				const auto newIndex = pNewChunk->AddEntity(oldEntity);

				// Transfer the disabled state
				const bool wasEnabled = !entityContainer.dis;
				auto& oldArchetype = *m_archetypes[pOldChunk->GetArchetypeId()];
				auto& newArchetype = *m_archetypes[pNewChunk->GetArchetypeId()];

				// Make sure the old entity becomes enabled now
				oldArchetype.EnableEntity(pOldChunk, oldIndex0, true, {m_entities.data(), m_entities.size()});
				// Enabling the entity might have changed the index so fetch it again
				const auto oldIndex = m_entities[oldEntity.id()].idx;

				// No data movement necessary when dealing with the root archetype
				if GAIA_LIKELY (pNewChunk->GetArchetypeId() + pOldChunk->GetArchetypeId() != 0) {
					// Move data from the old chunk to the new one
					if (pOldChunk->GetArchetypeId() == pNewChunk->GetArchetypeId())
						pNewChunk->MoveEntityData(oldEntity, newIndex, {m_entities.data(), m_entities.size()});
					else
						pNewChunk->MoveForeignEntityData(oldEntity, newIndex, {m_entities.data(), m_entities.size()});
				}

				// Transfer the enabled state to the new archetype as well
				newArchetype.EnableEntity(pNewChunk, newIndex, wasEnabled, {m_entities.data(), m_entities.size()});

				// Remove the entity record from the old chunk
				RemoveEntity<false>(pOldChunk, oldIndex);

				// Make the entity point to the new chunk
				entityContainer.pChunk = pNewChunk;
				entityContainer.idx = newIndex;
				entityContainer.gen = oldEntity.gen();
				GAIA_ASSERT(entityContainer.dis == !wasEnabled);
				// entityContainer.dis = !wasEnabled;

				// End-state validation
				ValidateChunk(pOldChunk);
				ValidateChunk(pNewChunk);
				ValidateEntityList();
			}

			/*!
			Moves an entity along with all its generic components from its current chunk to another one in a new archetype.
			\param oldEntity Entity to move
			\param newArchetype Target archetype
			*/
			void MoveEntity(Entity oldEntity, archetype::Archetype& newArchetype) {
				auto* pNewChunk = newArchetype.FindOrCreateFreeChunk();
				return MoveEntity(oldEntity, *pNewChunk);
			}

			//! Verifies than the implicit linked list of entities is valid
			void ValidateEntityList() const {
#if GAIA_ECS_VALIDATE_ENTITY_LIST
				m_entities.validate();
#endif
			}

			//! Verifies that the chunk is valid
			void ValidateChunk([[maybe_unused]] archetype::Chunk* pChunk) const {
#if GAIA_ECS_VALIDATE_CHUNKS
				// Note: Normally we'd go [[maybe_unused]] instead of "(void)" but MSVC
				// 2017 suffers an internal compiler error in that case...
				(void)pChunk;
				GAIA_ASSERT(pChunk != nullptr);

				if (pChunk->HasEntities()) {
					// Make sure a proper amount of entities reference the chunk
					uint32_t cnt = 0;
					for (const auto& e: m_entities) {
						if (e.pChunk != pChunk)
							continue;
						++cnt;
					}
					GAIA_ASSERT(cnt == pChunk->GetEntityCount());
				} else {
					// Make sure no entites reference the chunk
					for (const auto& e: m_entities) {
						(void)e;
						GAIA_ASSERT(e.pChunk != pChunk);
					}
				}
#endif
			}

			EntityContainer& AddComponent_Internal(
					component::ComponentType componentType, Entity entity, const component::ComponentInfo& infoToAdd) {
				GAIA_PROF_SCOPE(AddComponent);

				auto& entityContainer = m_entities[entity.id()];

				auto* pChunk = entityContainer.pChunk;

				GAIA_ASSERT(pChunk != nullptr);
				GAIA_ASSERT(
						!pChunk->IsStructuralChangesLocked() && "New components can't be added while their chunk is being iterated "
																										"(structural changes are forbidden during this time!)");

				// Adding a component to an entity which already is a part of some chunk
				{
					auto& archetype = *m_archetypes[pChunk->GetArchetypeId()];

#if GAIA_DEBUG
					VerifyAddComponent(archetype, entity, componentType, infoToAdd);
#endif

					auto* pTargetArchetype = FindOrCreateArchetype_AddComponent(&archetype, componentType, infoToAdd);
					MoveEntity(entity, *pTargetArchetype);
					pChunk = entityContainer.pChunk;
				}

				// Call the constructor for the newly added component if necessary
				if (componentType == component::ComponentType::CT_Generic)
					pChunk->CallConstructor(componentType, infoToAdd.componentId, entityContainer.idx);
				else if (componentType == component::ComponentType::CT_Chunk)
					pChunk->CallConstructor(componentType, infoToAdd.componentId, 0);

				return entityContainer;
			}

			ComponentSetter DelComponent_Internal(
					component::ComponentType componentType, Entity entity, const component::ComponentInfo& infoToRemove) {
				GAIA_PROF_SCOPE(Del);

				auto& entityContainer = m_entities[entity.id()];
				auto* pChunk = entityContainer.pChunk;

				GAIA_ASSERT(pChunk != nullptr);
				GAIA_ASSERT(
						!pChunk->IsStructuralChangesLocked() && "Components can't be removed while their chunk is being iterated "
																										"(structural changes are forbidden during this time!)");

				auto& archetype = *m_archetypes[pChunk->GetArchetypeId()];

#if GAIA_DEBUG
				VerifyRemoveComponent(archetype, entity, componentType, infoToRemove);
#endif

				auto* pNewArchetype = FindOrCreateArchetype_RemoveComponent(&archetype, componentType, infoToRemove);
				GAIA_ASSERT(pNewArchetype != nullptr);
				MoveEntity(entity, *pNewArchetype);

				return ComponentSetter{pChunk, entityContainer.idx};
			}

			void Init() {
				auto* pRootArchetype = CreateArchetype({}, {});
				pRootArchetype->SetHashes({0}, {0}, archetype::Archetype::CalculateLookupHash({0}, {0}));
				RegisterArchetype(pRootArchetype);
			}

			void Done() {
				Cleanup();

				ChunkAllocator::get().Flush();

#if GAIA_DEBUG && GAIA_ECS_CHUNK_ALLOCATOR
				// Make sure there are no leaks
				ChunkAllocatorStats memStats = ChunkAllocator::get().GetStats();
				for (const auto& s: memStats.stats) {
					if (s.AllocatedMemory != 0) {
						GAIA_ASSERT(false && "ECS leaking memory");
						GAIA_LOG_W("ECS leaking memory!");
						ChunkAllocator::get().Diag();
					}
				}
#endif
			}

			//! Creates a new entity from archetype
			//! \return Entity
			GAIA_NODISCARD Entity Add(archetype::Archetype& archetype) {
				const auto entity = m_entities.allocate();

				const auto& entityContainer = m_entities[entity.id()];
				auto* pChunk = archetype.FindOrCreateFreeChunk();

				StoreEntity(entity, pChunk);

				// Call constructors for the generic components on the newly added entity if necessary
				if (pChunk->HasAnyCustomGenericConstructor())
					pChunk->CallConstructors(component::ComponentType::CT_Generic, entityContainer.idx, 1);

				return entity;
			}

			//! Garbage collection. Checks all chunks and archetypes which are empty and have not been
			//! used for a while and tries to delete them and release memory allocated by them.
			void GC() {
				// Handle chunks
				for (uint32_t i = 0; i < m_chunksToRemove.size();) {
					auto* pChunk = m_chunksToRemove[i];

					// Skip reclaimed chunks
					if (pChunk->HasEntities()) {
						pChunk->PrepareToDie();
						core::erase_fast(m_chunksToRemove, i);
						continue;
					}

					if (pChunk->ProgressDeath()) {
						++i;
						continue;
					}
				}

				// Remove all dead chunks
				for (auto* pChunk: m_chunksToRemove) {
					auto& archetype = *m_archetypes[pChunk->GetArchetypeId()];
					archetype.RemoveChunk(pChunk);
				}
				m_chunksToRemove.clear();

#if !GAIA_AVOID_CHUNK_FRAGMENTATION
				// Defragment chunks only now. If we did this at the begging of the function
				// we would needlessly iterate chunks which have no way of being collected because
				// it would be their first frame dying.
				DefragmentChunks(100);
#endif
			}

		public:
			World() {
				Init();
			}

			~World() {
				Done();
			}

			World(World&&) = delete;
			World(const World&) = delete;
			World& operator=(World&&) = delete;
			World& operator=(const World&) = delete;

			//! Checks if \param entity is valid.
			//! \return True is the entity is valid. False otherwise.
			GAIA_NODISCARD bool IsValid(Entity entity) const {
				// Entity ID has to fit inside the entity array
				if (entity.id() >= m_entities.size())
					return false;

				const auto& entityContainer = m_entities[entity.id()];

				// Generation ID has to match the one in the array
				if (entityContainer.gen != entity.gen())
					return false;

				// The entity in the chunk must match the index in the entity container
				auto* pChunk = entityContainer.pChunk;
				return pChunk != nullptr && pChunk->GetEntity(entityContainer.idx) == entity;
			}

			//! Checks if \param entity is currently used by the world.
			//! \return True is the entity is used. False otherwise.
			GAIA_NODISCARD bool Has(Entity entity) const {
				// Entity ID has to fit inside the entity array
				if (entity.id() >= m_entities.size())
					return false;

				// Index of the entity must fit inside the chunk
				const auto& entityContainer = m_entities[entity.id()];
				auto* pChunk = entityContainer.pChunk;
				return pChunk != nullptr && entityContainer.idx < pChunk->GetEntityCount();
			}

			//! Clears the world so that all its entities and components are released
			void Cleanup() {
				// Clear entities
				m_entities.clear();

				// Clear archetypes
				{
					// Delete all allocated chunks and their parent archetypes
					for (auto* pArchetype: m_archetypes)
						delete pArchetype;

					m_archetypes = {};
					m_archetypeMap = {};
					m_chunksToRemove = {};
				}
			}

			//----------------------------------------------------------------------

			//! Returns the current version of the world.
			//! \return World version number.
			GAIA_NODISCARD uint32_t& GetWorldVersion() {
				return m_worldVersion;
			}

			//----------------------------------------------------------------------

			//! Creates a new empty entity
			//! \return New entity
			GAIA_NODISCARD Entity Add() {
				const auto entity = m_entities.allocate();
				auto* pChunk = m_archetypes[0]->FindOrCreateFreeChunk();
				StoreEntity(entity, pChunk);
				return entity;
			}

			//! Creates a new entity by cloning an already existing one.
			//! \param entity Entity to clone
			//! \return New entity
			GAIA_NODISCARD Entity Add(Entity entity) {
				auto& entityContainer = m_entities[entity.id()];

				auto* pChunk = entityContainer.pChunk;
				GAIA_ASSERT(pChunk != nullptr);

				auto& archetype = *m_archetypes[pChunk->GetArchetypeId()];
				const auto newEntity = Add(archetype);

				archetype::Chunk::CopyEntityData(entity, newEntity, {m_entities.data(), m_entities.size()});

				return newEntity;
			}

			//! Removes an entity along with all data associated with it.
			//! \param entity Entity to delete
			void Del(Entity entity) {
				if (m_entities.item_count() == 0 || entity == EntityNull)
					return;

				GAIA_ASSERT(IsValid(entity));

				const auto& entityContainer = m_entities[entity.id()];

				// Remove entity from chunk
				if (auto* pChunk = entityContainer.pChunk) {
					RemoveEntity<true>(pChunk, entityContainer.idx);
					ValidateChunk(pChunk);
					ValidateEntityList();
				} else {
					ReleaseEntity(entity);
				}
			}

			//! Enables or disables an entire entity.
			//! \param entity Entity
			//! \param enable Enable or disable the entity
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			void Enable(Entity entity, bool enable) {
				auto& entityContainer = m_entities[entity.id()];

				GAIA_ASSERT(
						(!entityContainer.pChunk || !entityContainer.pChunk->IsStructuralChangesLocked()) &&
						"Entities can't be enabled/disabled while their chunk is being iterated "
						"(structural changes are forbidden during this time!)");

				if (auto* pChunk = entityContainer.pChunk) {
					auto& archetype = *m_archetypes[pChunk->GetArchetypeId()];
					archetype.EnableEntity(pChunk, entityContainer.idx, enable, {m_entities.data(), m_entities.size()});
				}
			}

			//! Checks if an entity is valid.
			//! \param entity Entity
			//! \return True it the entity is valid. False otherwise.
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			bool IsEnabled(Entity entity) const {
				GAIA_ASSERT(IsValid(entity));

				const auto& entityContainer = m_entities[entity.id()];
				const bool entityStateInContainer = !entityContainer.dis;
#if GAIA_ASSERT_ENABLED
				const bool entityStateInChunk = entityContainer.pChunk->IsEntityEnabled(entityContainer.idx);
				GAIA_ASSERT(entityStateInChunk == entityStateInContainer);
#endif
				return entityStateInContainer;
			}

			//! Returns the number of active entities
			//! \return Entity
			GAIA_NODISCARD GAIA_FORCEINLINE uint32_t GetEntityCount() const {
				return m_entities.item_count();
			}

			//! Returns an entity at the index \param idx
			//! \return Entity
			GAIA_NODISCARD Entity Get(uint32_t idx) const {
				GAIA_ASSERT(idx < m_entities.size());
				const auto& entityContainer = m_entities[idx];
				return {idx, entityContainer.gen};
			}

			//! Returns a chunk containing the \param entity.
			//! \return Chunk or nullptr if not found.
			GAIA_NODISCARD archetype::Chunk* GetChunk(Entity entity) const {
				GAIA_ASSERT(entity.id() < m_entities.size());
				const auto& entityContainer = m_entities[entity.id()];
				return entityContainer.pChunk;
			}

			//! Returns a chunk containing the \param entity.
			//! Index of the entity is stored in \param indexInChunk
			//! \return Chunk or nullptr if not found
			GAIA_NODISCARD archetype::Chunk* GetChunk(Entity entity, uint32_t& indexInChunk) const {
				GAIA_ASSERT(entity.id() < m_entities.size());
				const auto& entityContainer = m_entities[entity.id()];
				indexInChunk = entityContainer.idx;
				return entityContainer.pChunk;
			}

			//----------------------------------------------------------------------

			//! Attaches a new component \tparam T to \param entity.
			//! \tparam T Component
			//! \param entity Entity
			//! \return ComponentSetter
			//! \warning It is expected the component is not present on \param entity yet. Undefined behavior otherwise.
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			template <typename T>
			ComponentSetter Add(Entity entity) {
				component::VerifyComponent<T>();
				GAIA_ASSERT(IsValid(entity));

				using U = typename component::component_type_t<T>::Type;
				const auto& info = ComponentCache::Get().GetOrCreateComponentInfo<U>();

				constexpr auto componentType = component::component_type_v<T>;
				auto& entityContainer = AddComponent_Internal(componentType, entity, info);
				return ComponentSetter{entityContainer.pChunk, entityContainer.idx};
			}

			//! Attaches a new component \tparam T to \param entity. Also sets its value.
			//! \tparam T Component
			//! \param entity Entity
			//! \param value Value to set for the component
			//! \return ComponentSetter object.
			//! \warning It is expected the component is not present on \param entity yet. Undefined behavior otherwise.
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			template <typename T, typename U = typename component::component_type_t<T>::Type>
			ComponentSetter Add(Entity entity, U&& value) {
				component::VerifyComponent<T>();
				GAIA_ASSERT(IsValid(entity));

				const auto& info = ComponentCache::Get().GetOrCreateComponentInfo<U>();

				if constexpr (component::component_type_v<T> == component::ComponentType::CT_Generic) {
					auto& entityContainer = AddComponent_Internal(component::ComponentType::CT_Generic, entity, info);
					auto* pChunk = entityContainer.pChunk;
					pChunk->template Set<T>(entityContainer.idx, std::forward<U>(value));
					return ComponentSetter{entityContainer.pChunk, entityContainer.idx};
				} else {
					auto& entityContainer = AddComponent_Internal(component::ComponentType::CT_Chunk, entity, info);
					auto* pChunk = entityContainer.pChunk;
					pChunk->template Set<T>(std::forward<U>(value));
					return ComponentSetter{entityContainer.pChunk, entityContainer.idx};
				}
			}

			//! Removes a component \tparam T from \param entity.
			//! \tparam T Component
			//! \param entity Entity
			//! \return ComponentSetter
			//! \warning It is expected the component is present on \param entity. Undefined behavior otherwise.
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			template <typename T>
			ComponentSetter Del(Entity entity) {
				component::VerifyComponent<T>();
				GAIA_ASSERT(IsValid(entity));

				using U = typename component::component_type_t<T>::Type;
				const auto& info = ComponentCache::Get().GetOrCreateComponentInfo<U>();

				constexpr auto componentType = component::component_type_v<T>;
				return DelComponent_Internal(componentType, entity, info);
			}

			//----------------------------------------------------------------------

			//! Sets the value of the component \tparam T on \param entity.
			//! \tparam T Component
			//! \param entity Entity
			//! \param value Value to set for the component
			//! \return ComponentSetter
			//! \warning It is expected the component is present on \param entity. Undefined behavior otherwise.
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			template <typename T, typename U = typename component::component_type_t<T>::Type>
			ComponentSetter Set(Entity entity, U&& value) {
				GAIA_ASSERT(IsValid(entity));

				const auto& entityContainer = m_entities[entity.id()];
				return ComponentSetter{entityContainer.pChunk, entityContainer.idx}.Set<T>(std::forward<U>(value));
			}

			//! Sets the value of the component \tparam T on \param entity without trigger a world version update.
			//! \tparam T Component
			//! \param entity Entity
			//! \param value Value to set for the component
			//! \return ComponentSetter
			//! \warning It is expected the component is present on \param entity. Undefined behavior otherwise.
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			template <typename T, typename U = typename component::component_type_t<T>::Type>
			ComponentSetter SetSilent(Entity entity, U&& value) {
				GAIA_ASSERT(IsValid(entity));

				const auto& entityContainer = m_entities[entity.id()];
				return ComponentSetter{entityContainer.pChunk, entityContainer.idx}.SetSilent<T>(std::forward<U>(value));
			}

			//----------------------------------------------------------------------

			//! Returns the value stored in the component \tparam T on \param entity.
			//! \tparam T Component
			//! \param entity Entity
			//! \return Value stored in the component.
			//! \warning It is expected the component is present on \param entity. Undefined behavior otherwise.
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			template <typename T>
			GAIA_NODISCARD auto Get(Entity entity) const {
				GAIA_ASSERT(IsValid(entity));

				const auto& entityContainer = m_entities[entity.id()];
				return ComponentGetter{entityContainer.pChunk, entityContainer.idx}.Get<T>();
			}

			//! Tells if \param entity contains the component \tparam T.
			//! \tparam T Component
			//! \param entity Entity
			//! \return True if the component is present on entity.
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			template <typename T>
			GAIA_NODISCARD bool Has(Entity entity) const {
				GAIA_ASSERT(IsValid(entity));

				const auto& entityContainer = m_entities[entity.id()];
				return ComponentGetter{entityContainer.pChunk, entityContainer.idx}.Has<T>();
			}

		private:
			template <typename... T>
			void UnpackArgsIntoQuery(Query& query, [[maybe_unused]] core::func_type_list<T...> types) const {
				static_assert(sizeof...(T) > 0, "Inputs-less functors can not be unpacked to query");
				query.All<T...>();
			}

			template <typename... T>
			static void RegisterComponents_Internal([[maybe_unused]] core::func_type_list<T...> types) {
				static_assert(sizeof...(T) > 0, "Empty Query is not supported in this context");
				auto& cc = ComponentCache::Get();
				((void)cc.GetOrCreateComponentInfo<T>(), ...);
			}

			template <typename Func>
			static void RegisterComponents() {
				using InputArgs = decltype(core::func_args(&Func::operator()));
				RegisterComponents_Internal(InputArgs{});
			}

			template <typename... T>
			static constexpr component::ComponentLookupHash
			CalculateQueryIdLookupHash([[maybe_unused]] core::func_type_list<T...> types) {
				return component::CalculateLookupHash<T...>();
			}

		public:
			template <bool UseCache = true>
			auto CreateQuery() {
				if constexpr (UseCache)
					return Query(m_queryCache, m_worldVersion, m_archetypes, m_componentToArchetypeMap);
				else
					return QueryUncached(m_worldVersion, m_archetypes, m_componentToArchetypeMap);
			}

			//! Iterates over all chunks satisfying conditions set by \param func and calls \param func for all of them.
			//! Query instance is generated internally from the input arguments of \param func.
			//! \warning Performance-wise it has less potential than iterating using ecs::Chunk or a comparable ForEach
			//!          passing in a query because it needs to do cached query lookups on each invocation. However, it is
			//!          easier to use and for non-critical code paths it is the most elegant way of iterating your data.
			template <typename Func>
			void ForEach(Func func) {
				using InputArgs = decltype(core::func_args(&Func::operator()));

				RegisterComponents<Func>();

				constexpr auto lookupHash = CalculateQueryIdLookupHash(InputArgs{});
				if (m_uniqueFuncQueryPairs.count(lookupHash) == 0) {
					Query query = CreateQuery();
					UnpackArgsIntoQuery(query, InputArgs{});
					(void)query.FetchQueryInfo();
					m_uniqueFuncQueryPairs.try_emplace(lookupHash, query.GetQueryId());
					CreateQuery().ForEach(query.GetQueryId(), func);
				} else {
					const auto queryId = m_uniqueFuncQueryPairs[lookupHash];
					CreateQuery().ForEach(queryId, func);
				}
			}

			//! Performs various internal operations related to the end of the frame such as
			//! memory cleanup and other various managment operations which keep the system healthy.
			void Update() {
				GC();

				// Signal the end of the frame
				GAIA_PROF_FRAME();
			}

			//--------------------------------------------------------------------------------

			//! Performs diagnostics on archetypes. Prints basic info about them and the chunks they contain.
			void DiagArchetypes() const {
				GAIA_LOG_N("Archetypes:%u", (uint32_t)m_archetypes.size());
				for (const auto* archetype: m_archetypes)
					archetype::Archetype::DiagArchetype(*archetype);
			}

			//! Performs diagnostics on registered components.
			//! Prints basic info about them and reports and detected issues.
			static void DiagRegisteredTypes() {
				ComponentCache::Get().Diag();
			}

			//! Performs diagnostics on entites of the world.
			//! Also performs validation of internal structures which hold the entities.
			void DiagEntities() const {
				ValidateEntityList();

				GAIA_LOG_N("Deleted entities: %u", (uint32_t)m_entities.get_free_items());
				if (m_entities.get_free_items() != 0U) {
					GAIA_LOG_N("  --> %u", (uint32_t)m_entities.get_next_free_item());

					uint32_t iters = 0;
					auto fe = m_entities[m_entities.get_next_free_item()].idx;
					while (fe != Entity::IdMask) {
						GAIA_LOG_N("  --> %u", m_entities[fe].idx);
						fe = m_entities[fe].idx;
						++iters;
						if (iters > m_entities.get_free_items())
							break;
					}

					if ((iters == 0U) || iters > m_entities.get_free_items())
						GAIA_LOG_E("  Entities recycle list contains inconsistent data!");
				}
			}

			/*!
			Performs all diagnostics.
			*/
			void Diag() const {
				DiagArchetypes();
				DiagRegisteredTypes();
				DiagEntities();
			}
		};
	} // namespace ecs
} // namespace gaia
