#pragma once
#include <cstdint>

#include "chunk.h"
#include "comp/component.h"

namespace gaia {
	namespace ecs {
		struct ComponentSetter {
			archetype::Chunk* m_pChunk;
			uint32_t m_idx;

			//! Sets the value of the component \tparam T on \param entity.
			//! \tparam T Component
			//! \param value Value to set for the component
			//! \return ComponentSetter
			template <typename T, typename U = typename comp::component_type_t<T>::Type>
			U& set() {
				comp::verify_comp<T>();

				if constexpr (comp::component_type_v<T> == comp::ComponentType::CT_Generic)
					return m_pChunk->template set<T>(m_idx);
				else
					return m_pChunk->template set<T>();
			}

			//! Sets the value of the component \tparam T on \param entity.
			//! \tparam T Component
			//! \param value Value to set for the component
			//! \return ComponentSetter
			template <typename T, typename U = typename comp::component_type_t<T>::Type>
			ComponentSetter& set(U&& data) {
				comp::verify_comp<T>();

				if constexpr (comp::component_type_v<T> == comp::ComponentType::CT_Generic)
					m_pChunk->template set<T>(m_idx, std::forward<U>(data));
				else
					m_pChunk->template set<T>(std::forward<U>(data));
				return *this;
			}

			//! Sets the value of the component \tparam T on \param entity without trigger a world version update.
			//! \tparam T Component
			//! \param value Value to set for the component
			//! \return ComponentSetter
			template <typename T, typename U = typename comp::component_type_t<T>::Type>
			ComponentSetter& sset(U&& data) {
				comp::verify_comp<T>();

				if constexpr (comp::component_type_v<T> == comp::ComponentType::CT_Generic)
					m_pChunk->template sset<T>(m_idx, std::forward<U>(data));
				else
					m_pChunk->template sset<T>(std::forward<U>(data));
				return *this;
			}
		};
	} // namespace ecs
} // namespace gaia