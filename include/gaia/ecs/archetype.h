#pragma once
#include "../containers/darray.h"
#include "../containers/sarray.h"
#include "../containers/sarray_ext.h"
#include "../utils/hashing_policy.h"
#include "../utils/mem.h"
#include "chunk.h"
#include "chunk_allocator.h"
#include "chunk_header.h"
#include "component.h"
#include "component_cache.h"
#include "component_utils.h"

namespace gaia {
	namespace ecs {
		class World;

		const ComponentCache& GetComponentCache();
		ComponentCache& GetComponentCacheRW();

		uint32_t GetWorldVersionFromWorld(const World& world);
		void* AllocateChunkMemory(World& world);
		void ReleaseChunkMemory(World& world, void* mem);

		class Archetype final {
		public:
			using LookupHash = utils::direct_hash_key<uint64_t>;
			using GenericComponentHash = utils::direct_hash_key<uint64_t>;
			using ChunkComponentHash = utils::direct_hash_key<uint64_t>;

		private:
			friend class World;
			friend class CommandBuffer;
			friend class Chunk;
			friend struct ChunkHeader;

			static constexpr uint32_t BadIndex = (uint32_t)-1;

#if GAIA_ARCHETYPE_GRAPH
			struct ArchetypeGraphEdge {
				uint32_t archetypeId;
			};
#endif

			//! World to which this chunk belongs to
			const World* parentWorld = nullptr;

			//! List of active chunks allocated by this archetype
			containers::darray<Chunk*> chunks;
			//! List of disabled chunks allocated by this archetype
			containers::darray<Chunk*> chunksDisabled;

#if GAIA_ARCHETYPE_GRAPH
			//! Map of edges in the archetype graph when adding components
			containers::map<ComponentLookupHash, ArchetypeGraphEdge> edgesAdd[ComponentType::CT_Count];
			//! Map of edges in the archetype graph when removing components
			containers::map<ComponentLookupHash, ArchetypeGraphEdge> edgesDel[ComponentType::CT_Count];
#endif

			//! Description of components within this archetype
			containers::sarray<ComponentInfoList, ComponentType::CT_Count> componentInfos;
			//! Lookup hashes of components within this archetype
			containers::sarray<ComponentLookupList, ComponentType::CT_Count> componentLookupData;

			GenericComponentHash genericHash = {0};
			ChunkComponentHash chunkHash = {0};

			//! Hash of components within this archetype - used for lookups
			ComponentLookupHash lookupHash = {0};
			//! Hash of components within this archetype - used for matching
			ComponentMatcherHash matcherHash[ComponentType::CT_Count] = {0};
			//! Archetype ID - used to address the archetype directly in the world's list or archetypes
			uint32_t id = 0;
			struct {
				//! The number of entities this archetype can take (e.g 5 = 5 entities with all their components)
				uint32_t capacity : 16;
				//! True if there's a component that requires custom destruction
				uint32_t hasGenericComponentWithCustomDestruction : 1;
				//! True if there's a component that requires custom destruction
				uint32_t hasChunkComponentWithCustomDestruction : 1;
				//! Updated when chunks are being iterated. Used to inform of structural changes when they shouldn't happen.
				uint32_t structuralChangesLocked : 4;
			} info{};

			// Constructor is hidden. Create archetypes via Create
			Archetype() = default;

			Archetype(Archetype&& world) = delete;
			Archetype(const Archetype& world) = delete;
			Archetype& operator=(Archetype&&) = delete;
			Archetype& operator=(const Archetype&) = delete;

			GAIA_NODISCARD static LookupHash
			CalculateLookupHash(GenericComponentHash genericHash, ChunkComponentHash chunkHash) noexcept {
				return {utils::hash_combine(genericHash.hash, chunkHash.hash)};
			}

			/*!
			Allocates memory for a new chunk.
			\param archetype Archetype of the chunk we want to allocate
			\return Newly allocated chunk
			*/
			GAIA_NODISCARD static Chunk* AllocateChunk(const Archetype& archetype) {
#if GAIA_ECS_CHUNK_ALLOCATOR
				auto& world = const_cast<World&>(*archetype.parentWorld);

				auto* pChunk = (Chunk*)AllocateChunkMemory(world);
				new (pChunk) Chunk(archetype);
#else
				auto pChunk = new Chunk(archetype);
#endif

				pChunk->header.capacity = archetype.info.capacity;
				return pChunk;
			}

