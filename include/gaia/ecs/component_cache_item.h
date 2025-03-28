#pragma once
#include "../config/config.h"

#include <cstdint>
#include <type_traits>

#include "../core/hashing_string.h"
#include "../mem/mem_alloc.h"
#include "../mem/mem_utils.h"
#include "component.h"
#include "component_desc.h"
#include "id.h"

namespace gaia {
	namespace ecs {
		class World;
		class Chunk;
		struct ComponentRecord;

		struct ComponentCacheItem final {
			using SymbolLookupKey = core::StringLookupKey<512>;
			using FuncCtor = void(void*, uint32_t);
			using FuncDtor = void(void*, uint32_t);
			using FuncFrom = void(void*, void*, uint32_t, uint32_t, uint32_t, uint32_t);
			using FuncCopy = void(void*, const void*, uint32_t, uint32_t, uint32_t, uint32_t);
			using FuncMove = void(void*, void*, uint32_t, uint32_t, uint32_t, uint32_t);
			using FuncSwap = void(void*, void*, uint32_t, uint32_t, uint32_t, uint32_t);
			using FuncCmp = bool(const void*, const void*);

			using FuncOnAdd = void(const World& world, const ComponentCacheItem&, Entity);
			using FuncOnDel = void(const World& world, const ComponentCacheItem&, Entity);
			using FuncOnSet = void(const World& world, const ComponentRecord&, Chunk& chunk);

			//! Component entity
			Entity entity;
			//! Unique component identifier
			Component comp;
			//! Complex hash used for look-ups
			ComponentLookupHash hashLookup;
			//! If component is SoA, this stores how many bytes each of the elements take
			uint8_t soaSizes[meta::StructToTupleMaxTypes];

			//! Component name
			SymbolLookupKey name;
			//! Function to call when the component needs to be constructed
			FuncCtor* func_ctor{};
			//! Function to call when the component needs to be move constructed
			FuncMove* func_move_ctor{};
			//! Function to call when the component needs to be copy constructed
			FuncCopy* func_copy_ctor{};
			//! Function to call when the component needs to be destroyed
			FuncDtor* func_dtor{};
			//! Function to call when the component needs to be copied
			FuncCopy* func_copy{};
			//! Function to call when the component needs to be moved
			FuncMove* func_move{};
			//! Function to call when the component needs to swap
			FuncSwap* func_swap{};
			//! Function to call when comparing two components of the same type
			FuncCmp* func_cmp{};

#if GAIA_ENABLE_HOOKS
			struct Hooks {
				//! Function to call whenever a component is added to an entity
				FuncOnAdd* func_add{};
				//! Function to call whenever a component is deleted from an entity
				FuncOnDel* func_del{};
	#if GAIA_ENABLE_SET_HOOKS
				//! Function to call whenever a component is accessed for modification
				FuncOnSet* func_set{};
	#endif
			};
			Hooks comp_hooks;
#endif

		private:
			ComponentCacheItem() = default;
			~ComponentCacheItem() = default;

		public:
			ComponentCacheItem(const ComponentCacheItem&) = delete;
			ComponentCacheItem(ComponentCacheItem&&) = delete;
			ComponentCacheItem& operator=(const ComponentCacheItem&) = delete;
			ComponentCacheItem& operator=(ComponentCacheItem&&) = delete;

			void
			ctor_move(void* pDst, void* pSrc, uint32_t idxDst, uint32_t idxSrc, uint32_t sizeDst, uint32_t sizeSrc) const {
				GAIA_ASSERT(func_move_ctor != nullptr && (pSrc != pDst || idxSrc != idxDst));
				func_move_ctor(pDst, pSrc, idxDst, idxSrc, sizeDst, sizeSrc);
			}

			void ctor_copy(
					void* pDst, const void* pSrc, uint32_t idxDst, uint32_t idxSrc, uint32_t sizeDst, uint32_t sizeSrc) const {
				GAIA_ASSERT(func_copy_ctor != nullptr && (pSrc != pDst || idxSrc != idxDst));
				func_copy_ctor(pDst, pSrc, idxDst, idxSrc, sizeDst, sizeSrc);
			}

			void dtor(void* pSrc) const {
				if (func_dtor != nullptr)
					func_dtor(pSrc, 1);
			}

			void
			copy(void* pDst, const void* pSrc, uint32_t idxDst, uint32_t idxSrc, uint32_t sizeDst, uint32_t sizeSrc) const {
				GAIA_ASSERT(func_copy != nullptr && (pSrc != pDst || idxSrc != idxDst));
				func_copy(pDst, pSrc, idxDst, idxSrc, sizeDst, sizeSrc);
			}

			void move(void* pDst, void* pSrc, uint32_t idxDst, uint32_t idxSrc, uint32_t sizeDst, uint32_t sizeSrc) const {
				GAIA_ASSERT(func_move != nullptr && (pSrc != pDst || idxSrc != idxDst));
				func_move(pDst, pSrc, idxDst, idxSrc, sizeDst, sizeSrc);
			}

