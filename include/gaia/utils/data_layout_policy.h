#pragma once
#include "../config/config.h"

#include <tuple>
#include <type_traits>
#include <utility>

#include "../utils/span.h"
#include "../utils/utility.h"
#include "mem_alloc.h"
#include "reflection.h"

namespace gaia {
	namespace utils {
		enum class DataLayout : uint32_t {
			AoS, //< Array Of Structures
			SoA, //< Structure Of Arrays, 4 packed items, good for SSE and similar
			SoA8, //< Structure Of Arrays, 8 packed items, good for AVX and similar
			SoA16, //< Structure Of Arrays, 16 packed items, good for AVX512 and similar

			Count = 4
		};

		// Helper templates
		namespace detail {
			//----------------------------------------------------------------------
			// Byte offset of a member of SoA-organized data
			//----------------------------------------------------------------------

			inline constexpr size_t get_aligned_byte_offset(uintptr_t address, size_t alig, size_t itemSize, size_t cnt) {
				const auto padding = utils::padding(address, alig);
				address += padding + itemSize * cnt;
				return address;
			}

			template <typename T, size_t Alignment>
			constexpr size_t get_aligned_byte_offset(uintptr_t address, size_t cnt) {
				const auto padding = utils::padding<Alignment>(address);
				const auto itemSize = sizeof(T);
				address += padding + itemSize * cnt;
				return address;
			}

			//----------------------------------------------------------------------
			// Data storage
			//----------------------------------------------------------------------

			template <uint32_t Size, uint32_t Aligment>
			struct raw_data_holder {
				alignas(Aligment) uint8_t data[Size];

				constexpr operator uint8_t*() noexcept {
					return data;
				}

				constexpr operator const uint8_t*() const noexcept {
					return data;
				}
			};

			template <uint32_t Size>
			struct raw_data_holder<Size, 0> {
				uint8_t data[Size];

				constexpr operator uint8_t*() noexcept {
					return data;
				}

				constexpr operator const uint8_t*() const noexcept {
					return data;
				}
			};
		} // namespace detail

		template <DataLayout TDataLayout, typename TItem>
		struct data_layout_properties;
		template <typename TItem>
		struct data_layout_properties<DataLayout::AoS, TItem> {
			constexpr static DataLayout Layout = DataLayout::AoS;
			constexpr static size_t PackSize = 1;
			constexpr static size_t Alignment = alignof(TItem);
			constexpr static size_t ArrayAlignment = 0;
		};
		template <typename TItem>
		struct data_layout_properties<DataLayout::SoA, TItem> {
			constexpr static DataLayout Layout = DataLayout::SoA;
			constexpr static size_t PackSize = 4;
			constexpr static size_t Alignment = PackSize * 4;
			constexpr static size_t ArrayAlignment = Alignment;
		};
		template <typename TItem>
		struct data_layout_properties<DataLayout::SoA8, TItem> {
			constexpr static DataLayout Layout = DataLayout::SoA8;
			constexpr static size_t PackSize = 8;
			constexpr static size_t Alignment = PackSize * 4;
			constexpr static size_t ArrayAlignment = Alignment;
		};
		template <typename TItem>
		struct data_layout_properties<DataLayout::SoA16, TItem> {
			constexpr static DataLayout Layout = DataLayout::SoA16;
			constexpr static size_t PackSize = 16;
			constexpr static size_t Alignment = PackSize * 4;
			constexpr static size_t ArrayAlignment = Alignment;
		};

		template <DataLayout TDataLayout, typename TItem>
		struct data_view_policy;

		template <DataLayout TDataLayout, typename TItem>
		struct data_view_policy_get;
		template <DataLayout TDataLayout, typename TItem>
		struct data_view_policy_set;

		template <DataLayout TDataLayout, typename TItem, size_t Ids>
		struct data_view_policy_get_idx;
		template <DataLayout TDataLayout, typename TItem, size_t Ids>
		struct data_view_policy_set_idx;

		/*!
		 * data_view_policy for accessing and storing data in the AoS way
		 *	Good for random access and when acessing data together.
		 *
		 * struct Foo { int x; int y; int z; };
		 * using fooViewPolicy = data_view_policy<DataLayout::AoS, Foo>;
		 *
		 * Memory is going be be organized as:
		 *		xyz xyz xyz xyz
		 */
		template <typename ValueType>
		struct data_view_policy_aos {
			using TargetCastType = std::add_pointer_t<ValueType>;

