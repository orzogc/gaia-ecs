#pragma once
#include "../config/config.h"

#include <cstdint>
#include <type_traits>

#include "../core/utility.h"
#include "darray.h"

namespace gaia {
	namespace cnt {
		struct ilist_item_base {};

		struct ilist_item: public ilist_item_base {
			//! Allocated items: Index in the list.
			//! Deleted items: Index of the next deleted item in the list.
			uint32_t idx;
			//! Generation ID
			uint32_t gen;

			ilist_item() = default;
			ilist_item(uint32_t index, uint32_t generation): idx(index), gen(generation) {}

			ilist_item(const ilist_item& other) {
				idx = other.idx;
				gen = other.gen;
			}
			ilist_item& operator=(const ilist_item& other) {
				GAIA_ASSERT(core::addressof(other) != this);
				idx = other.idx;
				gen = other.gen;
				return *this;
			}

			ilist_item(ilist_item&& other) {
				idx = other.idx;
				gen = other.gen;

				other.idx = (uint32_t)-1;
				other.gen = (uint32_t)-1;
			}
			ilist_item& operator=(ilist_item&& other) {
				GAIA_ASSERT(core::addressof(other) != this);
				idx = other.idx;
				gen = other.gen;

				other.idx = (uint32_t)-1;
				other.gen = (uint32_t)-1;
				return *this;
			}
		};

		//! Implicit list. Rather than with pointers, items \tparam TListItem are linked
		//! together through an internal indexing mechanism. To the outside world they are
		//! presented as \tparam TItemHandle. All items are stored in a container instance
		//! of the type \tparam TInternalStorage.
		//! \tparam TListItem needs to have idx and gen variables and expose a constructor
		//! that initializes them.
		template <typename TListItem, typename TItemHandle, typename TInternalStorage = cnt::darray<TListItem>>
		struct ilist {
			using internal_storage = TInternalStorage;
			// TODO: replace this iterator with a real list iterator
			using iterator = typename internal_storage::iterator;

			using iterator_category = typename internal_storage::iterator::iterator_category;
			using value_type = TListItem;
			using reference = TListItem&;
			using const_reference = const TListItem&;
			using pointer = TListItem*;
			using const_pointer = TListItem*;
			using difference_type = typename internal_storage::difference_type;
			using size_type = typename internal_storage::size_type;

			static_assert(std::is_base_of<ilist_item_base, TListItem>::value);
			//! Implicit list items
			internal_storage m_items;
			//! Index of the next item to recycle
			size_type m_nextFreeIdx = (size_type)-1;
			//! Number of items to recycle
			size_type m_freeItems = 0;

			GAIA_NODISCARD pointer data() noexcept {
				return (pointer)m_items.data();
			}

			GAIA_NODISCARD const_pointer data() const noexcept {
				return (const_pointer)m_items.data();
			}

			GAIA_NODISCARD reference operator[](size_type index) {
				return m_items[index];
			}
			GAIA_NODISCARD const_reference operator[](size_type index) const {
				return m_items[index];
			}

			void clear() {
				m_items.clear();
				m_nextFreeIdx = (size_type)-1;
				m_freeItems = 0;
			}

			GAIA_NODISCARD size_type get_next_free_item() const noexcept {
				return m_nextFreeIdx;
			}

			GAIA_NODISCARD size_type get_free_items() const noexcept {
				return m_freeItems;
			}

			GAIA_NODISCARD size_type item_count() const noexcept {
				return size() - m_freeItems;
			}

			GAIA_NODISCARD size_type size() const noexcept {
				return (size_type)m_items.size();
			}

			GAIA_NODISCARD bool empty() const noexcept {
				return size() == 0;
			}

			GAIA_NODISCARD size_type capacity() const noexcept {
				return (size_type)m_items.capacity();
			}

			GAIA_NODISCARD iterator begin() const noexcept {
				return {(pointer)m_items.data()};
			}

			GAIA_NODISCARD iterator end() const noexcept {
				return {(pointer)m_items.data() + size()};
			}

			void reserve(size_type cap) {
				m_items.reserve(cap);
			}

