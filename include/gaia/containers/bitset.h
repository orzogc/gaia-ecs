#pragma once
#include "../config/config.h"

#include <cinttypes>
#include <type_traits>

#include "../utils/iterator.h"

namespace gaia {
	namespace containers {
		template <uint32_t NBits>
		class bitset {
		public:
			static constexpr uint32_t BitCount = NBits;
			static_assert(NBits > 0);

		private:
			template <bool Use32Bit>
			struct size_type_selector {
				using type = std::conditional_t<Use32Bit, uint32_t, uint64_t>;
			};

			static constexpr uint32_t BitsPerItem = (NBits / 64) > 0 ? 64 : 32;
			static constexpr uint32_t Items = (NBits + BitsPerItem - 1) / BitsPerItem;
			using size_type = typename size_type_selector<BitsPerItem == 32>::type;
			static constexpr bool HasTrailingBits = (NBits % BitsPerItem) != 0;
			static constexpr size_type LastItemMask = ((size_type)1 << (NBits % BitsPerItem)) - 1;

			size_type m_data[Items]{};

		public:
			class const_iterator {
			public:
				using iterator_category = GAIA_UTIL::random_access_iterator_tag;
				using value_type = uint32_t;
				using size_type = bitset<NBits>::size_type;

			private:
				const bitset<NBits>& m_bitset;
				value_type m_pos;

				uint32_t find_next_set_bit(uint32_t pos) const {
					value_type wordIndex = pos / bitset::BitsPerItem;
					GAIA_ASSERT(wordIndex < Items);
					size_type word = 0;

					const size_type posInWord = pos % bitset::BitsPerItem;
					if (posInWord < bitset::BitsPerItem - 1) {
						const size_type mask = (size_type(1) << (posInWord + 1)) - 1;
						const size_type maskInv = ~mask;
						word = m_bitset.m_data[wordIndex] & maskInv;
					}

					// No set bit in the current word, move to the next one
					while (word == 0) {
						if (wordIndex >= bitset::Items - 1)
							return pos;

						word = m_bitset.m_data[++wordIndex];
					}

					// Process the word
					uint32_t fwd = 0;

					GAIA_MSVC_WARNING_PUSH()
					GAIA_MSVC_WARNING_DISABLE(4244)
					if constexpr (bitset::BitsPerItem == 32)
						fwd = GAIA_FFS(word) - 1;
					else
						fwd = GAIA_FFS64(word) - 1;
					GAIA_MSVC_WARNING_POP()

					return wordIndex * bitset::BitsPerItem + fwd;
				}

				uint32_t find_prev_set_bit(uint32_t pos) const {
					value_type wordIndex = pos / bitset::BitsPerItem;
					GAIA_ASSERT(wordIndex < Items);
					size_type word = m_bitset.m_data[wordIndex];

					// No set bit in the current word, move to the previous word
					while (word == 0) {
						if (wordIndex == 0)
							return pos;
						word = m_bitset.m_data[--wordIndex];
					}

					// Process the word
					uint32_t ctz = 0;

					GAIA_MSVC_WARNING_PUSH()
					GAIA_MSVC_WARNING_DISABLE(4244)
					if constexpr (bitset::BitsPerItem == 32)
						ctz = GAIA_CTZ(word);
					else
						ctz = GAIA_CTZ64(word);
					GAIA_MSVC_WARNING_POP()

					return bitset::BitsPerItem * (wordIndex + 1) - ctz - 1;
				}

			public:
				const_iterator(const bitset<NBits>& bitset, value_type pos, bool fwd): m_bitset(bitset), m_pos(pos) {
					if (fwd) {
						// Find the first set bit)
						if (pos != 0 || !bitset.test(0)) {
							pos = find_next_set_bit(m_pos);
							// Point beyond the last item if no set bit was found
							if (pos == m_pos)
								pos = bitset.size();
						}
						m_pos = pos;
					} else {
						const auto bitsetSize = bitset.size();
						const auto lastBit = bitsetSize - 1;

						// Stay inside bounds
						if (pos >= bitsetSize)
							pos = bitsetSize - 1;

						// Find the last set bit
						if (pos != lastBit || !bitset.test(lastBit)) {
							const uint32_t newPos = find_prev_set_bit(pos);
							// Point one beyond the last found bit
							pos = (newPos == pos) ? bitsetSize : newPos + 1;
						}
						// Point one beyond the last found bit
						else
							++pos;

						m_pos = pos;
					}
				}

				GAIA_NODISCARD value_type operator*() const {
					return m_pos;
				}

				const_iterator& operator++() {
					uint32_t newPos = find_next_set_bit(m_pos);
					// Point one past the last item if no new bit was found
					if (newPos == m_pos)
						++newPos;
					m_pos = newPos;
					return *this;
				}

				GAIA_NODISCARD const_iterator operator++(int) {
					const_iterator temp(*this);
					++*this;
					return temp;
				}

				GAIA_NODISCARD bool operator==(const const_iterator& other) const {
					return m_pos == other.m_pos;
				}

				GAIA_NODISCARD bool operator!=(const const_iterator& other) const {
					return m_pos != other.m_pos;
				}
			};

			const_iterator begin() const {
				return const_iterator(*this, 0, true);
			}

			const_iterator end() const {
				return const_iterator(*this, NBits, false);
			}