			/*!
			Releases all memory allocated by \param pChunk.
			\param pChunk Chunk which we want to destroy
			*/
			static void ReleaseChunk(Chunk* pChunk) {
				const auto& archetype = pChunk->header.owner;
				const auto& cc = GetComponentCache();

				auto callDestructors = [&](ComponentType type) {
					const auto& looks = archetype.componentLookupData[type];
					const auto itemCount = type == ComponentType::CT_Generic ? pChunk->GetItemCount() : 1U;
					for (auto look: looks) {
						const auto& infoCreate = cc.GetComponentCreateInfoFromIdx(look.infoIndex);
						if (infoCreate.destructor == nullptr)
							continue;
						auto* pSrc = (void*)((uint8_t*)pChunk + look.offset);
						infoCreate.destructor(pSrc, itemCount);
					}
				};

				// Call destructors for components that need it
				if (archetype.info.hasGenericComponentWithCustomDestruction == 1)
					callDestructors(ComponentType::CT_Generic);
				if (archetype.info.hasChunkComponentWithCustomDestruction == 1)
					callDestructors(ComponentType::CT_Chunk);

#if GAIA_ECS_CHUNK_ALLOCATOR
				auto& world = const_cast<World&>(*archetype.parentWorld);
				pChunk->~Chunk();
				ReleaseChunkMemory(world, pChunk);
#else
				delete pChunk;
#endif
			}

			GAIA_NODISCARD static Archetype*
			Create(World& pWorld, std::span<const ComponentInfo*> infosGeneric, std::span<const ComponentInfo*> infosChunk) {
				auto* newArch = new Archetype();
				newArch->parentWorld = &pWorld;

#if GAIA_ARCHETYPE_GRAPH
				// Preallocate arrays for graph edges
				// Generic components are going to be more common so we prepare bigger arrays for them.
				// Chunk components are expected to be very rare so only a small buffer is preallocated.
				newArch->edgesAdd[ComponentType::CT_Generic].reserve(8);
				newArch->edgesAdd[ComponentType::CT_Chunk].reserve(1);
				newArch->edgesDel[ComponentType::CT_Generic].reserve(8);
				newArch->edgesDel[ComponentType::CT_Chunk].reserve(1);
#endif

				// Size of the entity + all of its generic components
				size_t genericComponentListSize = sizeof(Entity);
				for (const auto* pInfo: infosGeneric) {
					genericComponentListSize += pInfo->properties.size;
					newArch->info.hasGenericComponentWithCustomDestruction |= (pInfo->properties.destructible != 0);
				}

				// Size of chunk components
				size_t chunkComponentListSize = 0;
				for (const auto* pInfo: infosChunk) {
					chunkComponentListSize += pInfo->properties.size;
					newArch->info.hasChunkComponentWithCustomDestruction |= (pInfo->properties.destructible != 0);
				}

				// TODO: Calculate the number of entities per chunks precisely so we can
				// fit more of them into chunk on average. Currently, DATA_SIZE_RESERVED
				// is substracted but that's not optimal...

				// Number of components we can fit into one chunk
				auto maxGenericItemsInArchetype = (Chunk::DATA_SIZE - chunkComponentListSize) / genericComponentListSize;

				// Calculate component offsets now. Skip the header and entity IDs
				auto componentOffset = sizeof(Entity) * maxGenericItemsInArchetype;
				auto alignedOffset = sizeof(ChunkHeader) + componentOffset;

				// Add generic infos
				for (size_t i = 0; i < infosGeneric.size(); i++) {
					const auto* pInfo = infosGeneric[i];
					const auto alignment = pInfo->properties.alig;
					if (alignment != 0) {
						const size_t padding = utils::align(alignedOffset, alignment) - alignedOffset;
						componentOffset += padding;
						alignedOffset += padding;

						// Make sure we didn't exceed the chunk size
						GAIA_ASSERT(componentOffset <= Chunk::DATA_SIZE_NORESERVE);

						// Register the component info
						newArch->componentInfos[ComponentType::CT_Generic].push_back(pInfo);
						newArch->componentLookupData[ComponentType::CT_Generic].push_back(
								{pInfo->infoIndex, (uint32_t)componentOffset});

						// Make sure the following component list is properly aligned
						componentOffset += pInfo->properties.size * maxGenericItemsInArchetype;
						alignedOffset += pInfo->properties.size * maxGenericItemsInArchetype;

						// Make sure we didn't exceed the chunk size
						GAIA_ASSERT(componentOffset <= Chunk::DATA_SIZE_NORESERVE);
					} else {
						// Register the component info
						newArch->componentInfos[ComponentType::CT_Generic].push_back(pInfo);
						newArch->componentLookupData[ComponentType::CT_Generic].push_back(
								{pInfo->infoIndex, (uint32_t)componentOffset});
					}
				}

				// Add chunk infos
				for (size_t i = 0; i < infosChunk.size(); i++) {
					const auto* pInfo = infosChunk[i];
					const auto alignment = pInfo->properties.alig;
					if (alignment != 0) {
						const size_t padding = utils::align(alignedOffset, alignment) - alignedOffset;
						componentOffset += padding;
						alignedOffset += padding;

						// Make sure we didn't exceed the chunk size
						GAIA_ASSERT(componentOffset <= Chunk::DATA_SIZE_NORESERVE);

						// Register the component info
						newArch->componentInfos[ComponentType::CT_Chunk].push_back(pInfo);
						newArch->componentLookupData[ComponentType::CT_Chunk].push_back(
								{pInfo->infoIndex, (uint32_t)componentOffset});

						// Make sure the following component list is properly aligned
						componentOffset += pInfo->properties.size;
						alignedOffset += pInfo->properties.size;

						// Make sure we didn't exceed the chunk size
						GAIA_ASSERT(componentOffset <= Chunk::DATA_SIZE_NORESERVE);
					} else {
						// Register the component info
						newArch->componentInfos[ComponentType::CT_Chunk].push_back(pInfo);
						newArch->componentLookupData[ComponentType::CT_Chunk].push_back(
								{pInfo->infoIndex, (uint32_t)componentOffset});
					}
				}

				newArch->info.capacity = (uint32_t)maxGenericItemsInArchetype;
				newArch->matcherHash[ComponentType::CT_Generic] = CalculateMatcherHash(infosGeneric);
				newArch->matcherHash[ComponentType::CT_Chunk] = CalculateMatcherHash(infosChunk);

				return newArch;
			}

