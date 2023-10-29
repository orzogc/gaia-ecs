#pragma once
#include "../config/config.h"

#include <cinttypes>

#include "../cnt/darray.h"
#include "../cnt/dbitset.h"
#include "../cnt/sarray.h"
#include "../cnt/sarray_ext.h"
#include "../core/hashing_policy.h"
#include "archetype_common.h"
#include "archetype_graph.h"
#include "chunk.h"
#include "chunk_allocator.h"
#include "chunk_header.h"
#include "component.h"
#include "component_cache.h"
#include "component_utils.h"
#include "entity.h"

namespace gaia {
	namespace ecs {
		class Archetype;

		class ArchetypeBase {
		protected:
			//! Archetype ID - used to address the archetype directly in the world's list or archetypes
			ArchetypeId m_archetypeId = ArchetypeIdBad;

		public:
			GAIA_NODISCARD ArchetypeId id() const {
				return m_archetypeId;
			}
		};

		GAIA_NODISCARD inline bool cmp_comp_ids(ComponentIdSpan* compIds, ComponentIdSpan* compIdsOther) {
			// Size has to match
			if (compIds[ComponentKind::CK_Generic].size() != compIdsOther[ComponentKind::CK_Generic].size())
				return false;
			if (compIds[ComponentKind::CK_Chunk].size() != compIdsOther[ComponentKind::CK_Chunk].size())
				return false;

			// Elements have to match
			for (uint32_t i = 0; i < compIds[ComponentKind::CK_Generic].size(); ++i) {
				if (compIds[ComponentKind::CK_Generic][i] != compIdsOther[ComponentKind::CK_Generic][i])
					return false;
			}
			for (uint32_t i = 0; i < compIds[ComponentKind::CK_Chunk].size(); ++i) {
				if (compIds[ComponentKind::CK_Chunk][i] != compIdsOther[ComponentKind::CK_Chunk][i])
					return false;
			}

			return true;
		}

		class ArchetypeLookupChecker: public ArchetypeBase {
			friend class Archetype;

			//! List of component indices
			ComponentIdSpan m_compIds[ComponentKind::CK_Count];

		public:
			ArchetypeLookupChecker(ComponentIdSpan compIdsGeneric, ComponentIdSpan compIdsChunk) {
				m_compIds[ComponentKind::CK_Generic] = compIdsGeneric;
				m_compIds[ComponentKind::CK_Chunk] = compIdsChunk;
			}

			GAIA_NODISCARD bool cmp_comp_ids(const ArchetypeLookupChecker& other) const {
				return ecs::cmp_comp_ids((ComponentIdSpan*)&m_compIds[0], (ComponentIdSpan*)&other.m_compIds[0]);
			}
		};

		class Archetype final: public ArchetypeBase {
		public:
			using LookupHash = core::direct_hash_key<uint64_t>;
			using GenericComponentHash = core::direct_hash_key<uint64_t>;
			using ChunkComponentHash = core::direct_hash_key<uint64_t>;

			struct Properties {
				//! The number of entities this archetype can take (e.g 5 = 5 entities with all their components)
				uint16_t capacity;
				//! How many bytes of data is needed for a fully utilized chunk
				uint16_t chunkDataBytes;
			};
			static_assert(sizeof(Properties) <= sizeof(uint32_t));

		private:
			Properties m_properties{};
			//! Stable reference to parent world's world version
			uint32_t& m_worldVersion;

			//! List of chunks allocated by this archetype
			cnt::darray<Chunk*> m_chunks;
			//! Mask of chunks with disabled entities
			// cnt::dbitset m_disabledMask;
			//! Graph of archetypes linked with this one
			ArchetypeGraph m_graph;

			//! Offsets to various parts of data inside chunk
			ChunkHeaderOffsets m_dataOffsets;
			//! List of component indices
			cnt::sarray<Chunk::ComponentIdArray, ComponentKind::CK_Count> m_compIds;
			//! List of components offset indices
			cnt::sarray<Chunk::ComponentOffsetArray, ComponentKind::CK_Count> m_compOffs;

			//! Hash of generic components
			GenericComponentHash m_genericHash = {0};
			//! Hash of chunk components
			ChunkComponentHash m_chunkHash = {0};
			//! Hash of components within this archetype - used for lookups
			LookupHash m_lookupHash = {0};
			//! Hash of components within this archetype - used for matching
			ComponentMatcherHash m_matcherHash[ComponentKind::CK_Count]{};

