#pragma once
#include "../../config/config.h"

#include <cstddef>
#include <initializer_list>
#include <type_traits>
#include <utility>

#include "../../core/iterator.h"
#include "../../core/utility.h"
#include "../../mem/data_layout_policy.h"
#include "../../mem/mem_sani.h"
#include "../../mem/mem_utils.h"

namespace gaia {
	namespace cnt {
		namespace darr_detail {
			using difference_type = uint32_t;
			using size_type = uint32_t;
		} // namespace darr_detail

		template <typename T>
		struct darr_iterator_soa {
			using value_type = T;
			// using pointer = T*; not supported
			// using reference = T&; not supported
			using difference_type = darr_detail::difference_type;
			using size_type = darr_detail::size_type;

			using iterator = darr_iterator_soa;
			using iterator_category = core::random_access_iterator_tag;

		private:
			uint8_t* m_ptr;
			uint32_t m_cnt;
			uint32_t m_idx;

		public:
			darr_iterator_soa(uint8_t* ptr, uint32_t cnt, uint32_t idx): m_ptr(ptr), m_cnt(cnt), m_idx(idx) {}

			T operator*() const {
				return mem::data_view_policy<T::gaia_Data_Layout, T>::get({m_ptr, m_cnt}, m_idx);
			}
			T operator->() const {
				return mem::data_view_policy<T::gaia_Data_Layout, T>::get({m_ptr, m_cnt}, m_idx);
			}
			iterator operator[](size_type offset) const {
				return iterator(m_ptr, m_cnt, m_idx + offset);
			}

			iterator& operator+=(size_type diff) {
				m_idx += diff;
				return *this;
			}
			iterator& operator-=(size_type diff) {
				m_idx -= diff;
				return *this;
			}
			iterator& operator++() {
				++m_idx;
				return *this;
			}
			iterator operator++(int) {
				iterator temp(*this);
				++*this;
				return temp;
			}
			iterator& operator--() {
				--m_idx;
				return *this;
			}
			iterator operator--(int) {
				iterator temp(*this);
				--*this;
				return temp;
			}

			iterator operator+(size_type offset) const {
				return iterator(m_ptr, m_cnt, m_idx + offset);
			}
			iterator operator-(size_type offset) const {
				return iterator(m_ptr, m_cnt, m_idx + offset);
			}
			difference_type operator-(const iterator& other) const {
				GAIA_ASSERT(m_ptr == other.m_ptr);
				return (difference_type)(m_idx - other.m_idx);
			}

			GAIA_NODISCARD bool operator==(const iterator& other) const {
				GAIA_ASSERT(m_ptr == other.m_ptr);
				return m_idx == other.m_idx;
			}
			GAIA_NODISCARD bool operator!=(const iterator& other) const {
				GAIA_ASSERT(m_ptr == other.m_ptr);
				return m_idx != other.m_idx;
			}
			GAIA_NODISCARD bool operator>(const iterator& other) const {
				GAIA_ASSERT(m_ptr == other.m_ptr);
				return m_idx > other.m_idx;
			}
			GAIA_NODISCARD bool operator>=(const iterator& other) const {
				GAIA_ASSERT(m_ptr == other.m_ptr);
				return m_idx >= other.m_idx;
			}
			GAIA_NODISCARD bool operator<(const iterator& other) const {
				GAIA_ASSERT(m_ptr == other.m_ptr);
				return m_idx < other.m_idx;
			}
			GAIA_NODISCARD bool operator<=(const iterator& other) const {
				GAIA_ASSERT(m_ptr == other.m_ptr);
				return m_idx <= other.m_idx;
			}
		};

		//! Array with variable size of elements of type \tparam T allocated on heap.
		//! Interface compatiblity with std::vector where it matters.
		template <typename T, typename Allocator = mem::DefaultAllocatorAdaptor>
		class darr {
		public:
			using value_type = T;
			using reference = T&;
			using const_reference = const T&;
			using pointer = T*;
			using const_pointer = T*;
			using view_policy = mem::auto_view_policy<T>;
			using difference_type = darr_detail::difference_type;
			using size_type = darr_detail::size_type;

			using iterator = pointer;
			using iterator_soa = darr_iterator_soa<T>;
			using iterator_category = core::random_access_iterator_tag;

		private:
			uint8_t* m_pData = nullptr;
			size_type m_cnt = size_type(0);
			size_type m_cap = size_type(0);

