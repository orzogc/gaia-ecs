#pragma once
#include <cinttypes>
#include <type_traits>

#include "../config/config_core.h"
#include "../containers/darray.h"
#include "../containers/map.h"
#include "../containers/sarray_ext.h"
#include "../utils/data_layout_policy.h"
#include "../utils/hashing_policy.h"
#include "../utils/span.h"
#include "../utils/type_info.h"
#include "../utils/utility.h"
#include "common.h"

namespace gaia {
	namespace ecs {

		static constexpr uint32_t MAX_COMPONENTS_SIZE_BITS = 8;
		//! Maximum size of components in bytes
		static constexpr uint32_t MAX_COMPONENTS_SIZE = (1 << MAX_COMPONENTS_SIZE_BITS) - 1;

		enum ComponentType : uint8_t {
			// General purpose component
			CT_Generic = 0,
			// Chunk component
			CT_Chunk,
			// Number of component types
			CT_Count
		};

		inline const char* const ComponentTypeString[ComponentType::CT_Count] = {"Generic", "Chunk"};

		struct ComponentInfo;

		using ComponentId = uint32_t;
		using ComponentLookupHash = utils::direct_hash_key<uint64_t>;
		using ComponentMatcherHash = utils::direct_hash_key<uint64_t>;

		//----------------------------------------------------------------------
		// Component type deduction
		//----------------------------------------------------------------------

		template <typename T>
		struct AsChunk {
			using TType = typename std::decay_t<typename std::remove_pointer_t<T>>;
			using TTypeOriginal = T;
			static constexpr ComponentType TComponentType = ComponentType::CT_Chunk;
		};

		namespace detail {
			template <typename T>
			struct ExtractComponentType_Generic {
				using Type = typename std::decay_t<typename std::remove_pointer_t<T>>;
				using TypeOriginal = T;
			};
			template <typename T>
			struct ExtractComponentType_NonGeneric {
				using Type = typename T::TType;
				using TypeOriginal = typename T::TTypeOriginal;
			};

			template <typename T, typename = void>
			struct IsGenericComponent_Internal: std::true_type {};
			template <typename T>
			struct IsGenericComponent_Internal<T, decltype((void)T::TComponentType, void())>: std::false_type {};

			template <typename T>
			struct IsComponentSizeValid_Internal: std::bool_constant<sizeof(T) < MAX_COMPONENTS_SIZE> {};

			template <typename T>
			struct IsComponentTypeValid_Internal:
					std::bool_constant<
							// SoA types need to be trivial. No restrictions otherwise.
							(!utils::is_soa_layout_v<T> || std::is_trivially_copyable_v<T>)> {};
		} // namespace detail

		template <typename T>
		inline constexpr bool IsGenericComponent = detail::IsGenericComponent_Internal<T>::value;
		template <typename T>
		inline constexpr bool IsComponentSizeValid = detail::IsComponentSizeValid_Internal<T>::value;
		template <typename T>
		inline constexpr bool IsComponentTypeValid = detail::IsComponentTypeValid_Internal<T>::value;

		template <typename T>
		using DeduceComponent = std::conditional_t<
				IsGenericComponent<T>, typename detail::ExtractComponentType_Generic<T>,
				typename detail::ExtractComponentType_NonGeneric<T>>;

		//! Returns the component id for \tparam T
		//! \return Component id
		template <typename T>
		GAIA_NODISCARD inline ComponentId GetComponentId() {
			using U = typename DeduceComponent<T>::Type;
			return utils::type_info::id<U>();
		}

		//! Returns the component id for \tparam T
		//! \warning Does not perform any deduction for \tparam T.
		//!          Passing "const X" and "X" would therefore yield to different results.
		//!          Therefore, this must be used only when we known \tparam T is the deduced "raw" type.
		//! \return Component id
		template <typename T>
		GAIA_NODISCARD inline ComponentId GetComponentIdUnsafe() {
			// This is essentially the same thing as GetComponentId but when used correctly
			// we can save some compilation time.
			return utils::type_info::id<T>();
		}

		template <typename T>
		struct IsReadOnlyType:
				std::bool_constant<
						std::is_const_v<std::remove_reference_t<std::remove_pointer_t<T>>> ||
						(!std::is_pointer<T>::value && !std::is_reference<T>::value)> {};