			// Constructor is hidden. Create archetypes via Create
			Archetype(uint32_t& worldVersion): m_worldVersion(worldVersion) {}

			void UpdateDataOffsets(uintptr_t memoryAddress) {
				uint32_t offset = 0;

				// Versions
				// We expect versions to fit in the first 256 bytes.
				// With 64 components per archetype (32 generic + 32 chunk) this gives us some headroom.
				{
					offset += mem::padding<alignof(uint32_t)>(memoryAddress);

					if (!comp_ids(ComponentKind::CK_Generic).empty()) {
						GAIA_ASSERT(offset < 256);
						m_dataOffsets.firstByte_Versions[ComponentKind::CK_Generic] = (ChunkVersionOffset)offset;
						offset += sizeof(uint32_t) * comp_ids(ComponentKind::CK_Generic).size();
					}
					if (!comp_ids(ComponentKind::CK_Chunk).empty()) {
						GAIA_ASSERT(offset < 256);
						m_dataOffsets.firstByte_Versions[ComponentKind::CK_Chunk] = (ChunkVersionOffset)offset;
						offset += sizeof(uint32_t) * comp_ids(ComponentKind::CK_Chunk).size();
					}
				}

				// Component ids
				{
					offset += mem::padding<alignof(ComponentId)>(offset);

					if (!comp_ids(ComponentKind::CK_Generic).empty()) {
						m_dataOffsets.firstByte_ComponentIds[ComponentKind::CK_Generic] = (ChunkComponentOffset)offset;
						offset += sizeof(ComponentId) * comp_ids(ComponentKind::CK_Generic).size();
					}
					if (!comp_ids(ComponentKind::CK_Chunk).empty()) {
						m_dataOffsets.firstByte_ComponentIds[ComponentKind::CK_Chunk] = (ChunkComponentOffset)offset;
						offset += sizeof(ComponentId) * comp_ids(ComponentKind::CK_Chunk).size();
					}
				}

				// Component offsets
				{
					offset += mem::padding<alignof(ChunkComponentOffset)>(offset);

					if (!comp_ids(ComponentKind::CK_Generic).empty()) {
						m_dataOffsets.firstByte_CompOffs[ComponentKind::CK_Generic] = (ChunkComponentOffset)offset;
						offset += sizeof(ChunkComponentOffset) * comp_ids(ComponentKind::CK_Generic).size();
					}
					if (!comp_ids(ComponentKind::CK_Chunk).empty()) {
						m_dataOffsets.firstByte_CompOffs[ComponentKind::CK_Chunk] = (ChunkComponentOffset)offset;
						offset += sizeof(ChunkComponentOffset) * comp_ids(ComponentKind::CK_Chunk).size();
					}
				}

				// First entity offset
				{
					offset += mem::padding<alignof(Entity)>(offset);
					m_dataOffsets.firstByte_EntityData = (ChunkComponentOffset)offset;
				}
			}

			/*!
			Checks if a component with \param compId and type \param compKind is present in the archetype.
			\param compId Component id
			\param compKind Component type
			\return True if found. False otherwise.
			*/
			GAIA_NODISCARD bool has_inter(ComponentKind compKind, ComponentId compId) const {
				const auto& compIds = comp_ids(compKind);
				return core::has(compIds, compId);
			}

			/*!
			Estimates how many entities can fit into the chunk described by \param compIds components.
			*/
			static bool est_max_entities_per_archetype(
					uint32_t& dataOffset, uint32_t& maxItems, ComponentIdSpan compIds, uint32_t size, uint32_t maxDataOffset) {
				const auto& cc = ComponentCache::get();

				for (const auto compId: compIds) {
					const auto& desc = cc.comp_desc(compId);
					const auto alignment = desc.properties.alig;
					if (alignment == 0)
						continue;

					// If we're beyond what the chunk could take, subtract one entity
					const auto nextOffset = desc.calc_new_mem_offset(dataOffset, size);
					if (nextOffset >= maxDataOffset) {
						const auto subtractItems = (nextOffset - maxDataOffset + desc.properties.size) / desc.properties.size;
						GAIA_ASSERT(subtractItems > 0);
						GAIA_ASSERT(maxItems > subtractItems);
						maxItems -= subtractItems;
						return false;
					}

					dataOffset = nextOffset;
				}

				return true;
			};