			void try_grow() {
				const auto cnt = size();
				const auto cap = capacity();

				// Unless we reached the capacity don't do anything
				if GAIA_LIKELY (cap != 0 && cnt < cap)
					return;

				// If no data is allocated go with at least 4 elements
				if GAIA_UNLIKELY (m_pData == nullptr) {
					m_pData = view_policy::template alloc<Allocator>(m_cap = 4);
					GAIA_MEM_SANI_ADD_BLOCK(value_type, m_pData, m_cap, cnt);
					return;
				}

				// We increase the capacity in multiples of 1.5 which is about the golden ratio (1.618).
				// This effectively means we prefer more frequent allocations over memory fragmentation.
				m_cap = (cap * 3 + 1) / 2;

				auto* pDataOld = m_pData;
				m_pData = view_policy::template alloc<Allocator>(m_cap);
				GAIA_MEM_SANI_ADD_BLOCK(value_type, m_pData, m_cap, cnt);
				mem::move_elements<T>(m_pData, pDataOld, cnt, 0, m_cap, cap);
				GAIA_MEM_SANI_DEL_BLOCK(value_type, pDataOld, cap, cnt);
				view_policy::template free<Allocator>(pDataOld, cnt);
			}

		public:
			constexpr darr() noexcept = default;

			darr(size_type count, const_reference value) {
				resize(count);
				for (auto it: *this)
					*it = value;
			}

			darr(size_type count) {
				resize(count);
			}

			template <typename InputIt>
			darr(InputIt first, InputIt last) {
				const auto count = (size_type)core::distance(first, last);
				resize(count);

				if constexpr (std::is_pointer_v<InputIt>) {
					for (size_type i = 0; i < count; ++i)
						operator[](i) = first[i];
				} else if constexpr (std::is_same_v<typename InputIt::iterator_category, core::random_access_iterator_tag>) {
					for (size_type i = 0; i < count; ++i)
						operator[](i) = *(first[i]);
				} else {
					size_type i = 0;
					for (auto it = first; it != last; ++it)
						operator[](++i) = *it;
				}
			}

			darr(std::initializer_list<T> il): darr(il.begin(), il.end()) {}

			darr(const darr& other): darr(other.begin(), other.end()) {}

			darr(darr&& other) noexcept {
				// This is a newly constructed object.
				// It can't have any memory allocated, yet.
				GAIA_ASSERT(m_pData == nullptr);

				m_pData = other.m_pData;
				m_cnt = other.m_cnt;
				m_cap = other.m_cap;

				other.m_cnt = size_type(0);
				other.m_cap = size_type(0);
				other.m_pData = nullptr;
			}

			darr& operator=(std::initializer_list<T> il) {
				*this = darr(il.begin(), il.end());
				return *this;
			}

			darr& operator=(const darr& other) {
				GAIA_ASSERT(core::addressof(other) != this);

				resize(other.size());
				mem::copy_elements<T>(
						(uint8_t*)m_pData, (const uint8_t*)other.m_pData, other.size(), 0, capacity(), other.capacity());

				return *this;
			}

			darr& operator=(darr&& other) noexcept {
				GAIA_ASSERT(core::addressof(other) != this);

				// Release previously allocated memory if there was anything
				GAIA_MEM_SANI_DEL_BLOCK(value_type, m_pData, m_cap, m_cnt);
				view_policy::template free<Allocator>(m_pData, m_cnt);

				m_pData = other.m_pData;
				m_cnt = other.m_cnt;
				m_cap = other.m_cap;

				other.m_pData = nullptr;
				other.m_cnt = size_type(0);
				other.m_cap = size_type(0);

				return *this;
			}

			~darr() {
				GAIA_MEM_SANI_DEL_BLOCK(value_type, m_pData, m_cap, m_cnt);
				view_policy::template free<Allocator>(m_pData, m_cnt);
			}

			GAIA_CLANG_WARNING_PUSH()
			// Memory is aligned so we can silence this warning
			GAIA_CLANG_WARNING_DISABLE("-Wcast-align")

			GAIA_NODISCARD pointer data() noexcept {
				return (pointer)m_pData;
			}

			GAIA_NODISCARD const_pointer data() const noexcept {
				return (const_pointer)m_pData;
			}

			GAIA_NODISCARD decltype(auto) operator[](size_type pos) noexcept {
				GAIA_ASSERT(pos < size());
				return view_policy::set({(typename view_policy::TargetCastType)m_pData, capacity()}, pos);
			}