		//----------------------------------------------------------------------
		// Component verification
		//----------------------------------------------------------------------

		template <typename T>
		constexpr void VerifyComponent() {
			using U = typename DeduceComponent<T>::Type;
			// Make sure we only use this for "raw" types
			static_assert(!std::is_const_v<U>);
			static_assert(!std::is_pointer_v<U>);
			static_assert(!std::is_reference_v<U>);
			static_assert(!std::is_volatile_v<U>);
			static_assert(IsComponentSizeValid<U>, "MAX_COMPONENTS_SIZE in bytes is exceeded");
			static_assert(IsComponentTypeValid<U>, "Component type restrictions not met");
		}

		//----------------------------------------------------------------------
		// Component hash operations
		//----------------------------------------------------------------------

		namespace detail {
			template <typename T>
			constexpr uint64_t CalculateMatcherHash() noexcept {
				return (uint64_t(1) << (utils::type_info::hash<T>() % uint64_t(63)));
			}
		} // namespace detail

		template <typename = void, typename...>
		constexpr ComponentMatcherHash CalculateMatcherHash() noexcept;

		template <typename T, typename... Rest>
		GAIA_NODISCARD constexpr ComponentMatcherHash CalculateMatcherHash() noexcept {
			if constexpr (sizeof...(Rest) == 0)
				return {detail::CalculateMatcherHash<T>()};
			else
				return {utils::combine_or(detail::CalculateMatcherHash<T>(), detail::CalculateMatcherHash<Rest>()...)};
		}

		template <>
		GAIA_NODISCARD constexpr ComponentMatcherHash CalculateMatcherHash() noexcept {
			return {0};
		}

		//-----------------------------------------------------------------------------------

		template <typename Container>
		GAIA_NODISCARD constexpr ComponentLookupHash CalculateLookupHash(Container arr) noexcept {
			constexpr auto arrSize = arr.size();
			if constexpr (arrSize == 0) {
				return {0};
			} else {
				ComponentLookupHash::Type hash = arr[0];
				utils::for_each<arrSize - 1>([&hash, &arr](auto i) {
					hash = utils::hash_combine(hash, arr[i + 1]);
				});
				return {hash};
			}
		}

		template <typename = void, typename...>
		constexpr ComponentLookupHash CalculateLookupHash() noexcept;

		template <typename T, typename... Rest>
		GAIA_NODISCARD constexpr ComponentLookupHash CalculateLookupHash() noexcept {
			if constexpr (sizeof...(Rest) == 0)
				return {utils::type_info::hash<T>()};
			else
				return {utils::hash_combine(utils::type_info::hash<T>(), utils::type_info::hash<Rest>()...)};
		}

		template <>
		GAIA_NODISCARD constexpr ComponentLookupHash CalculateLookupHash() noexcept {
			return {0};
		}

		//----------------------------------------------------------------------
		// ComponentDesc
		//----------------------------------------------------------------------

		struct ComponentDesc final {
			using FuncDestructor = void(void*, size_t);
			using FuncCopy = void(void*, void*);
			using FuncMove = void(void*, void*);

			//! Component name
			std::span<const char> name;
			//! Destructor to call when the component is destroyed
			FuncDestructor* destructor = nullptr;
			//! Function to call when the component is copied
			FuncMove* copy = nullptr;
			//! Fucntion to call when the component is moved
			FuncMove* move = nullptr;
			//! Unique component identifier
			ComponentId componentId = (ComponentId)-1;

			//! Various component properties
			struct {
				//! Component alignment
				uint32_t alig: MAX_COMPONENTS_SIZE_BITS;
				//! Component size
				uint32_t size: MAX_COMPONENTS_SIZE_BITS;
				//! Tells if the component is laid out in SoA style
				uint32_t soa : 1;
				//! Tells if the component is destructible
				uint32_t destructible : 1;
				//! Tells if the component is copyable
				uint32_t copyable : 1;
				//! Tells if the component is movable
				uint32_t movable : 1;
			} properties{};

