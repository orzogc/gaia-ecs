#pragma once
#include <cinttypes>
#include <type_traits>

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

		using ComponentHash = utils::direct_hash_key<uint64_t>;
		using ComponentIndexHash = utils::direct_hash_key<uint32_t>;
		using ComponentIndex = uint32_t;

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
					std::bool_constant<std::is_trivially_copyable<T>::value && std::is_default_constructible<T>::value> {};
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
			static_assert(IsComponentTypeValid<U>, "Only components of trivial type are allowed");
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
		constexpr uint64_t CalculateMatcherHash() noexcept;

		template <typename T, typename... Rest>
		GAIA_NODISCARD constexpr uint64_t CalculateMatcherHash() noexcept {
			if constexpr (sizeof...(Rest) == 0)
				return detail::CalculateMatcherHash<T>();
			else
				return utils::combine_or(detail::CalculateMatcherHash<T>(), detail::CalculateMatcherHash<Rest>()...);
		}

		template <>
		GAIA_NODISCARD constexpr uint64_t CalculateMatcherHash() noexcept {
			return 0;
		}

		//-----------------------------------------------------------------------------------

		template <typename Container>
		GAIA_NODISCARD constexpr uint64_t CalculateLookupHash(Container arr) noexcept {
			constexpr auto arrSize = arr.size();
			if constexpr (arrSize == 0) {
				return 0;
			} else {
				uint64_t hash = arr[0];
				utils::for_each<arrSize - 1>([&hash, &arr](auto i) {
					hash = utils::hash_combine(hash, arr[i + 1]);
				});
				return hash;
			}
		}

		template <typename = void, typename...>
		constexpr uint64_t CalculateLookupHash() noexcept;

		template <typename T, typename... Rest>
		GAIA_NODISCARD constexpr uint64_t CalculateLookupHash() noexcept {
			if constexpr (sizeof...(Rest) == 0)
				return utils::type_info::hash<T>();
			else
				return utils::hash_combine(utils::type_info::hash<T>(), utils::type_info::hash<Rest>()...);
		}

		template <>
		GAIA_NODISCARD constexpr uint64_t CalculateLookupHash() noexcept {
			return 0;
		}

		//----------------------------------------------------------------------
		// ComponentInfo
		//----------------------------------------------------------------------

		struct ComponentInfoCreate final {
			using FuncConstructor = void(void*, size_t);
			using FuncDestructor = void(void*, size_t);

			//! [ 0-15] Component name
			std::span<const char> name;
			//! [16-31] Constructor to call when the component is being constructed
			FuncConstructor* constructor;
			//! [32-47] Destructor to call when the component is being destroyed
			FuncDestructor* destructor;
			//! [48-51] Unique component identifier
			uint32_t infoIndex;

			template <typename T>
			GAIA_NODISCARD static constexpr ComponentInfoCreate Calculate() {
				using U = typename DeduceComponent<T>::Type;

				ComponentInfoCreate info{};
				info.name = utils::type_info::name<U>();
				info.infoIndex = utils::type_info::index<U>();

				if constexpr (!std::is_empty_v<U> && !utils::is_soa_layout_v<U>) {
					info.constructor = [](void* ptr, size_t cnt) {
						auto first = (U*)ptr;
						auto last = (U*)ptr + cnt;
						std::uninitialized_default_construct(first, last);
					};
					info.destructor = [](void* ptr, size_t cnt) {
						auto first = (U*)ptr;
						auto last = (U*)ptr + cnt;
						std::destroy(first, last);
					};
				}

				return info;
			}

			template <typename T>
			static ComponentInfoCreate Create() {
				using U = std::decay_t<T>;
				return ComponentInfoCreate::Calculate<U>();
			}
		};

		//----------------------------------------------------------------------
		// ComponentInfo
		//----------------------------------------------------------------------

		struct ComponentInfo final {
			//! [0-7] Complex hash used for look-ups
			uint64_t lookupHash;
			//! [8-15] Simple hash used for matching component
			uint64_t matcherHash;
			//! [16-19] Unique component identifier
			uint32_t infoIndex;
			//! [20-23]
			struct {
				//! Component alignment
				uint32_t alig: MAX_COMPONENTS_SIZE_BITS;
				//! Component size
				uint32_t size: MAX_COMPONENTS_SIZE_BITS;
				//! Tells if the component is laid out in SoA style
				uint32_t soa : 1;
			} properties{};

			GAIA_NODISCARD bool operator==(const ComponentInfo& other) const {
				return lookupHash == other.lookupHash && infoIndex == other.infoIndex;
			}
			GAIA_NODISCARD bool operator!=(const ComponentInfo& other) const {
				return lookupHash != other.lookupHash || infoIndex != other.infoIndex;
			}
			GAIA_NODISCARD bool operator<(const ComponentInfo& other) const {
				return lookupHash < other.lookupHash;
			}

			template <typename T>
			GAIA_NODISCARD static constexpr ComponentInfo Calculate() {
				using U = typename DeduceComponent<T>::Type;

				ComponentInfo info{};
				info.lookupHash = utils::type_info::hash<U>();
				info.matcherHash = CalculateMatcherHash<U>();
				info.infoIndex = utils::type_info::index<U>();

				if constexpr (!std::is_empty_v<U>) {
					info.properties.alig = utils::auto_view_policy<U>::Alignment;
					info.properties.size = (uint32_t)sizeof(U);
					info.properties.soa = utils::is_soa_layout_v<U>;
				}

				return info;
			}

			template <typename T>
			static const ComponentInfo* Create() {
				using U = std::decay_t<T>;
				return new ComponentInfo{Calculate<U>()};
			}
		};

		GAIA_NODISCARD inline uint64_t CalculateMatcherHash(uint64_t hashA, uint64_t hashB) noexcept {
			return utils::combine_or(hashA, hashB);
		}

		GAIA_NODISCARD inline uint64_t CalculateMatcherHash(std::span<const ComponentInfo*> infos) noexcept {
			const auto infosSize = infos.size();
			if (infosSize == 0)
				return 0;

			uint64_t hash = infos[0]->matcherHash;
			for (size_t i = 1; i < infosSize; ++i)
				hash = utils::combine_or(hash, infos[i]->matcherHash);
			return hash;
		}

		GAIA_NODISCARD inline uint64_t CalculateLookupHash(std::span<const ComponentInfo*> infos) noexcept {
			const auto infosSize = infos.size();
			if (infosSize == 0)
				return 0;

			uint64_t hash = infos[0]->lookupHash;
			for (size_t i = 1; i < infosSize; ++i)
				hash = utils::hash_combine(hash, infos[i]->lookupHash);
			return hash;
		}

		//----------------------------------------------------------------------

		struct ComponentLookupData final {
			//! Component info index. A copy of the value in ComponentInfo
			uint32_t infoIndex;
			//! Distance in bytes from the archetype's chunk data segment
			uint32_t offset;
		};

		using ComponentInfoList = containers::sarray_ext<const ComponentInfo*, MAX_COMPONENTS_PER_ARCHETYPE>;
		using ComponentLookupList = containers::sarray_ext<ComponentLookupData, MAX_COMPONENTS_PER_ARCHETYPE>;

	} // namespace ecs
} // namespace gaia

REGISTER_HASH_TYPE(gaia::ecs::ComponentHash)
REGISTER_HASH_TYPE(gaia::ecs::ComponentIndexHash)