			GAIA_NODISCARD decltype(auto) operator[](size_type pos) const noexcept {
				GAIA_ASSERT(pos < size());
				return view_policy::get({(typename view_policy::TargetCastType)m_pData, capacity()}, pos);
			}

			GAIA_CLANG_WARNING_POP()

			void reserve(size_type cap) {
				if (cap <= m_cap)
					return;

				auto* pDataOld = m_pData;
				m_pData = view_policy::template alloc<Allocator>(cap);
				GAIA_MEM_SANI_ADD_BLOCK(value_type, m_pData, cap, m_cnt);

				if (pDataOld != nullptr) {
					mem::move_elements<T>(m_pData, pDataOld, m_cnt, 0, cap, m_cap);
					GAIA_MEM_SANI_DEL_BLOCK(value_type, pDataOld, m_cap, m_cnt);
					view_policy::template free<Allocator>(pDataOld, m_cnt);
				}

				m_cap = cap;
			}

			void resize(size_type count) {
				// Fresh allocation
				if (m_pData == nullptr) {
					if (count > 0) {
						m_pData = view_policy::template alloc<Allocator>(count);
						GAIA_MEM_SANI_ADD_BLOCK(value_type, m_pData, count, count);
						m_cap = count;
						m_cnt = count;
					}
					return;
				}

				// Resizing to a smaller size
				if (count <= m_cnt) {
					// Destroy elements at the end
					if constexpr (!mem::is_soa_layout_v<T>) {
						core::call_dtor_n(&data()[count], m_cnt - count);
						GAIA_MEM_SANI_POP_N(value_type, m_pData, m_cap, m_cnt - count, count);
					}

					m_cnt = count;
					return;
				}

				// Resizing to a bigger size but still within allocated capacity
				if (count <= m_cap) {
					// Construct new elements
					if constexpr (!mem::is_soa_layout_v<T>) {
						GAIA_MEM_SANI_PUSH_N(value_type, m_pData, m_cap, m_cnt, count - m_cnt);
						core::call_ctor_n(&data()[m_cnt], count - m_cnt);
					}

					m_cnt = count;
					return;
				}

				auto* pDataOld = m_pData;
				m_pData = view_policy::template alloc<Allocator>(count);
				GAIA_MEM_SANI_ADD_BLOCK(value_type, m_pData, count, count);
				{
					// Move old data to the new location
					mem::move_elements<T>(m_pData, pDataOld, m_cnt, 0, count, m_cap);
					// Default-construct new items
					if constexpr (!mem::is_soa_layout_v<T>)
						core::call_ctor_n(&data()[m_cnt], count - m_cnt);
				}

				// Release old memory
				GAIA_MEM_SANI_DEL_BLOCK(value_type, pDataOld, m_cap, m_cnt);
				view_policy::template free<Allocator>(pDataOld, m_cnt);

				m_cap = count;
				m_cnt = count;
			}

			void push_back(const T& arg) {
				try_grow();

				if constexpr (mem::is_soa_layout_v<T>) {
					operator[](m_cnt++) = arg;
				} else {
					GAIA_MEM_SANI_PUSH(value_type, m_pData, m_cap, m_cnt);
					auto* ptr = &data()[m_cnt++];
					core::call_ctor(ptr, arg);
				}
			}

			void push_back(T&& arg) {
				try_grow();

				if constexpr (mem::is_soa_layout_v<T>) {
					operator[](m_cnt++) = GAIA_MOV(arg);
				} else {
					GAIA_MEM_SANI_PUSH(value_type, m_pData, m_cap, m_cnt);
					auto* ptr = &data()[m_cnt++];
					core::call_ctor(ptr, GAIA_MOV(arg));
				}
			}

			template <typename... Args>
			decltype(auto) emplace_back(Args&&... args) {
				try_grow();

				if constexpr (mem::is_soa_layout_v<T>) {
					operator[](m_cnt++) = T(GAIA_FWD(args)...);
					return;
				} else {
					GAIA_MEM_SANI_PUSH(value_type, m_pData, m_cap, m_cnt);
					auto* ptr = &data()[m_cnt++];
					core::call_ctor(ptr, GAIA_FWD(args)...);
					return (reference)*ptr;
				}
			}

			void pop_back() noexcept {
				GAIA_ASSERT(!empty());

				if constexpr (!mem::is_soa_layout_v<T>) {
					auto* ptr = &data()[m_cnt];
					core::call_dtor(ptr);
					GAIA_MEM_SANI_POP(value_type, m_pData, m_cap, m_cnt - 1);
				}

				--m_cnt;
			}