			template <typename T>
			GAIA_NODISCARD static constexpr ComponentDesc Calculate() {
				using U = typename DeduceComponent<T>::Type;

				ComponentDesc info{};
				info.name = utils::type_info::name<U>();
				info.componentId = GetComponentIdUnsafe<U>();

				if constexpr (!std::is_empty_v<U> && !utils::is_soa_layout_v<U>) {
					// Custom destruction
					if constexpr (!std::is_trivially_destructible_v<U>) {
						info.destructor = [](void* ptr, size_t cnt) {
							auto first = (U*)ptr;
							auto last = (U*)ptr + cnt;
							std::destroy(first, last);
						};
					}

					// Copyability
					if (!std::is_trivially_copyable_v<U>) {
						if constexpr (std::is_copy_assignable_v<U>) {
							info.copy = [](void* from, void* to) {
								auto src = (U*)from;
								auto dst = (U*)to;
								*dst = *src;
							};
						} else if constexpr (std::is_copy_constructible_v<U>) {
							info.copy = [](void* from, void* to) {
								auto src = (U*)from;
								auto dst = (U*)to;
								*dst = U(*src);
							};
						}
					}

					// Movability
					if constexpr (!std::is_trivially_move_assignable_v<U> && std::is_move_assignable_v<U>) {
						info.move = [](void* from, void* to) {
							auto src = (U*)from;
							auto dst = (U*)to;
							*dst = std::move(*src);
						};
					} else if constexpr (!std::is_trivially_move_constructible_v<U> && std::is_move_constructible_v<U>) {
						info.move = [](void* from, void* to) {
							auto src = (U*)from;
							auto dst = (U*)to;
							*dst = U(std::move(*src));
						};
					}
				}

				if constexpr (!std::is_empty_v<U>) {
					info.properties.alig = utils::auto_view_policy<U>::Alignment;
					info.properties.size = (uint32_t)sizeof(U);

					if constexpr (utils::is_soa_layout_v<U>) {
						info.properties.soa = 1;
					} else {
						info.properties.destructible = !std::is_trivially_destructible_v<U>;
						info.properties.copyable =
								!std::is_trivially_copyable_v<U> && (std::is_copy_assignable_v<U> || std::is_copy_constructible_v<U>);
						info.properties.movable = (!std::is_trivially_move_assignable_v<U> && std::is_move_assignable_v<U>) ||
																			(!std::is_trivially_move_constructible_v<U> && std::is_move_constructible_v<U>);
					}
				}

				return info;
			}

			template <typename T>
			GAIA_NODISCARD static ComponentDesc Create() {
				using U = std::decay_t<T>;
				return ComponentDesc::Calculate<U>();
			}
		};

		//----------------------------------------------------------------------
		// ComponentInfo
		//----------------------------------------------------------------------

		struct ComponentInfo final {
			//! Complex hash used for look-ups
			ComponentLookupHash lookupHash;
			//! Simple hash used for matching component
			ComponentMatcherHash matcherHash;
			//! Unique component identifier
			ComponentId componentId;

			GAIA_NODISCARD bool operator==(const ComponentInfo& other) const {
				return lookupHash == other.lookupHash && componentId == other.componentId;
			}
			GAIA_NODISCARD bool operator!=(const ComponentInfo& other) const {
				return lookupHash != other.lookupHash || componentId != other.componentId;
			}
			GAIA_NODISCARD bool operator<(const ComponentInfo& other) const {
				return componentId < other.componentId;
			}

			template <typename T>
			GAIA_NODISCARD static constexpr ComponentInfo Calculate() {
				using U = typename DeduceComponent<T>::Type;

				ComponentInfo info{};
				info.lookupHash = {utils::type_info::hash<U>()};
				info.matcherHash = CalculateMatcherHash<U>();
				info.componentId = utils::type_info::id<U>();

				return info;
			}

			template <typename T>
			static const ComponentInfo* Create() {
				using U = std::decay_t<T>;
				return new ComponentInfo{Calculate<U>()};
			}
		};

		using ComponentIdList = containers::sarray_ext<ComponentId, MAX_COMPONENTS_PER_ARCHETYPE>;
		using ComponentIdSpan = std::span<const ComponentId>;

		//----------------------------------------------------------------------
		// ComponentLookupData
		//----------------------------------------------------------------------

		using ComponentOffsetList = containers::sarray_ext<ComponentId, MAX_COMPONENTS_PER_ARCHETYPE>;

	} // namespace ecs
} // namespace gaia

REGISTER_HASH_TYPE(gaia::ecs::ComponentLookupHash)
REGISTER_HASH_TYPE(gaia::ecs::ComponentMatcherHash)