			GAIA_NODISCARD Chunk* FindOrCreateFreeChunk_Internal(containers::darray<Chunk*>& chunkArray) const {
				const auto chunkCnt = chunkArray.size();

				if (chunkCnt > 0) {
					// Look for chunks with free space back-to-front.
					// We do it this way because we always try to keep fully utilized and
					// thus only the one in the back should be free.
					auto i = chunkCnt - 1;
					do {
						auto* pChunk = chunkArray[i];
						GAIA_ASSERT(pChunk != nullptr);
						if (!pChunk->IsFull())
							return pChunk;
					} while (i-- > 0);
				}

				GAIA_ASSERT(chunkCnt < (uint32_t)UINT16_MAX);

				// No free space found anywhere. Let's create a new one.
				auto* pChunk = AllocateChunk(*this);
				pChunk->header.index = (uint16_t)chunkCnt;
				chunkArray.push_back(pChunk);
				return pChunk;
			}

			//! Tries to locate a chunk that has some space left for a new entity.
			//! If not found a new chunk is created
			GAIA_NODISCARD Chunk* FindOrCreateFreeChunk() {
				return FindOrCreateFreeChunk_Internal(chunks);
			}

			//! Tries to locate a chunk for disabled entities that has some space left for a new one.
			//! If not found a new chunk is created
			GAIA_NODISCARD Chunk* FindOrCreateFreeChunkDisabled() {
				auto* pChunk = FindOrCreateFreeChunk_Internal(chunksDisabled);
				pChunk->header.disabled = true;
				return pChunk;
			}

			/*!
			Removes a chunk from the list of chunks managed by their achetype.
			\param pChunk Chunk to remove from the list of managed archetypes
			*/
			void RemoveChunk(Chunk* pChunk) {
				const bool isDisabled = pChunk->IsDisabled();
				const auto chunkIndex = pChunk->header.index;

				ReleaseChunk(pChunk);

				auto remove = [&](auto& chunkArray) {
					if (chunkArray.size() > 1)
						chunkArray.back()->header.index = chunkIndex;
					GAIA_ASSERT(chunkIndex == utils::get_index(chunkArray, pChunk));
					utils::erase_fast(chunkArray, chunkIndex);
				};

				if (isDisabled)
					remove(chunksDisabled);
				else
					remove(chunks);
			}

#if GAIA_ARCHETYPE_GRAPH
			//! Create an edge in the graph leading from this archetype to \param archetypeId via component \param info.
			void AddEdgeArchetypeRight(ComponentType type, const ComponentInfo* pInfo, uint32_t archetypeId) {
				[[maybe_unused]] const auto ret =
						edgesAdd[type].try_emplace({pInfo->lookupHash}, ArchetypeGraphEdge{archetypeId});
				GAIA_ASSERT(ret.second);
			}

			//! Create an edge in the graph leading from this archetype to \param archetypeId via component \param info.
			void AddEdgeArchetypeLeft(ComponentType type, const ComponentInfo* pInfo, uint32_t archetypeId) {
				[[maybe_unused]] const auto ret =
						edgesDel[type].try_emplace({pInfo->lookupHash}, ArchetypeGraphEdge{archetypeId});
				GAIA_ASSERT(ret.second);
			}

