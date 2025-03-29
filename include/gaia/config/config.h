#pragma once
#include "config_core.h"
#include "version.h"

//------------------------------------------------------------------------------
// General settings.
// You are free to modify these.
//------------------------------------------------------------------------------

//! If enabled, GAIA_DEBUG is defined despite using a "Release" build configuration for example
#if !defined(GAIA_FORCE_DEBUG)
	#define GAIA_FORCE_DEBUG 0
#endif
//! If enabled, no asserts are thrown even in debug builds
#if !defined(GAIA_DISABLE_ASSERTS)
	#define GAIA_DISABLE_ASSERTS 0
#endif

//------------------------------------------------------------------------------
// Internal features.
// You are free to modify these but you probably should not.
// It is expected to only use them when something doesn't work as expected
// or as some sort of workaround.
//------------------------------------------------------------------------------

//! If enabled, custom allocator is used for allocating archetype chunks.
#ifndef GAIA_ECS_CHUNK_ALLOCATOR
	#define GAIA_ECS_CHUNK_ALLOCATOR 1
#endif

//! Hashing algorithm. GAIA_ECS_HASH_FNV1A or GAIA_ECS_HASH_MURMUR2A
#ifndef GAIA_ECS_HASH
	#define GAIA_ECS_HASH GAIA_ECS_HASH_MURMUR2A
#endif

//! If enabled, memory prefetching is used when querying chunks, possibly improving performance in edge-cases
#ifndef GAIA_USE_PREFETCH
	#define GAIA_USE_PREFETCH 1
#endif

//! If enabled, systems as entities are enabled
#ifndef GAIA_SYSTEMS_ENABLED
	#define GAIA_SYSTEMS_ENABLED 1
#endif

//! If enabled, entities are stored in paged-storage. This way, the cost of adding any number of entities
//! is always the same. Blocks of fixed size and stable memory address  are allocated for entity records.
#ifndef GAIA_USE_PAGED_ENTITY_CONTAINER
	#define GAIA_USE_PAGED_ENTITY_CONTAINER 1
#endif

//! If enabled, hooks are enabled for components. Any time a new component is added to, or removed from
//! an entity, they can be triggered. Set hooks for when component value is changed are possible, too.
#ifndef GAIA_ENABLE_HOOKS
	#define GAIA_ENABLE_HOOKS 1
#endif
#ifndef GAIA_ENABLE_SET_HOOKS
	#define GAIA_ENABLE_SET_HOOKS (GAIA_ENABLE_HOOKS && 1)
#else
// If GAIA_ENABLE_SET_HOOKS is defined and GAIA_ENABLE_HOOKS is not, unset it
	#ifndef GAIA_ENABLE_HOOKS
		#undef GAIA_ENABLE_SET_HOOKS
		#define GAIA_ENABLE_SET_HOOKS 0
	#endif
#endif

//! If enabled, reference counting of entities is enabled. This gives you access to ecs::SafeEntity
//! and similar features.
#ifndef GAIA_USE_ENTITY_REFCNT
	#define GAIA_USE_ENTITY_REFCNT 1
#endif

//------------------------------------------------------------------------------

#include "config_core_end.h"