		public:
			Archetype(Archetype&& world) = delete;
			Archetype(const Archetype& world) = delete;
			Archetype& operator=(Archetype&&) = delete;
			Archetype& operator=(const Archetype&) = delete;

			~Archetype() {
				// Delete all archetype chunks
				for (auto* pChunk: m_chunks)
					Chunk::free(pChunk);
			}

			GAIA_NODISCARD bool cmp_comp_ids(const ArchetypeLookupChecker& other) const {
				const auto& ids0 = comp_ids(ComponentKind::CK_Generic);
				const auto& ids1 = comp_ids(ComponentKind::CK_Chunk);
				ComponentIdSpan s[ComponentKind::CK_Count] = {{ids0.data(), ids0.size()}, {ids1.data(), ids1.size()}};
				return ecs::cmp_comp_ids(s, (ComponentIdSpan*)&other.m_compIds[0]);
			}

			GAIA_NODISCARD static LookupHash
			calc_lookup_hash(GenericComponentHash genericHash, ChunkComponentHash chunkHash) noexcept {
				return {core::hash_combine(genericHash.hash, chunkHash.hash)};
			}

			GAIA_NODISCARD static Archetype* create(
					ArchetypeId archetypeId, uint32_t& worldVersion, ComponentIdSpan compIdsGeneric,
					ComponentIdSpan compIdsChunk) {
				auto* newArch = new Archetype(worldVersion);
				newArch->m_archetypeId = archetypeId;
				const uint32_t maxEntities = archetypeId == 0 ? ChunkHeader::MAX_CHUNK_ENTITIES : 512;

				newArch->m_compIds[ComponentKind::CK_Generic].resize((uint32_t)compIdsGeneric.size());
				newArch->m_compIds[ComponentKind::CK_Chunk].resize((uint32_t)compIdsChunk.size());
				newArch->m_compOffs[ComponentKind::CK_Generic].resize((uint32_t)compIdsGeneric.size());
				newArch->m_compOffs[ComponentKind::CK_Chunk].resize((uint32_t)compIdsChunk.size());
				newArch->UpdateDataOffsets(sizeof(ChunkHeader) + MemoryBlockUsableOffset);

				const auto& cc = ComponentCache::get();
				const auto& dataOffset = newArch->m_dataOffsets;

				// Calculate the number of entities per chunks precisely so we can
				// fit as many of them into chunk as possible.

				// Total size of generic components
				uint32_t genericComponentListSize = 0;
				for (const auto compId: compIdsGeneric) {
					const auto& desc = cc.comp_desc(compId);
					genericComponentListSize += desc.properties.size;
				}

				// Total size of chunk components
				uint32_t chunkComponentListSize = 0;
				for (const auto compId: compIdsChunk) {
					const auto& desc = cc.comp_desc(compId);
					chunkComponentListSize += desc.properties.size;
				}

				const uint32_t size0 = Chunk::chunk_data_bytes(detail::ChunkAllocatorImpl::mem_block_size(0));
				const uint32_t size1 = Chunk::chunk_data_bytes(detail::ChunkAllocatorImpl::mem_block_size(1));
				const auto sizeM = (size0 + size1) / 2;

				uint32_t maxDataOffsetTarget = size1;
				// Theoretical maximum number of components we can fit into one chunk.
				// This can be further reduced due alignment and padding.
				auto maxGenericItemsInArchetype =
						(maxDataOffsetTarget - dataOffset.firstByte_EntityData - chunkComponentListSize - 1) /
						(genericComponentListSize + (uint32_t)sizeof(Entity));

				bool finalCheck = false;
			recalculate:
				auto currOff = dataOffset.firstByte_EntityData + (uint32_t)sizeof(Entity) * maxGenericItemsInArchetype;

				// Adjust the maximum number of entities. Recalculation happens at most once when the original guess
				// for entity count is not right (most likely because of padding or usage of SoA components).
				if (!est_max_entities_per_archetype(
								currOff, maxGenericItemsInArchetype, compIdsGeneric, maxGenericItemsInArchetype, maxDataOffsetTarget))
					goto recalculate;
				if (!est_max_entities_per_archetype(currOff, maxGenericItemsInArchetype, compIdsChunk, 1, maxDataOffsetTarget))
					goto recalculate;

				// Limit the number of entities to a ceratain number so we can make use of smaller
				// chunks where it makes sense.
				// TODO:
				// Tweak this so the full remaining capacity is used. So if we occupy 7000 B we still
				// have 1000 B left to fill.
				if (maxGenericItemsInArchetype > maxEntities) {
					maxGenericItemsInArchetype = maxEntities;
					goto recalculate;
				}

				// We create chunks of either 8K or 16K but might end up with requested capacity 8.1K. Allocating a 16K chunk
				// in this case would be wasteful. Therefore, let's find the middle ground. Anything 12K or smaller we'll
				// allocate into 8K chunks so we avoid wasting too much memory.
				if (!finalCheck && currOff < sizeM) {
					finalCheck = true;
					maxDataOffsetTarget = size0;

					maxGenericItemsInArchetype =
							(maxDataOffsetTarget - dataOffset.firstByte_EntityData - chunkComponentListSize - 1) /
							(genericComponentListSize + (uint32_t)sizeof(Entity));
					goto recalculate;
				}

				// Update the offsets according to the recalculated maxGenericItemsInArchetype
				currOff = dataOffset.firstByte_EntityData + (uint32_t)sizeof(Entity) * maxGenericItemsInArchetype;

				auto registerComponents = [&](ComponentIdSpan compIds, ComponentKind compKind, const uint32_t count) {
					auto& ids = newArch->m_compIds[compKind];
					auto& ofs = newArch->m_compOffs[compKind];

					for (uint32_t i = 0; i < compIds.size(); ++i) {
						const auto compId = compIds[i];
						const auto& desc = cc.comp_desc(compId);
						const auto alignment = desc.properties.alig;
						if (alignment == 0) {
							GAIA_ASSERT(desc.properties.size == 0);

							// Register the component info
							ids[i] = compId;
							ofs[i] = {};
						} else {
							currOff = mem::align(currOff, alignment);

							// Register the component info
							ids[i] = compId;
							ofs[i] = (ChunkComponentOffset)currOff;

							// Make sure the following component list is properly aligned
							currOff += desc.properties.size * count;
						}
					}
				};
				registerComponents(compIdsGeneric, ComponentKind::CK_Generic, maxGenericItemsInArchetype);
				registerComponents(compIdsChunk, ComponentKind::CK_Chunk, 1);

				newArch->m_properties.capacity = (uint16_t)maxGenericItemsInArchetype;
				newArch->m_properties.chunkDataBytes = (uint16_t)currOff;
				GAIA_ASSERT(Chunk::chunk_total_bytes((uint16_t)currOff) < detail::ChunkAllocatorImpl::mem_block_size(currOff));

				newArch->m_matcherHash[ComponentKind::CK_Generic] = ecs::matcher_hash(compIdsGeneric);
				newArch->m_matcherHash[ComponentKind::CK_Chunk] = ecs::matcher_hash(compIdsChunk);

				return newArch;
			}

