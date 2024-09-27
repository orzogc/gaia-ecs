#pragma once
#include "../config/config.h"

#include <cinttypes>
#include <cstdint>
#include <type_traits>

#include "../core/iterator.h"
#include "../mem/data_layout_policy.h"
#include "archetype.h"
#include "chunk.h"
#include "component.h"
#include "component_cache_item.h"
#include "id.h"
#include "query_common.h"

namespace gaia {
	namespace ecs {
		class World;

		template <typename T>
		const ComponentCacheItem& comp_cache_add(World& world);

		//! QueryImpl constraints
		enum class Constraints : uint8_t { EnabledOnly, DisabledOnly, AcceptAll };

		namespace detail {
			template <Constraints IterConstraint>
			class ChunkIterImpl {
			protected:
				using CompIndicesBitView = core::bit_view<ChunkHeader::MAX_COMPONENTS_BITS>;

				//! Archetype owning the chunk
				const Archetype* m_pArchetype = nullptr;
				//! Chunk currently associated with the iterator
				Chunk* m_pChunk = nullptr;
				//! ChunkHeader::MAX_COMPONENTS values for component indices mapping for the parent archetype
				const uint8_t* m_pCompIdxMapping = nullptr;
				//! GroupId. 0 if not set.
				GroupId m_groupId = 0;

			public:
				ChunkIterImpl() = default;
				~ChunkIterImpl() = default;
				ChunkIterImpl(ChunkIterImpl&&) noexcept = default;
				ChunkIterImpl& operator=(ChunkIterImpl&&) noexcept = default;
				ChunkIterImpl(const ChunkIterImpl&) = delete;
				ChunkIterImpl& operator=(const ChunkIterImpl&) = delete;

				void set_archetype(const Archetype* pArchetype) {
					GAIA_ASSERT(pArchetype != nullptr);
					m_pArchetype = pArchetype;
				}

				void set_chunk(Chunk* pChunk) {
					GAIA_ASSERT(pChunk != nullptr);
					m_pChunk = pChunk;
				}

				void set_remapping_indices(const uint8_t* pCompIndicesMapping) {
					m_pCompIdxMapping = pCompIndicesMapping;
				}

				void set_group_id(GroupId groupId) {
					m_groupId = groupId;
				}

				GAIA_NODISCARD GroupId group_id() const {
					return m_groupId;
				}

				//! Returns a read-only entity or component view.
				//! \warning If \tparam T is a component it is expected it is present. Undefined behavior otherwise.
				//! \tparam T Component or Entity
				//! \return Entity of component view with read-only access
				template <typename T>
				GAIA_NODISCARD auto view() const {
					return m_pArchetype->view<T>(m_pChunk->m_header, from(), to());
				}

				template <typename T>
				GAIA_NODISCARD auto view(uint32_t termIdx) {
					using U = typename actual_type_t<T>::Type;

					const auto compIdx = m_pCompIdxMapping[termIdx];
					GAIA_ASSERT(compIdx < m_pArchetype->ids_view().size());

					if constexpr (mem::is_soa_layout_v<U>) {
						auto* pData = m_pArchetype->comp_ptr_mut(m_pChunk->m_header, compIdx);
						return m_pArchetype->view_raw<T>(pData, m_pChunk->capacity());
					} else {
						auto* pData = m_pArchetype->comp_ptr_mut(m_pChunk->m_header, compIdx, from());
						return m_pArchetype->view_raw<T>(pData, to() - from());
					}
				}

				//! Returns a mutable entity or component view.
				//! \warning If \tparam T is a component it is expected it is present. Undefined behavior otherwise.
				//! \tparam T Component or Entity
				//! \return Entity or component view with read-write access
				template <typename T>
				GAIA_NODISCARD auto view_mut() {
					return const_cast<Archetype*>(m_pArchetype)->view_mut<T>(m_pChunk->m_header, from(), to());
				}

				template <typename T>
				GAIA_NODISCARD auto view_mut(uint32_t termIdx) {
					using U = typename actual_type_t<T>::Type;

					const auto compIdx = m_pCompIdxMapping[termIdx];
					GAIA_ASSERT(compIdx < m_pChunk->comp_rec_view().size());

					m_pChunk->update_world_version(compIdx);

					if constexpr (mem::is_soa_layout_v<U>) {
						auto* pData = m_pArchetype->comp_ptr_mut(m_pChunk->m_header, compIdx);
						return m_pArchetype->view_mut_raw<T>(pData, m_pChunk->capacity());
					} else {
						auto* pData = m_pArchetype->comp_ptr_mut(m_pChunk->m_header, compIdx, from());
						return m_pArchetype->view_mut_raw<T>(pData, to() - from());
					}
				}

				//! Returns a mutable component view.
				//! Doesn't update the world version when the access is acquired.
				//! \warning It is expected the component \tparam T is present. Undefined behavior otherwise.
				//! \tparam T Component
				//! \return Component view with read-write access
				template <typename T>
				GAIA_NODISCARD auto sview_mut() {
					return const_cast<Archetype*>(m_pArchetype)->sview_mut<T>(m_pChunk->m_header, from(), to());
				}