			constexpr static DataLayout Layout = data_layout_properties<DataLayout::AoS, ValueType>::Layout;
			constexpr static size_t Alignment = data_layout_properties<DataLayout::AoS, ValueType>::Alignment;
			constexpr static size_t ArrayAlignment = data_layout_properties<DataLayout::AoS, ValueType>::ArrayAlignment;

			GAIA_NODISCARD static constexpr uint32_t get_min_byte_size(uintptr_t addr, size_t cnt) noexcept {
				const auto offset = detail::get_aligned_byte_offset<ValueType, Alignment>(addr, cnt);
				return (uint32_t)(offset - addr);
			}

			GAIA_NODISCARD static uint8_t* alloc_mem(size_t cnt) noexcept {
				const auto bytes = get_min_byte_size(0, cnt);
				auto* pData = (uint8_t*)utils::mem_alloc(bytes);
				utils::call_ctor((ValueType*)pData, cnt);
				return pData;
			}

			static void free_mem(uint8_t* pData, size_t cnt) noexcept {
				if (pData == nullptr)
					return;
				utils::call_dtor((ValueType*)pData, cnt);
				return utils::mem_free(pData);
			}

			GAIA_NODISCARD constexpr static ValueType get_value(std::span<const ValueType> s, size_t idx) noexcept {
				return s[idx];
			}

			GAIA_NODISCARD constexpr static const ValueType& get(std::span<const ValueType> s, size_t idx) noexcept {
				return s[idx];
			}

			GAIA_NODISCARD constexpr static ValueType& set(std::span<ValueType> s, size_t idx) noexcept {
				return s[idx];
			}

			constexpr static void set(std::span<ValueType> s, size_t idx, ValueType&& val) noexcept {
				s[idx] = std::forward<ValueType>(val);
			}
		};

		template <typename ValueType>
		struct data_view_policy<DataLayout::AoS, ValueType>: data_view_policy_aos<ValueType> {};

		template <typename ValueType>
		struct data_view_policy_aos_get {
			using view_policy = data_view_policy_aos<ValueType>;

			//! Raw data pointed to by the view policy
			std::span<const uint8_t> m_data;

			data_view_policy_aos_get(std::span<uint8_t> data): m_data({(const uint8_t*)data.data(), data.size()}) {}
			data_view_policy_aos_get(std::span<const uint8_t> data): m_data({(const uint8_t*)data.data(), data.size()}) {}
			template <typename C>
			data_view_policy_aos_get(const C& c): m_data({(const uint8_t*)c.data(), c.size()}) {
				static_assert(!std::is_same_v<C, data_view_policy_aos_get>);
			}

			GAIA_NODISCARD const ValueType& operator[](size_t idx) const noexcept {
				return std::span<const ValueType>{(const ValueType*)m_data.data(), m_data.size()}[idx];
			}

			GAIA_NODISCARD auto data() const noexcept {
				return m_data.data();
			}

			GAIA_NODISCARD auto size() const noexcept {
				return m_data.size();
			}
		};

		template <typename ValueType>
		struct data_view_policy_get<DataLayout::AoS, ValueType>: data_view_policy_aos_get<ValueType> {};

		template <typename ValueType>
		struct data_view_policy_aos_set {
			using view_policy = data_view_policy_aos<ValueType>;

			//! Raw data pointed to by the view policy
			std::span<uint8_t> m_data;

			data_view_policy_aos_set(std::span<uint8_t> data): m_data({(uint8_t*)data.data(), data.size()}) {}
			data_view_policy_aos_set(std::span<const uint8_t> data): m_data({(uint8_t*)data.data(), data.size()}) {}
			template <typename C>
			data_view_policy_aos_set(const C& c): m_data({(uint8_t*)c.data(), c.size()}) {
				static_assert(!std::is_same_v<C, data_view_policy_aos_set>);
			}

			GAIA_NODISCARD ValueType& operator[](size_t idx) noexcept {
				return std::span<ValueType>{(ValueType*)m_data.data(), m_data.size()}[idx];
			}

			GAIA_NODISCARD const ValueType& operator[](size_t idx) const noexcept {
				return std::span<const ValueType>{(const ValueType*)m_data.data(), m_data.size()}[idx];
			}

			GAIA_NODISCARD auto data() const noexcept {
				return m_data.data();
			}

			GAIA_NODISCARD auto size() const noexcept {
				return m_data.size();
			}
		};

		template <typename ValueType>
		struct data_view_policy_set<DataLayout::AoS, ValueType>: data_view_policy_aos_set<ValueType> {};