			/*!
			Sets hashes for each component type and lookup.
			\param hashGeneric Generic components hash
			\param hashChunk Chunk components hash
			\param hashLookup Hash used for archetype lookup purposes
			*/
			void set_hashes(GenericComponentHash hashGeneric, ChunkComponentHash hashChunk, LookupHash hashLookup) {
				m_genericHash = hashGeneric;
				m_chunkHash = hashChunk;
				m_lookupHash = hashLookup;
			}

			/*!
			Enables or disables the entity on a given index in the chunk.
			\param pChunk Chunk the entity belongs to
			\param index Index of the entity
			\param enableEntity Enables the entity
			*/
			void enable_entity(Chunk* pChunk, uint32_t entityIdx, bool enableEntity, std::span<EntityContainer> entities) {
				pChunk->enable_entity(entityIdx, enableEntity, entities);
				// m_disabledMask.set(pChunk->idx(), enableEntity ? true : pChunk->has_disabled_entities());
			}

			/*!
			Removes a chunk from the list of chunks managed by their archetype.
			\param pChunk Chunk to remove from the list of managed archetypes
			*/
			void remove_chunk(Chunk* pChunk) {
				const auto chunkIndex = pChunk->idx();

				Chunk::free(pChunk);

				auto remove = [&](auto& chunkArray) {
					if (chunkArray.size() > 1)
						chunkArray.back()->set_idx(chunkIndex);
					GAIA_ASSERT(chunkIndex == core::get_index(chunkArray, pChunk));
					core::erase_fast(chunkArray, chunkIndex);
				};

				remove(m_chunks);
			}