				template <typename T>
				GAIA_NODISCARD auto sview_mut(uint32_t termIdx) {
					using U = typename actual_type_t<T>::Type;

					const auto compIdx = m_pCompIdxMapping[termIdx];
					GAIA_ASSERT(compIdx < m_pArchetype->ids_view().size());

					if constexpr (mem::is_soa_layout_v<U>) {
						auto* pData = m_pArchetype->comp_ptr_mut(m_pChunk->m_header, compIdx);
						return m_pArchetype->view_mut_raw<T>(m_pChunk->m_header, pData, m_pChunk->capacity());
					} else {
						auto* pData = m_pArchetype->comp_ptr_mut(m_pChunk->m_header, compIdx, from());
						return m_pArchetype->view_mut_raw<T>(pData, to() - from());
					}
				}

				//! Returns either a mutable or immutable entity/component view based on the requested type.
				//! Value and const types are considered immutable. Anything else is mutable.
				//! \warning If \tparam T is a component it is expected to be present. Undefined behavior otherwise.
				//! \tparam T Component or Entity
				//! \return Entity or component view
				template <typename T>
				GAIA_NODISCARD auto view_auto() {
					return const_cast<Archetype*>(m_pArchetype)->view_auto<T>(m_pChunk->m_header, from(), to());
				}

				//! Returns either a mutable or immutable entity/component view based on the requested type.
				//! Value and const types are considered immutable. Anything else is mutable.
				//! Doesn't update the world version when read-write access is acquired.
				//! \warning If \tparam T is a component it is expected to be present. Undefined behavior otherwise.
				//! \tparam T Component or Entity
				//! \return Entity or component view
				template <typename T>
				GAIA_NODISCARD auto sview_auto() {
					return m_pArchetype->sview_auto<T>(m_pChunk->m_header, from(), to());
				}

				//! Checks if the entity at the current iterator index is enabled.
				//! \return True it the entity is enabled. False otherwise.
				GAIA_NODISCARD bool enabled(uint32_t index) const {
					const auto row = (uint16_t)(from() + index);
					return m_pChunk->enabled(row);
				}

				//! Checks if entity \param entity is present in the chunk.
				//! \param entity Entity
				//! \return True if the component is present. False otherwise.
				GAIA_NODISCARD bool has(Entity entity) const {
					return m_pArchetype->has(entity);
				}

				//! Checks if relationship pair \param pair is present in the chunk.
				//! \param pair Relationship pair
				//! \return True if the component is present. False otherwise.
				GAIA_NODISCARD bool has(Pair pair) const {
					return m_pArchetype->has((Entity)pair);
				}

				//! Checks if component \tparam T is present in the chunk.
				//! \tparam T Component
				//! \return True if the component is present. False otherwise.
				template <typename T>
				GAIA_NODISCARD bool has() const {
					return m_pArchetype->has<T>();
				}

				//! Returns the number of entities accessible via the iterator
				GAIA_NODISCARD uint16_t size() const noexcept {
					if constexpr (IterConstraint == Constraints::EnabledOnly)
						return m_pChunk->size_enabled();
					else if constexpr (IterConstraint == Constraints::DisabledOnly)
						return m_pChunk->size_disabled();
					else
						return m_pChunk->size();
				}

				//! Returns the absolute index that should be used to access an item in the chunk.
				//! AoS indices map directly, SoA indices need some adjustments because the view is
				//! always considered {0..ChunkCapacity} instead of {FirstEnabled..ChunkSize}.
				template <typename T>
				uint32_t acc_index(uint32_t idx) const noexcept {
					using U = typename actual_type_t<T>::Type;

					if constexpr (mem::is_soa_layout_v<U>)
						return idx + from();
					else
						return idx;
				}

			protected:
				//! Returns the starting index of the iterator
				GAIA_NODISCARD uint16_t from() const noexcept {
					if constexpr (IterConstraint == Constraints::EnabledOnly)
						return m_pChunk->size_disabled();
					else
						return 0;
				}

				//! Returns the ending index of the iterator (one past the last valid index)
				GAIA_NODISCARD uint16_t to() const noexcept {
					if constexpr (IterConstraint == Constraints::DisabledOnly)
						return m_pChunk->size_disabled();
					else
						return m_pChunk->size();
				}
			};
		} // namespace detail

		//! Iterator for iterating enabled entities
		using Iter = detail::ChunkIterImpl<Constraints::EnabledOnly>;
		//! Iterator for iterating disabled entities
		using IterDisabled = detail::ChunkIterImpl<Constraints::DisabledOnly>;

		//! Iterator for iterating both enabled and disabled entities.
		//! Disabled entities always precede enabled ones.
		class IterAll: public detail::ChunkIterImpl<Constraints::AcceptAll> {
		public:
			//! Returns the number of enabled entities accessible via the iterator.
			GAIA_NODISCARD uint16_t size_enabled() const noexcept {
				return m_pChunk->size_enabled();
			}

			//! Returns the number of disabled entities accessible via the iterator.
			//! Can be read also as "the index of the first enabled entity".
			GAIA_NODISCARD uint16_t size_disabled() const noexcept {
				return m_pChunk->size_disabled();
			}
		};
	} // namespace ecs
} // namespace gaia