			//! Insert the element to the position given by iterator \param pos
			iterator insert(iterator pos, const T& arg) noexcept {
				GAIA_ASSERT(pos >= data());
				GAIA_ASSERT(empty() || (pos < iterator(data() + size())));

				try_grow();

				const auto idxSrc = (size_type)core::distance(begin(), pos);
				const auto idxDst = (size_type)core::distance(begin(), end()) + 1;

				mem::shift_elements_right<T>(m_pData, idxDst, idxSrc, m_cap);

				if constexpr (mem::is_soa_layout_v<T>) {
					operator[](idxSrc) = arg;
				} else {
					auto* ptr = &data()[m_cnt];
					core::call_ctor(ptr, arg);
					GAIA_MEM_SANI_PUSH(value_type, m_pData, m_cap, m_cnt);
				}

				++m_cnt;

				return iterator(&data()[idxSrc]);
			}

			//! Insert the element to the position given by iterator \param pos
			iterator insert(iterator pos, T&& arg) noexcept {
				GAIA_ASSERT(pos >= data());
				GAIA_ASSERT(empty() || (pos < iterator(data() + size())));

				try_grow();

				const auto idxSrc = (size_type)core::distance(begin(), pos);
				const auto idxDst = (size_type)core::distance(begin(), end());

				mem::shift_elements_right<T>(m_pData, idxDst, idxSrc, m_cap);

				if constexpr (mem::is_soa_layout_v<T>) {
					operator[](idxSrc) = GAIA_MOV(arg);
				} else {
					auto* ptr = &data()[idxSrc];
					core::call_ctor(ptr, GAIA_MOV(arg));
					GAIA_MEM_SANI_PUSH(value_type, m_pData, m_cap, m_cnt);
				}

				++m_cnt;

				return iterator(&data()[idxSrc]);
			}

			//! Removes the element at pos
			//! \param pos Iterator to the element to remove
			iterator erase(iterator pos) noexcept {
				GAIA_ASSERT(pos >= data());
				GAIA_ASSERT(empty() || (pos < iterator(data() + size())));

				if (empty())
					return end();

				const auto idxSrc = (size_type)core::distance(begin(), pos);
				const auto idxDst = (size_type)core::distance(begin(), end()) - 1;

				mem::shift_elements_left<T>(m_pData, idxDst, idxSrc, m_cap);
				// Destroy if it's the last element
				if constexpr (!mem::is_soa_layout_v<T>) {
					auto* ptr = &data()[m_cnt - 1];
					core::call_dtor(ptr);
					GAIA_MEM_SANI_POP(value_type, m_pData, m_cap, m_cnt - 1);
				}

				--m_cnt;

				return iterator(&data()[idxSrc]);
			}

			//! Removes the elements in the range [first, last)
			//! \param first Iterator to the element to remove
			//! \param last Iterator to the one beyond the last element to remove
			iterator erase(iterator first, iterator last) noexcept {
				GAIA_ASSERT(first >= data())
				GAIA_ASSERT(empty() || (first < iterator(data() + size())));
				GAIA_ASSERT(last > first);
				GAIA_ASSERT(last <= iterator(data() + size()));

				if (empty())
					return end();

				const auto idxSrc = (size_type)core::distance(begin(), first);
				const auto idxDst = size();
				const auto cnt = (size_type)(last - first);

				mem::shift_elements_left_n<T>(m_pData, idxDst, idxSrc, cnt, m_cap);
				// Destroy if it's the last element
				if constexpr (!mem::is_soa_layout_v<T>) {
					auto* ptr = &data()[m_cnt - cnt];
					core::call_dtor_n(ptr, cnt);
					GAIA_MEM_SANI_POP_N(value_type, data(), m_cap, m_cnt - cnt, cnt);
				}

				m_cnt -= cnt;

				return iterator(&data()[idxSrc]);
			}

			void clear() {
				resize(0);
			}

			void shrink_to_fit() {
				const auto cap = capacity();
				const auto cnt = size();

				if (cap == cnt)
					return;

				auto* pDataOld = m_pData;
				m_pData = view_policy::mem_alloc(m_cap = cnt);
				GAIA_MEM_SANI_ADD_BLOCK(value_type, m_pData, m_cap, m_cnt);
				mem::move_elements<T>(m_pData, pDataOld, cnt, 0);
				GAIA_MEM_SANI_DEL_BLOCK(value_type, pDataOld, cap, cnt);
				view_policy::mem_free(pDataOld);
			}

