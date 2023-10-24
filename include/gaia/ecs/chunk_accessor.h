#pragma once
#include "../config/config.h"

#include <cinttypes>
#include <type_traits>

#include "chunk.h"
#include "component.h"
#include "component_getter.h"
#include "component_setter.h"

namespace gaia {
	namespace ecs {
		namespace detail {
			class ChunkAccessor {
			protected:
				archetype::Chunk& m_chunk;

			public:
				ChunkAccessor(archetype::Chunk& chunk): m_chunk(chunk) {}

				//! Checks if component \tparam T is present in the chunk.
				//! \tparam T Component
				//! \return True if the component is present. False otherwise.
				template <typename T>
				GAIA_NODISCARD bool Has() const {
					return m_chunk.Has<T>();
				}

				//! Checks if the entity at the current iterator index is enabled.
				//! \return True it the entity is enabled. False otherwise.
				GAIA_NODISCARD bool IsEnabled(uint32_t entityIdx) const {
					return entityIdx >= m_chunk.GetDisabledEntityCount() && entityIdx < m_chunk.GetEntityCount();
				}

				//! Returns a read-only entity or component view.
				//! \warning If \tparam T is a component it is expected it is present. Undefined behavior otherwise.
				//! \tparam T Component or Entity
				//! \return Entity of component view with read-only access
				template <typename T>
				GAIA_NODISCARD auto View() const {
					return m_chunk.View<T>();
				}

				//! Returns a mutable entity or component view.
				//! \warning If \tparam T is a component it is expected it is present. Undefined behavior otherwise.
				//! \tparam T Component or Entity
				//! \return Entity or component view with read-write access
				template <typename T>
				GAIA_NODISCARD auto ViewRW() {
					return m_chunk.ViewRW<T>();
				}

				//! Returns a mutable component view.
				//! Doesn't update the world version when the access is aquired.
				//! \warning It is expected the component \tparam T is present. Undefined behavior otherwise.
				//! \tparam T Component
				//! \return Component view with read-write access
				template <typename T>
				GAIA_NODISCARD auto ViewRWSilent() {
					return m_chunk.ViewRWSilent<T>();
				}
			};
		} // namespace detail

		struct ChunkAccessorIt {
			using value_type = uint32_t;

		protected:
			value_type m_pos;

		public:
			ChunkAccessorIt(value_type pos) noexcept: m_pos(pos) {}

			GAIA_NODISCARD value_type operator*() const noexcept {
				return m_pos;
			}

			GAIA_NODISCARD value_type operator->() const noexcept {
				return m_pos;
			}

			ChunkAccessorIt operator++() noexcept {
				++m_pos;
				return *this;
			}

			GAIA_NODISCARD ChunkAccessorIt operator++(int) noexcept {
				ChunkAccessorIt temp(*this);
				++*this;
				return temp;
			}

			GAIA_NODISCARD bool operator==(const ChunkAccessorIt& other) const noexcept {
				return m_pos == other.m_pos;
			}

			GAIA_NODISCARD bool operator!=(const ChunkAccessorIt& other) const noexcept {
				return m_pos != other.m_pos;
			}

			GAIA_NODISCARD bool operator<(const ChunkAccessorIt& other) const noexcept {
				return m_pos < other.m_pos;
			}
		};
	} // namespace ecs
} // namespace gaia