			GAIA_NODISCARD uint32_t FindAddEdgeArchetypeId(ComponentType type, const ComponentInfo* pInfo) const {
				const auto& edges = edgesAdd[type];
				const auto it = edges.find({pInfo->lookupHash});
				return it != edges.end() ? it->second.archetypeId : BadIndex;
			}

			GAIA_NODISCARD uint32_t FindDelEdgeArchetypeId(ComponentType type, const ComponentInfo* pInfo) const {
				const auto& edges = edgesDel[type];
				const auto it = edges.find({pInfo->lookupHash});
				return it != edges.end() ? it->second.archetypeId : BadIndex;
			}
#endif

		public:
			/*!
			Checks if the archetype id is valid.
			\return True if the id is valid, false otherwise.
			*/
			static bool IsIdValid(uint32_t id) {
				return id != BadIndex;
			}

			~Archetype() {
				// Delete all archetype chunks
				for (auto* pChunk: chunks)
					ReleaseChunk(pChunk);
				for (auto* pChunk: chunksDisabled)
					ReleaseChunk(pChunk);
			}

			/*!
			Initializes the archetype with hash values for each kind of component types.
			\param hashGeneric Generic components hash
			\param hashChunk Chunk components hash
			\param hashLookup Hash used for archetype lookup purposes
			*/
			void Init(GenericComponentHash hashGeneric, ChunkComponentHash hashChunk, ComponentLookupHash hashLookup) {
				this->genericHash = hashGeneric;
				this->chunkHash = hashChunk;
				this->lookupHash = hashLookup;
			}

			GAIA_NODISCARD const World& GetWorld() const {
				return *parentWorld;
			}

			GAIA_NODISCARD uint32_t GetWorldVersion() const {
				return GetWorldVersionFromWorld(*parentWorld);
			}

			GAIA_NODISCARD uint32_t GetCapacity() const {
				return info.capacity;
			}

			GAIA_NODISCARD ComponentMatcherHash GetMatcherHash(ComponentType type) const {
				return matcherHash[type];
			}

			GAIA_NODISCARD const ComponentInfoList& GetComponentInfoList(ComponentType type) const {
				return componentInfos[type];
			}

			GAIA_NODISCARD const ComponentLookupList& GetComponentLookupList(ComponentType type) const {
				return componentLookupData[type];
			}

			/*!
			Checks if a given component is present on the archetype.
			\return True if the component is present. False otherwise.
			*/
			template <typename T>
			GAIA_NODISCARD bool HasComponent() const {
				return HasComponent_Internal<T>();
			}

		private:
			template <typename T>
			GAIA_NODISCARD bool HasComponent_Internal() const {
				const auto infoIndex = GetComponentIndex<T>();

				if constexpr (IsGenericComponent<T>) {
					return utils::has_if(GetComponentLookupList(ComponentType::CT_Generic), [&](const auto& info) {
						return info.infoIndex == infoIndex;
					});
				} else {
					return utils::has_if(GetComponentLookupList(ComponentType::CT_Chunk), [&](const auto& info) {
						return info.infoIndex == infoIndex;
					});
				}
			}
		};

		GAIA_NODISCARD inline uint32_t GetWorldVersionFromArchetype(const Archetype& archetype) {
			return archetype.GetWorldVersion();
		}

		GAIA_NODISCARD inline ComponentMatcherHash GetArchetypeMatcherHash(const Archetype& archetype, ComponentType type) {
			return archetype.GetMatcherHash(type);
		}

		GAIA_NODISCARD inline const ComponentInfo* GetComponentInfoFromIdx(uint32_t index) {
			return GetComponentCache().GetComponentInfoFromIdx(index);
		}

		GAIA_NODISCARD inline const ComponentInfoCreate& GetComponentCreateInfoFromIdx(uint32_t index) {
			return GetComponentCache().GetComponentCreateInfoFromIdx(index);
		}

		GAIA_NODISCARD inline const ComponentInfoList&
		GetArchetypeComponentInfoList(const Archetype& archetype, ComponentType type) {
			return archetype.GetComponentInfoList(type);
		}

		GAIA_NODISCARD inline const ComponentLookupList&
		GetArchetypeComponentLookupList(const Archetype& archetype, ComponentType type) {
			return archetype.GetComponentLookupList(type);
		}
	} // namespace ecs
} // namespace gaia

REGISTER_HASH_TYPE(gaia::ecs::Archetype::LookupHash)
REGISTER_HASH_TYPE(gaia::ecs::Archetype::GenericComponentHash)
REGISTER_HASH_TYPE(gaia::ecs::Archetype::ChunkComponentHash)