		/*!
		 * data_view_policy for accessing and storing data in the SoA way
		 *	Good for SIMD processing.
		 *
		 * struct Foo { int x; int y; int z; };
		 * using fooViewPolicy = data_view_policy<DataLayout::SoA, Foo>;
		 *
		 * Memory is going be be organized as:
		 *		xxxx yyyy zzzz
		 */
		template <DataLayout TDataLayout, typename ValueType>
		struct data_view_policy_soa {
			static_assert(std::is_copy_assignable_v<ValueType>);

			using TTuple = decltype(struct_to_tuple(ValueType{}));
			using TargetCastType = uint8_t*;

			constexpr static DataLayout Layout = data_layout_properties<TDataLayout, ValueType>::Layout;
			constexpr static size_t Alignment = data_layout_properties<TDataLayout, ValueType>::Alignment;
			constexpr static size_t ArrayAlignment = data_layout_properties<TDataLayout, ValueType>::ArrayAlignment;
			constexpr static size_t TTupleItems = std::tuple_size<TTuple>::value;
			static_assert(Alignment > 0, "SoA data can't be zero-aligned");
			static_assert(sizeof(ValueType) > 0, "SoA data can't be zero-size");

			template <size_t Item>
			using value_type = typename std::tuple_element<Item, TTuple>::type;
			template <size_t Item>
			using const_value_type = typename std::add_const<value_type<Item>>::type;

			GAIA_NODISCARD constexpr static uint32_t get_min_byte_size(uintptr_t addr, size_t cnt) noexcept {
				const auto offset = get_aligned_byte_offset<TTupleItems>(addr, cnt);
				return (uint32_t)(offset - addr);
			}

			GAIA_NODISCARD static uint8_t* alloc_mem(size_t cnt) noexcept {
				const auto bytes = get_min_byte_size(0, cnt);
				return (uint8_t*)utils::mem_alloc_alig(bytes, Alignment);
			}

			static void free_mem(uint8_t* pData, [[maybe_unused]] size_t cnt) noexcept {
				if (pData == nullptr)
					return;
				return utils::mem_free_alig(pData);
			}

			GAIA_NODISCARD constexpr static ValueType get(std::span<const uint8_t> s, size_t idx) noexcept {
				auto t = struct_to_tuple(ValueType{});
				return get_internal(t, s, idx, std::make_index_sequence<TTupleItems>());
			}

			template <size_t Item>
			constexpr static auto get(std::span<const uint8_t> s, size_t idx = 0) noexcept {
				const auto offset = get_aligned_byte_offset<Item>((uintptr_t)s.data(), s.size());
				const auto& ref = get_ref<const value_type<Item>>((const uint8_t*)offset, idx);
				return std::span{&ref, s.size() - idx};
			}

			class accessor {
				std::span<uint8_t> m_data;
				size_t m_idx;

			public:
				accessor(std::span<uint8_t> data, size_t idx): m_data(data), m_idx(idx) {}

				constexpr void operator=(const ValueType& val) noexcept {
					auto t = struct_to_tuple(val);
					set_internal(t, m_data, m_idx, std::make_index_sequence<TTupleItems>());
				}

				constexpr void operator=(ValueType&& val) noexcept {
					auto t = struct_to_tuple(std::forward<ValueType>(val));
					set_internal(t, m_data, m_idx, std::make_index_sequence<TTupleItems>());
				}

				GAIA_NODISCARD constexpr operator ValueType() const noexcept {
					auto t = struct_to_tuple(ValueType{});
					return get_internal(
							t, {(const uint8_t*)m_data.data(), m_data.size()}, m_idx, std::make_index_sequence<TTupleItems>());
				}
			};

			constexpr static auto set(std::span<uint8_t> s, size_t idx) noexcept {
				return accessor(s, idx);
			}

			template <size_t Item>
			constexpr static auto set(std::span<uint8_t> s, size_t idx = 0) noexcept {
				const auto offset = get_aligned_byte_offset<Item>((uintptr_t)s.data(), s.size());
				auto& ref = get_ref<value_type<Item>>((const uint8_t*)offset, idx);
				return std::span{&ref, s.size() - idx};
			}

		private:
			template <size_t... Ids>
			constexpr static size_t
			get_aligned_byte_offset_seq(uintptr_t address, size_t cnt, std::index_sequence<Ids...> /*no_name*/) {
				// Align the address for the first type
				address = utils::align<Alignment>(address);
				// Offset and align the rest
				((address = utils::align<Alignment>(address + sizeof(value_type<Ids>) * cnt)), ...);
				return address;
			}