			//! defragments the chunk.
			//! \param maxEntites Maximum number of entities moved per call
			//! \param chunksToRemove Container of chunks ready for removal
			//! \param entities Container with entities
			void defrag(uint32_t& maxEntities, cnt::darray<Chunk*>& chunksToRemove, std::span<EntityContainer> entities) {
				// Assuming the following chunk layout:
				//   Chunk_1: 10/10
				//   Chunk_2:  1/10
				//   Chunk_3:  7/10
				//   Chunk_4: 10/10
				//   Chunk_5:  9/10
				// After full defragmentation we end up with:
				//   Chunk_1: 10/10
				//   Chunk_2: 10/10 (7 entities from Chunk_3 + 2 entities from Chunk_5)
				//   Chunk_3:  0/10 (empty, ready for removal)
				//   Chunk_4: 10/10
				//   Chunk_5:  7/10
				// TODO:
				// Implement mask of semi-full chunks so we can pick one easily when searching
				// for a chunk to fill with a new entity and when defragmenting.
				// NOTE:
				// Even though entity movement might be present during defragmentation, we do
				// not update the world version here because no real structural changes happen.
				// All entites and components remain intact, they just move to a different place.

				if (m_chunks.empty())
					return;

				uint32_t front = 0;
				uint32_t back = m_chunks.size() - 1;

				// Find the first semi-empty chunk in the front
				while (front < back && !m_chunks[front++]->is_semi())
					;

				auto* pDstChunk = m_chunks[front];

				// Find the first semi-empty chunk in the back
				while (front < back && m_chunks[--back]->is_semi()) {
					auto* pSrcChunk = m_chunks[back];

					const uint32_t entitiesInChunk = pSrcChunk->size();
					const uint32_t entitiesToMove = entitiesInChunk > maxEntities ? maxEntities : entitiesInChunk;
					for (uint32_t i = 0; i < entitiesToMove; ++i) {
						const auto lastEntityIdx = entitiesInChunk - i - 1;
						auto entity = pSrcChunk->entity_view()[lastEntityIdx];

						const auto& entityContainer = entities[entity.id()];

						const auto oldIndex = entityContainer.idx;
						const auto newIndex = pDstChunk->add_entity(entity);
						const bool wasEnabled = !entityContainer.dis;

						// Make sure the old entity becomes enabled now
						enable_entity(pSrcChunk, oldIndex, true, entities);
						// We go back-to-front in the chunk so enabling the entity is not expected to change its index
						GAIA_ASSERT(oldIndex == entityContainer.idx);

						// Transfer the original enabled state to the new chunk
						enable_entity(pDstChunk, newIndex, wasEnabled, entities);

						// Remove the entity record from the old chunk
						pSrcChunk->remove_entity(oldIndex, entities, chunksToRemove);

						// The destination chunk is full, we need to move to the next one
						if (pDstChunk->size() == m_properties.capacity) {
							++front;

							// We reached the source chunk which means this archetype has been defragmented
							if (front >= back) {
								maxEntities -= i + 1;
								return;
							}
						}
					}

					maxEntities -= entitiesToMove;
				}
			}

			//! Tries to locate a chunk that has some space left for a new entity.
			//! If not found a new chunk is created.
			GAIA_NODISCARD Chunk* foc_free_chunk() {
				const auto chunkCnt = m_chunks.size();

				if (chunkCnt > 0) {
					// Find first semi-empty chunk.
					// Picking the first non-full would only support fragmentation.
					Chunk* pEmptyChunk = nullptr;
					for (auto* pChunk: m_chunks) {
						GAIA_ASSERT(pChunk != nullptr);
						const auto entityCnt = pChunk->size();
						if GAIA_UNLIKELY (entityCnt == 0)
							pEmptyChunk = pChunk;
						else if (entityCnt < pChunk->capacity())
							return pChunk;
					}
					if (pEmptyChunk != nullptr)
						return pEmptyChunk;
				}

				// Make sure not too many chunks are allocated
				GAIA_ASSERT(chunkCnt < UINT32_MAX);

				// No free space found anywhere. Let's create a new chunk.
				auto* pChunk = Chunk::create(
						m_archetypeId, chunkCnt, m_properties.capacity, m_properties.chunkDataBytes, m_worldVersion, m_dataOffsets,
						m_compIds, m_compOffs);

				m_chunks.push_back(pChunk);
				return pChunk;
			}

