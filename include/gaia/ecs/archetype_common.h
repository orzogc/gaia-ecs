#pragma once
#include <cinttypes>

#include "../containers/darray.h"
#include "../containers/sarray_ext.h"
#include "../utils/hashing_policy.h"
#include "component.h"

namespace gaia {
	namespace ecs {
		namespace archetype {
			//! Maximum number of components on archetype
			constexpr uint32_t MAX_COMPONENTS_PER_ARCHETYPE = 32U;

			class Archetype;

			using ArchetypeId = uint32_t;
			using LookupHash = utils::direct_hash_key<uint64_t>;
			using ArchetypeList = containers::darray<Archetype*>;
			using ComponentIdArray = containers::sarray_ext<component::ComponentId, MAX_COMPONENTS_PER_ARCHETYPE>;
			// uint16_t can fit at most 65535 items therefore MemoryBlockSize can't be set to a value biggen than that
			using ChunkComponentOffset = uint16_t;
			using ComponentOffsetArray = containers::sarray_ext<ChunkComponentOffset, MAX_COMPONENTS_PER_ARCHETYPE>;

			static constexpr ArchetypeId ArchetypeIdBad = (ArchetypeId)-1;

			GAIA_NODISCARD inline constexpr bool VerifyArchetypeComponentCount(uint32_t count) {
				return count <= MAX_COMPONENTS_PER_ARCHETYPE;
			}
		} // namespace archetype
	} // namespace ecs
} // namespace gaia