			template <uint32_t N>
			constexpr static size_t get_aligned_byte_offset(uintptr_t address, size_t cnt) {
				return get_aligned_byte_offset_seq(address, cnt, std::make_index_sequence<N>());
			}

			template <typename TMemberType>
			constexpr static TMemberType& get_ref(const uint8_t* data, size_t idx) noexcept {
				// Write the value directly to the memory address.
				// Usage of unaligned_ref is not necessary because the memory is aligned.
				auto* pCastData = (TMemberType*)data;
				return pCastData[idx];
			}

			template <size_t... Ids>
			GAIA_NODISCARD constexpr static ValueType get_internal(
					TTuple& t, std::span<const uint8_t> s, size_t idx, std::index_sequence<Ids...> /*no_name*/) noexcept {
				auto address = utils::align<Alignment>((uintptr_t)s.data());
				((
						 // Put the value at the address into our tuple. Data is aligned so we can read directly.
						 std::get<Ids>(t) = get_ref<value_type<Ids>>((const uint8_t*)address, idx),
						 // Skip towards the next element and make sure the address is aligned properly
						 address = utils::align<Alignment>(address + sizeof(value_type<Ids>) * s.size())),
				 ...);
				return tuple_to_struct<ValueType, TTuple>(std::forward<TTuple>(t));
			}

			template <size_t... Ids>
			constexpr static void
			set_internal(TTuple& t, std::span<uint8_t> s, size_t idx, std::index_sequence<Ids...> /*no_name*/) noexcept {
				auto address = utils::align<Alignment>((uintptr_t)s.data());
				((
						 // Set the tuple value. Data is aligned so we can write directly.
						 get_ref<value_type<Ids>>((uint8_t*)address, idx) = std::get<Ids>(t),
						 // Skip towards the next element and make sure the address is aligned properly
						 address = utils::align<Alignment>(address + sizeof(value_type<Ids>) * s.size())),
				 ...);
			}
		};

		template <typename ValueType>
		struct data_view_policy<DataLayout::SoA, ValueType>: data_view_policy_soa<DataLayout::SoA, ValueType> {};
		template <typename ValueType>
		struct data_view_policy<DataLayout::SoA8, ValueType>: data_view_policy_soa<DataLayout::SoA8, ValueType> {};
		template <typename ValueType>
		struct data_view_policy<DataLayout::SoA16, ValueType>: data_view_policy_soa<DataLayout::SoA16, ValueType> {};

		template <DataLayout TDataLayout, typename ValueType>
		struct data_view_policy_soa_get {
			static_assert(std::is_copy_assignable_v<ValueType>);

			using view_policy = data_view_policy_soa<TDataLayout, ValueType>;

			template <size_t Item>
			struct data_view_policy_idx_info {
				using const_value_type = typename view_policy::template const_value_type<Item>;
			};

			//! Raw data pointed to by the view policy
			std::span<const uint8_t> m_data;

			data_view_policy_soa_get(std::span<uint8_t> data): m_data({(const uint8_t*)data.data(), data.size()}) {}
			data_view_policy_soa_get(std::span<const uint8_t> data): m_data({(const uint8_t*)data.data(), data.size()}) {}
			template <typename C>
			data_view_policy_soa_get(const C& c): m_data({(const uint8_t*)c.data(), c.size()}) {
				static_assert(!std::is_same_v<C, data_view_policy_soa_get>);
			}

			GAIA_NODISCARD constexpr auto operator[](size_t idx) const noexcept -> decltype(view_policy::get({}, 0)) {
				return view_policy::get(m_data, idx);
			}

			template <size_t Item>
			GAIA_NODISCARD constexpr auto get() const noexcept {
				auto s = view_policy::template get<Item>(m_data);
				return std::span(s.data(), s.size());
			}

			GAIA_NODISCARD auto data() const noexcept {
				return m_data.data();
			}

			GAIA_NODISCARD auto size() const noexcept {
				return m_data.size();
			}
		};

		template <typename ValueType>
		struct data_view_policy_get<DataLayout::SoA, ValueType>: data_view_policy_soa_get<DataLayout::SoA, ValueType> {};
		template <typename ValueType>
		struct data_view_policy_get<DataLayout::SoA8, ValueType>: data_view_policy_soa_get<DataLayout::SoA8, ValueType> {};
		template <typename ValueType>
		struct data_view_policy_get<DataLayout::SoA16, ValueType>:
				data_view_policy_soa_get<DataLayout::SoA16, ValueType> {};