			GAIA_NODISCARD const Properties& props() const {
				return m_properties;
			}

			GAIA_NODISCARD const cnt::darray<Chunk*>& chunks() const {
				return m_chunks;
			}

			GAIA_NODISCARD GenericComponentHash generic_hash() const {
				return m_genericHash;
			}

			GAIA_NODISCARD ChunkComponentHash chunk_hash() const {
				return m_chunkHash;
			}

			GAIA_NODISCARD LookupHash lookup_hash() const {
				return m_lookupHash;
			}

			GAIA_NODISCARD ComponentMatcherHash matcher_hash(ComponentKind compKind) const {
				return m_matcherHash[compKind];
			}

			GAIA_NODISCARD const Chunk::ComponentIdArray& comp_ids(ComponentKind compKind) const {
				return m_compIds[compKind];
			}

			GAIA_NODISCARD const Chunk::ComponentOffsetArray& comp_offs(ComponentKind compKind) const {
				return m_compOffs[compKind];
			}

			/*!
			Checks if a component of type \tparam T is present in the archetype.
			\return True if found. False otherwise.
			*/
			template <typename T>
			GAIA_NODISCARD bool has() const {
				const auto compId = comp_id<T>();

				constexpr auto compKind = component_kind_v<T>;
				return has_inter(compKind, compId);
			}

			/*!
			Returns the internal index of a component based on the provided \param compId.
			\param compKind Component type
			\return Component index if the component was found. -1 otherwise.
			*/
			GAIA_NODISCARD uint32_t comp_idx(ComponentKind compKind, ComponentId compId) const {
				const auto idx = core::get_index_unsafe(comp_ids(compKind), compId);
				GAIA_ASSERT(idx != BadIndex);
				return (uint32_t)idx;
			}

			void build_graph_edges(Archetype* pArchetypeRight, ComponentKind compKind, ComponentId compId) {
				GAIA_ASSERT(pArchetypeRight != this);
				m_graph.add_edge_right(compKind, compId, pArchetypeRight->id());
				pArchetypeRight->build_graph_edges_left(this, compKind, compId);
			}

			void build_graph_edges_left(Archetype* pArchetypeLeft, ComponentKind compKind, ComponentId compId) {
				GAIA_ASSERT(pArchetypeLeft != this);
				m_graph.add_edge_left(compKind, compId, pArchetypeLeft->id());
			}

			//! Checks if the graph edge for component type \param compKind contains the component \param compId.
			//! \return Archetype id of the target archetype if the edge is found. ArchetypeIdBad otherwise.
			GAIA_NODISCARD ArchetypeId find_edge_right(ComponentKind compKind, const ComponentId compId) const {
				return m_graph.find_edge_right(compKind, compId);
			}

			//! Checks if the graph edge for component type \param compKind contains the component \param compId.
			//! \return Archetype id of the target archetype if the edge is found. ArchetypeIdBad otherwise.
			GAIA_NODISCARD ArchetypeId find_edge_left(ComponentKind compKind, const ComponentId compId) const {
				return m_graph.find_edge_left(compKind, compId);
			}