			//! Removes all elements that fail the predicate.
			//! \param func A lambda or a functor with the bool operator()(Container::value_type&) overload.
			//! \return The new size of the array.
			template <typename Func>
			auto retain(Func&& func) {
				size_type erased = 0;
				size_type idxDst = 0;
				size_type idxSrc = 0;

				while (idxSrc < m_cnt) {
					if (func(operator[](idxSrc))) {
						if (idxDst < idxSrc) {
							mem::move_element<T>(m_pData, m_pData, idxDst, idxSrc, m_cap, m_cap);
							auto* ptr = &data()[idxSrc];
							core::call_dtor(ptr);
						}
						++idxDst;
					} else {
						auto* ptr = &data()[idxSrc];
						core::call_dtor(ptr);
						++erased;
					}

					++idxSrc;
				}

				if constexpr (!mem::is_soa_layout_v<T>) {
					if (erased > 0)
						GAIA_MEM_SANI_POP_N(value_type, data(), m_cap, m_cnt - erased, erased);
				}

				m_cnt -= erased;
				return idxDst;
			}

			GAIA_NODISCARD size_type size() const noexcept {
				return m_cnt;
			}

			GAIA_NODISCARD bool empty() const noexcept {
				return size() == 0;
			}

			GAIA_NODISCARD size_type capacity() const noexcept {
				return m_cap;
			}

			GAIA_NODISCARD size_type max_size() const noexcept {
				return static_cast<size_type>(-1);
			}

			GAIA_NODISCARD decltype(auto) front() noexcept {
				GAIA_ASSERT(!empty());
				if constexpr (mem::is_soa_layout_v<T>)
					return *begin();
				else
					return (reference)*begin();
			}

			GAIA_NODISCARD decltype(auto) front() const noexcept {
				GAIA_ASSERT(!empty());
				if constexpr (mem::is_soa_layout_v<T>)
					return *begin();
				else
					return (const_reference)*begin();
			}

			GAIA_NODISCARD decltype(auto) back() noexcept {
				GAIA_ASSERT(!empty());
				if constexpr (mem::is_soa_layout_v<T>)
					return operator[](m_cnt - 1);
				else
					return (reference)(operator[](m_cnt - 1));
			}

			GAIA_NODISCARD decltype(auto) back() const noexcept {
				GAIA_ASSERT(!empty());
				if constexpr (mem::is_soa_layout_v<T>)
					return operator[](m_cnt - 1);
				else
					return (const_reference) operator[](m_cnt - 1);
			}

			GAIA_NODISCARD auto begin() const noexcept {
				if constexpr (mem::is_soa_layout_v<T>)
					return iterator_soa(m_pData, size(), 0);
				else
					return iterator(data());
			}

			GAIA_NODISCARD auto rbegin() const noexcept {
				if constexpr (mem::is_soa_layout_v<T>)
					return iterator_soa(m_pData, size(), size() - 1);
				else
					return iterator((pointer)&back());
			}

			GAIA_NODISCARD auto end() const noexcept {
				if constexpr (mem::is_soa_layout_v<T>)
					return iterator_soa(m_pData, size(), size());
				else
					return iterator(data() + size());
			}

			GAIA_NODISCARD auto rend() const noexcept {
				if constexpr (mem::is_soa_layout_v<T>)
					return iterator_soa(m_pData, size(), -1);
				else
					return iterator(data() - 1);
			}

			GAIA_NODISCARD bool operator==(const darr& other) const {
				if (m_cnt != other.m_cnt)
					return false;
				const size_type n = size();
				for (size_type i = 0; i < n; ++i)
					if (!(operator[](i) == other[i]))
						return false;
				return true;
			}

			GAIA_NODISCARD constexpr bool operator!=(const darr& other) const {
				return !operator==(other);
			}

			template <size_t Item>
			auto soa_view_mut() noexcept {
				return mem::data_view_policy<T::gaia_Data_Layout, T>::template get<Item>(
						std::span<uint8_t>{(uint8_t*)m_pData, capacity()});
			}

			template <size_t Item>
			auto soa_view() const noexcept {
				return mem::data_view_policy<T::gaia_Data_Layout, T>::template get<Item>(
						std::span<const uint8_t>{(const uint8_t*)m_pData, capacity()});
			}
		};
	} // namespace cnt

} // namespace gaia