		template <DataLayout TDataLayout, typename ValueType>
		struct data_view_policy_soa_set {
			static_assert(std::is_copy_assignable_v<ValueType>);

			using view_policy = data_view_policy_soa<TDataLayout, ValueType>;

			template <size_t Item>
			struct data_view_policy_idx_info {
				using value_type = typename view_policy::template value_type<Item>;
				using const_value_type = typename view_policy::template const_value_type<Item>;
			};

			//! Raw data pointed to by the view policy
			std::span<uint8_t> m_data;

			data_view_policy_soa_set(std::span<uint8_t> data): m_data({(uint8_t*)data.data(), data.size()}) {}
			data_view_policy_soa_set(std::span<const uint8_t> data): m_data({(uint8_t*)data.data(), data.size()}) {}
			template <typename C>
			data_view_policy_soa_set(const C& c): m_data({(uint8_t*)c.data(), c.size()}) {
				static_assert(!std::is_same_v<C, data_view_policy_soa_set>);
			}

			struct accessor {
				std::span<uint8_t> m_data;
				size_t m_idx;

				constexpr void operator=(const ValueType& val) noexcept {
					view_policy::set(m_data, m_idx) = val;
				}
				constexpr void operator=(ValueType&& val) noexcept {
					view_policy::set(m_data, m_idx) = std::forward<ValueType>(val);
				}
			};

			GAIA_NODISCARD constexpr auto operator[](size_t idx) const noexcept -> decltype(view_policy::get({}, 0)) {
				return view_policy::get({(const uint8_t*)m_data.data(), m_data.size()}, idx);
			}
			GAIA_NODISCARD constexpr auto operator[](size_t idx) noexcept {
				return accessor{m_data, idx};
			}

			template <size_t Item>
			GAIA_NODISCARD constexpr auto get() const noexcept {
				auto s = view_policy::template get<Item>(m_data);
				return std::span(s.data(), s.size());
			}

			template <size_t Item>
			GAIA_NODISCARD constexpr auto set() noexcept {
				auto s = view_policy::template set<Item>(m_data);
				return std::span(s.data(), s.size());
			}

			GAIA_NODISCARD auto data() const noexcept {
				return m_data.data();
			}

			GAIA_NODISCARD auto size() const noexcept {
				return m_data.size();
			}
		};

		template <typename ValueType>
		struct data_view_policy_set<DataLayout::SoA, ValueType>: data_view_policy_soa_set<DataLayout::SoA, ValueType> {};
		template <typename ValueType>
		struct data_view_policy_set<DataLayout::SoA8, ValueType>: data_view_policy_soa_set<DataLayout::SoA8, ValueType> {};
		template <typename ValueType>
		struct data_view_policy_set<DataLayout::SoA16, ValueType>:
				data_view_policy_soa_set<DataLayout::SoA16, ValueType> {};

		//----------------------------------------------------------------------
		// Helpers
		//----------------------------------------------------------------------

		namespace detail {
			template <typename, typename = void>
			struct auto_view_policy_internal {
				static constexpr DataLayout data_layout_type = DataLayout::AoS;
			};
			template <typename T>
			struct auto_view_policy_internal<T, std::void_t<decltype(T::Layout)>> {
				static constexpr DataLayout data_layout_type = T::Layout;
			};

			template <typename, typename = void>
			struct is_soa_layout: std::false_type {};
			template <typename T>
			struct is_soa_layout<T, std::void_t<decltype(T::Layout)>>: std::bool_constant<(T::Layout != DataLayout::AoS)> {};
		} // namespace detail

		template <typename T>
		using auto_view_policy = data_view_policy<detail::auto_view_policy_internal<T>::data_layout_type, T>;
		template <typename T>
		using auto_view_policy_get = data_view_policy_get<detail::auto_view_policy_internal<T>::data_layout_type, T>;
		template <typename T>
		using auto_view_policy_set = data_view_policy_set<detail::auto_view_policy_internal<T>::data_layout_type, T>;

		template <typename T>
		inline constexpr bool is_soa_layout_v = detail::is_soa_layout<T>::value;

		template <typename T, uint32_t N>
		using raw_data_holder = detail::raw_data_holder<N, auto_view_policy<T>::ArrayAlignment>;

	} // namespace utils
} // namespace gaia