			static void diag_basic_info(const Archetype& archetype) {
				const auto& cc = ComponentCache::get();
				const auto& genericComponents = archetype.comp_ids(ComponentKind::CK_Generic);
				const auto& chunkComponents = archetype.comp_ids(ComponentKind::CK_Chunk);

				// Caclulate the number of entites in archetype
				uint32_t entCnt = 0;
				uint32_t entCntDisabled = 0;
				for (const auto* chunk: archetype.m_chunks) {
					entCnt += chunk->size();
					entCntDisabled += chunk->size_disabled();
				}

				// Calculate the number of components
				uint32_t genericComponentsSize = 0;
				uint32_t chunkComponentsSize = 0;
				for (const auto compId: genericComponents) {
					const auto& desc = cc.comp_desc(compId);
					genericComponentsSize += desc.properties.size;
				}
				for (const auto compId: chunkComponents) {
					const auto& desc = cc.comp_desc(compId);
					chunkComponentsSize += desc.properties.size;
				}

				GAIA_LOG_N(
						"Archetype ID:%u, "
						"lookupHash:%016" PRIx64 ", "
						"mask:%016" PRIx64 "/%016" PRIx64 ", "
						"chunks:%u (%uK), data:%u/%u/%u B, "
						"entities:%u/%u/%u",
						archetype.id(), archetype.lookup_hash().hash, archetype.matcher_hash(ComponentKind::CK_Generic).hash,
						archetype.matcher_hash(ComponentKind::CK_Chunk).hash, (uint32_t)archetype.chunks().size(),
						Chunk::chunk_total_bytes(archetype.props().chunkDataBytes) <= 8192 ? 8 : 16, genericComponentsSize,
						chunkComponentsSize, archetype.props().chunkDataBytes, entCnt, entCntDisabled, archetype.props().capacity);

				auto logComponentInfo = [](const ComponentInfo& info, const ComponentDesc& desc) {
					GAIA_LOG_N(
							"    lookupHash:%016" PRIx64 ", mask:%016" PRIx64 ", size:%3u B, align:%3u B, %.*s", info.lookupHash.hash,
							info.matcherHash.hash, desc.properties.size, desc.properties.alig, (uint32_t)desc.name.size(),
							desc.name.data());
				};

				if (!genericComponents.empty()) {
					GAIA_LOG_N("  Generic components - count:%u", (uint32_t)genericComponents.size());
					for (const auto compId: genericComponents) {
						const auto& info = cc.comp_info(compId);
						logComponentInfo(info, cc.comp_desc(compId));
					}
					if (!chunkComponents.empty()) {
						GAIA_LOG_N("  Chunk components - count:%u", (uint32_t)chunkComponents.size());
						for (const auto compId: chunkComponents) {
							const auto& info = cc.comp_info(compId);
							logComponentInfo(info, cc.comp_desc(compId));
						}
					}
				}
			}

			static void diag_graph_info(const Archetype& archetype) {
				archetype.m_graph.diag();
			}

			static void diag_chunk_info(const Archetype& archetype) {
				auto logChunks = [](const auto& chunks) {
					for (uint32_t i = 0; i < chunks.size(); ++i) {
						const auto* pChunk = chunks[i];
						pChunk->diag((uint32_t)i);
					}
				};

				const auto& chunks = archetype.m_chunks;
				if (!chunks.empty())
					GAIA_LOG_N("  Chunks");

				logChunks(chunks);
			}

			/*!
			Performs diagnostics on a specific archetype. Prints basic info about it and the chunks it contains.
			\param archetype Archetype to run diagnostics on
			*/
			static void diag(const Archetype& archetype) {
				diag_basic_info(archetype);
				diag_graph_info(archetype);
				diag_chunk_info(archetype);
			}
		};

		class ArchetypeLookupKey final {
			Archetype::LookupHash m_hash;
			const ArchetypeBase* m_pArchetypeBase;

		public:
			static constexpr bool IsDirectHashKey = true;

			ArchetypeLookupKey(): m_hash({0}), m_pArchetypeBase(nullptr) {}
			ArchetypeLookupKey(Archetype::LookupHash hash, const ArchetypeBase* pArchetypeBase):
					m_hash(hash), m_pArchetypeBase(pArchetypeBase) {}

			size_t hash() const {
				return (size_t)m_hash.hash;
			}

			bool operator==(const ArchetypeLookupKey& other) const {
				// Hash doesn't match we don't have a match.
				// Hash collisions are expected to be very unlikely so optimize for this case.
				if GAIA_LIKELY (m_hash != other.m_hash)
					return false;

				const auto id = m_pArchetypeBase->id();
				if (id == ArchetypeIdBad) {
					const auto* pArchetype = (const Archetype*)other.m_pArchetypeBase;
					const auto* pArchetypeLookupChecker = (const ArchetypeLookupChecker*)m_pArchetypeBase;
					return pArchetype->cmp_comp_ids(*pArchetypeLookupChecker);
				}

				// Real ArchetypeID is given. Compare the pointers.
				// Normally we'd compare archetype IDs but because we do not allow archetype copies and all archetypes are
				// unique it's guaranteed that if pointers are the same we have a match.
				// This also saves a pointer indirection because we do not access the memory the pointer points to.
				return m_pArchetypeBase == other.m_pArchetypeBase;
			}
		};
	} // namespace ecs
} // namespace gaia