			void
			swap(void* pLeft, void* pRight, uint32_t idxLeft, uint32_t idxRight, uint32_t sizeDst, uint32_t sizeSrc) const {
				GAIA_ASSERT(func_swap != nullptr);
				func_swap(pLeft, pRight, idxLeft, idxRight, sizeDst, sizeSrc);
			}

			bool cmp(const void* pLeft, const void* pRight) const {
				GAIA_ASSERT(pLeft != pRight);
				GAIA_ASSERT(func_cmp != nullptr);
				return func_cmp(pLeft, pRight);
			}

#if GAIA_ENABLE_HOOKS

			Hooks& hooks() {
				return comp_hooks;
			}

			const Hooks& hooks() const {
				return comp_hooks;
			}

#endif

			GAIA_NODISCARD uint32_t calc_new_mem_offset(uint32_t addr, size_t N) const noexcept {
				if (comp.soa() == 0) {
					addr = (uint32_t)mem::detail::get_aligned_byte_offset(addr, comp.alig(), comp.size(), N);
				} else {
					GAIA_FOR(comp.soa()) {
						addr = (uint32_t)mem::detail::get_aligned_byte_offset(addr, comp.alig(), soaSizes[i], N);
					}
					// TODO: Magic offset. Otherwise, SoA data might leak past the chunk boundary when accessing
					//       the last element. By faking the memory offset we can bypass this is issue for now.
					//       Obviously, this needs fixing at some point.
					addr += comp.soa() * 4;
				}
				return addr;
			}

			template <typename T>
			GAIA_NODISCARD static ComponentCacheItem* create(Entity entity) {
				static_assert(core::is_raw_v<T>);

				constexpr auto componentSize = detail::ComponentDesc<T>::size();
				static_assert(
						componentSize < Component::MaxComponentSizeInBytes,
						"Trying to register a component larger than the maximum allowed component size! In the future this "
						"restriction won't apply to components not stored inside archetype chunks.");

				auto* cci = mem::AllocHelper::alloc<ComponentCacheItem>("ComponentCacheItem");
				(void)new (cci) ComponentCacheItem();
				cci->entity = entity;
				cci->comp = Component(
						// component id
						detail::ComponentDesc<T>::id(),
						// soa
						detail::ComponentDesc<T>::soa(cci->soaSizes),
						// size in bytes
						componentSize,
						// alignment
						detail::ComponentDesc<T>::alig());
				cci->hashLookup = detail::ComponentDesc<T>::hash_lookup();

				auto ct_name = detail::ComponentDesc<T>::name();

				// Allocate enough memory for the name string + the null-terminating character (
				// the compile time string returned by ComponentDesc<T>::name is not null-terminated).
				// Different compilers will give a bit different strings, e.g.:
				//   Clang/GCC: gaia::ecs::uni<Position>
				//   MSVC     : gaia::ecs::uni<struct Position>
				// Therefore, we first copy the compile-time string and then tweak it so it is
				// the same on all supported compilers.
				char nameTmp[256];
				auto nameTmpLen = (uint32_t)ct_name.size();
				GAIA_ASSERT(nameTmpLen < 256);
				memcpy((void*)nameTmp, (const void*)ct_name.data(), nameTmpLen + 1);
				nameTmp[ct_name.size()] = 0;

				// Remove "class " or "struct " substrings from the string
				const uint32_t NSubstrings = 2;
				const char* to_remove[NSubstrings] = {"class ", "struct "};
				const uint32_t to_remove_len[NSubstrings] = {6, 7};
				GAIA_FOR(NSubstrings) {
					const auto& str = to_remove[i];
					const auto len = to_remove_len[i];

					auto* pos = nameTmp;
					while ((pos = strstr(pos, str)) != nullptr) {
						memmove(pos, pos + len, strlen(pos + len) + 1);
						nameTmpLen -= len;
					}
				}

				// Allocate the final string
				char* name = mem::AllocHelper::alloc<char>(nameTmpLen + 1);
				memcpy((void*)name, (const void*)nameTmp, nameTmpLen + 1);
				name[nameTmpLen] = 0;

				SymbolLookupKey tmp(name, nameTmpLen);
				cci->name = SymbolLookupKey(tmp.str(), tmp.len(), 1, {tmp.hash()});

				cci->func_ctor = detail::ComponentDesc<T>::func_ctor();
				cci->func_move_ctor = detail::ComponentDesc<T>::func_move_ctor();
				cci->func_copy_ctor = detail::ComponentDesc<T>::func_copy_ctor();
				cci->func_dtor = detail::ComponentDesc<T>::func_dtor();
				cci->func_copy = detail::ComponentDesc<T>::func_copy();
				cci->func_move = detail::ComponentDesc<T>::func_move();
				cci->func_swap = detail::ComponentDesc<T>::func_swap();
				cci->func_cmp = detail::ComponentDesc<T>::func_cmp();
				return cci;
			}

			static void destroy(ComponentCacheItem* pItem) {
				if (pItem == nullptr)
					return;

				if (pItem->name.str() != nullptr && pItem->name.owned()) {
					mem::AllocHelper::free((void*)pItem->name.str());
					pItem->name = {};
				}

				pItem->~ComponentCacheItem();
				mem::AllocHelper::free("ComponentCacheItem", pItem);
			}
		};
	} // namespace ecs
} // namespace gaia