			//! Allocates a new item in the list
			//! \return Handle to the new item
			GAIA_NODISCARD TItemHandle alloc(void* ctx) {
				if GAIA_UNLIKELY (m_freeItems == 0U) {
					// We don't want to go out of range for new item
					const auto itemCnt = (size_type)m_items.size();
					GAIA_ASSERT(itemCnt < TItemHandle::IdMask && "Trying to allocate too many items!");

					GAIA_GCC_WARNING_PUSH()
					GAIA_CLANG_WARNING_PUSH()
					GAIA_GCC_WARNING_DISABLE("-Wstringop-overflow");
					GAIA_GCC_WARNING_DISABLE("-Wmissing-field-initializers");
					GAIA_CLANG_WARNING_DISABLE("-Wmissing-field-initializers");
					m_items.push_back(TListItem::create(itemCnt, 0U, ctx));
					return TListItem::handle(m_items.back());
					GAIA_GCC_WARNING_POP()
					GAIA_CLANG_WARNING_POP()
				}

				// Make sure the list is not broken
				GAIA_ASSERT(m_nextFreeIdx < (size_type)m_items.size() && "Item recycle list broken!");

				--m_freeItems;
				const auto index = m_nextFreeIdx;
				auto& j = m_items[m_nextFreeIdx];
				m_nextFreeIdx = j.idx;
				j = TListItem::create(index, j.gen, ctx);
				return TListItem::handle(j);
			}

			//! Allocates a new item in the list
			//! \return Handle to the new item
			GAIA_NODISCARD TItemHandle alloc() {
				if GAIA_UNLIKELY (m_freeItems == 0U) {
					// We don't want to go out of range for new item
					const auto itemCnt = (size_type)m_items.size();
					GAIA_ASSERT(itemCnt < TItemHandle::IdMask && "Trying to allocate too many items!");

					GAIA_GCC_WARNING_PUSH()
					GAIA_CLANG_WARNING_PUSH()
					GAIA_GCC_WARNING_DISABLE("-Wstringop-overflow");
					GAIA_GCC_WARNING_DISABLE("-Wmissing-field-initializers");
					GAIA_CLANG_WARNING_DISABLE("-Wmissing-field-initializers");
					m_items.push_back(TListItem(itemCnt, 0U));
					return {itemCnt, 0U};
					GAIA_GCC_WARNING_POP()
					GAIA_CLANG_WARNING_POP()
				}

				// Make sure the list is not broken
				GAIA_ASSERT(m_nextFreeIdx < (size_type)m_items.size() && "Item recycle list broken!");

				--m_freeItems;
				const auto index = m_nextFreeIdx;
				auto& j = m_items[m_nextFreeIdx];
				m_nextFreeIdx = j.idx;
				return {index, m_items[index].gen};
			}

			//! Invalidates \param handle.
			//! Every time an item is deallocated its generation is increased by one.
			TListItem& free(TItemHandle handle) {
				auto& item = m_items[handle.id()];

				// Update our implicit list
				if GAIA_UNLIKELY (m_freeItems == 0)
					item.idx = TItemHandle::IdMask;
				else
					item.idx = m_nextFreeIdx;
				++item.gen;

				m_nextFreeIdx = handle.id();
				++m_freeItems;

				return item;
			}

			//! Verifies that the implicit linked list is valid
			void validate() const {
				bool hasThingsToRemove = m_freeItems > 0;
				if (!hasThingsToRemove)
					return;

				// If there's something to remove there has to be at least one entity left
				GAIA_ASSERT(!m_items.empty());

				auto freeItems = m_freeItems;
				auto nextFreeItem = m_nextFreeIdx;
				while (freeItems > 0) {
					GAIA_ASSERT(nextFreeItem < m_items.size() && "Item recycle list broken!");

					nextFreeItem = m_items[nextFreeItem].idx;
					--freeItems;
				}

				// At this point the index of the last item in list should
				// point to -1 because that's the tail of our implicit list.
				GAIA_ASSERT(nextFreeItem == TItemHandle::IdMask);
			}
		};
	} // namespace cnt
} // namespace gaia