			const_iterator cbegin() const {
				return const_iterator(*this, 0, true);
			}

			const_iterator cend() const {
				return const_iterator(*this, NBits, false);
			}

			GAIA_NODISCARD constexpr bool operator[](uint32_t pos) const {
				return test(pos);
			}

			GAIA_NODISCARD constexpr bool operator==(const bitset& other) const {
				for (uint32_t i = 0; i < Items; ++i)
					if (m_data[i] != other.m_data[i])
						return false;
				return true;
			}

			GAIA_NODISCARD constexpr bool operator!=(const bitset& other) const {
				for (uint32_t i = 0; i < Items; ++i)
					if (m_data[i] == other.m_data[i])
						return false;
				return true;
			}

			//! Sets all bits
			constexpr void set() {
				if constexpr (HasTrailingBits) {
					for (uint32_t i = 0; i < Items - 1; ++i)
						m_data[i] = (size_type)-1;
					m_data[Items - 1] = LastItemMask;
				} else {
					for (uint32_t i = 0; i < Items; ++i)
						m_data[i] = (size_type)-1;
				}
			}

			//! Sets the bit at the postion \param pos to value \param value
			constexpr void set(uint32_t pos, bool value = true) {
				GAIA_ASSERT(pos < NBits);
				if (value)
					m_data[pos / BitsPerItem] |= ((size_type)1 << (pos % BitsPerItem));
				else
					m_data[pos / BitsPerItem] &= ~((size_type)1 << (pos % BitsPerItem));
			}

			//! Flips all bits
			constexpr bitset& flip() {
				if constexpr (HasTrailingBits) {
					for (uint32_t i = 0; i < Items - 1; ++i)
						m_data[i] = ~m_data[i];
					m_data[Items - 1] = (~m_data[Items - 1]) & LastItemMask;
				} else {
					for (uint32_t i = 0; i < Items; ++i)
						m_data[i] = ~m_data[i];
				}
				return *this;
			}

			//! Flips the bit at the postion \param pos
			constexpr void flip(uint32_t pos) {
				GAIA_ASSERT(pos < NBits);
				m_data[pos / BitsPerItem] ^= ((size_type)1 << (pos % BitsPerItem));
			}

			//! Flips all bits from \param bitFrom to \param bitTo (including)
			constexpr bitset& flip(uint32_t bitFrom, uint32_t bitTo) {
				GAIA_ASSERT(bitFrom <= bitTo);
				GAIA_ASSERT(bitFrom < size());

				if GAIA_UNLIKELY (size() == 0)
					return *this;

				for (uint32_t i = bitFrom; i <= bitTo; i++) {
					uint32_t wordIdx = i / BitsPerItem;
					uint32_t bitOffset = i % BitsPerItem;
					m_data[wordIdx] ^= ((size_type)1 << bitOffset);
				}

				return *this;
			}

			//! Unsets all bits
			constexpr void reset() {
				for (uint32_t i = 0; i < Items; ++i)
					m_data[i] = 0;
			}

			//! Unsets the bit at the postion \param pos
			constexpr void reset(uint32_t pos) {
				GAIA_ASSERT(pos < NBits);
				m_data[pos / BitsPerItem] &= ~((size_type)1 << (pos % BitsPerItem));
			}

			//! Returns the value of the bit at the position \param pos
			GAIA_NODISCARD constexpr bool test(uint32_t pos) const {
				GAIA_ASSERT(pos < NBits);
				return (m_data[pos / BitsPerItem] & ((size_type)1 << (pos % BitsPerItem))) != 0;
			}

			//! Checks if all bits are set
			GAIA_NODISCARD constexpr bool all() const {
				if constexpr (HasTrailingBits) {
					for (uint32_t i = 0; i < Items - 1; ++i)
						if (m_data[i] != (size_type)-1)
							return false;
					return (m_data[Items - 1] & LastItemMask) == LastItemMask;
				} else {
					for (uint32_t i = 0; i < Items; ++i)
						if (m_data[i] != (size_type)-1)
							return false;
					return true;
				}
			}

			//! Checks if any bit is set
			GAIA_NODISCARD constexpr bool any() const {
				for (uint32_t i = 0; i < Items; ++i)
					if (m_data[i] != 0)
						return true;
				return false;
			}

			//! Checks if all bits are reset
			GAIA_NODISCARD constexpr bool none() const {
				for (uint32_t i = 0; i < Items; ++i)
					if (m_data[i] != 0)
						return false;
				return true;
			}

			//! Returns the number of set bits
			GAIA_NODISCARD uint32_t count() const {
				uint32_t total = 0;

				GAIA_MSVC_WARNING_PUSH()
				GAIA_MSVC_WARNING_DISABLE(4244)
				if constexpr (sizeof(size_type) == 4) {
					for (uint32_t i = 0; i < Items; ++i)
						total += GAIA_POPCNT(m_data[i]);
				} else {
					for (uint32_t i = 0; i < Items; ++i)
						total += GAIA_POPCNT64(m_data[i]);
				}
				GAIA_MSVC_WARNING_POP()

				return total;
			}

			//! Returns the number of bits the bitset can hold
			GAIA_NODISCARD constexpr uint32_t size() const {
				return NBits;
			}
		};
	} // namespace containers
} // namespace gaia