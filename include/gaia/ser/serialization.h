#pragma once
#include "../config/config.h"

#include <type_traits>
#include <utility>

#include "../core/utility.h"
#include "../meta/reflection.h"

namespace gaia {
	namespace ser {
		namespace detail {
			enum class serialization_type_id : uint8_t {
				// Integer types
				s8 = 1,
				u8 = 2,
				s16 = 3,
				u16 = 4,
				s32 = 5,
				u32 = 6,
				s64 = 7,
				u64 = 8,

				// Boolean
				b = 40,

				// Character types
				c8 = 41,
				c16 = 42,
				c32 = 43,
				cw = 44,

				// Floating point types
				f8 = 81,
				f16 = 82,
				f32 = 83,
				f64 = 84,
				f128 = 85,

				// Special
				trivial_wrapper = 200,
				data_and_size = 201,

				Last = 255,
			};

			GAIA_DEFINE_HAS_FUNCTION(resize);
			GAIA_DEFINE_HAS_FUNCTION(bytes);
			GAIA_DEFINE_HAS_FUNCTION(save);
			GAIA_DEFINE_HAS_FUNCTION(load);

			template <typename T>
			struct is_trivially_serializable {
			private:
				static constexpr bool update() {
					return std::is_enum_v<T> || std::is_fundamental_v<T> || std::is_trivially_copyable_v<T>;
				}

			public:
				static constexpr bool value = update();
			};

			template <typename T>
			struct is_int_kind_id:
					std::disjunction<
							std::is_same<T, int8_t>, std::is_same<T, uint8_t>, //
							std::is_same<T, int16_t>, std::is_same<T, uint16_t>, //
							std::is_same<T, int32_t>, std::is_same<T, uint32_t>, //
							std::is_same<T, int64_t>, std::is_same<T, uint64_t>, //
							std::is_same<T, bool>> {};

			template <typename T>
			struct is_flt_kind_id:
					std::disjunction<
							// std::is_same<T, float8_t>, //
							// std::is_same<T, float16_t>, //
							std::is_same<T, float>, //
							std::is_same<T, double>, //
							std::is_same<T, long double>> {};

			template <typename T>
			GAIA_NODISCARD constexpr serialization_type_id int_kind_id() {
				static_assert(is_int_kind_id<T>::value, "Unsupported integral type");

				if constexpr (std::is_same_v<int8_t, T>) {
					return serialization_type_id::s8;
				} else if constexpr (std::is_same_v<uint8_t, T>) {
					return serialization_type_id::u8;
				} else if constexpr (std::is_same_v<int16_t, T>) {
					return serialization_type_id::s16;
				} else if constexpr (std::is_same_v<uint16_t, T>) {
					return serialization_type_id::u16;
				} else if constexpr (std::is_same_v<int32_t, T>) {
					return serialization_type_id::s32;
				} else if constexpr (std::is_same_v<uint32_t, T>) {
					return serialization_type_id::u32;
				} else if constexpr (std::is_same_v<int64_t, T>) {
					return serialization_type_id::s64;
				} else if constexpr (std::is_same_v<uint64_t, T>) {
					return serialization_type_id::u64;
				} else { // if constexpr (std::is_same_v<bool, T>) {
					return serialization_type_id::b;
				}
			}

			template <typename T>
			GAIA_NODISCARD constexpr serialization_type_id flt_type_id() {
				static_assert(is_flt_kind_id<T>::value, "Unsupported floating type");

				// if constexpr (std::is_same_v<float8_t, T>) {
				// 	return serialization_type_id::f8;
				// } else if constexpr (std::is_same_v<float16_t, T>) {
				// 	return serialization_type_id::f16;
				// } else
				if constexpr (std::is_same_v<float, T>) {
					return serialization_type_id::f32;
				} else if constexpr (std::is_same_v<double, T>) {
					return serialization_type_id::f64;
				} else { // if constexpr (std::is_same_v<long double, T>) {
					return serialization_type_id::f128;
				}
			}

			template <typename T>
			GAIA_NODISCARD constexpr serialization_type_id type_id() {
				if constexpr (std::is_enum_v<T>)
					return int_kind_id<std::underlying_type_t<T>>();
				else if constexpr (std::is_integral_v<T>)
					return int_kind_id<T>();
				else if constexpr (std::is_floating_point_v<T>)
					return flt_type_id<T>();
				else if constexpr (core::has_data_and_size<T>::value)
					return serialization_type_id::data_and_size;
				else if constexpr (std::is_class_v<T>)
					return serialization_type_id::trivial_wrapper;

				return serialization_type_id::Last;
			}

			template <typename T>
			GAIA_NODISCARD constexpr uint32_t bytes_one(const T& item) noexcept {
				using U = core::raw_t<T>;

				constexpr auto id = type_id<U>();
				static_assert(id != serialization_type_id::Last);
				uint32_t size_in_bytes{};

				// Custom bytes() has precedence
				if constexpr (has_bytes<U>::value) {
					size_in_bytes = (uint32_t)item.bytes();
				}
				// Trivially serializable types
				else if constexpr (is_trivially_serializable<U>::value) {
					size_in_bytes = (uint32_t)sizeof(U);
				}
				// Types which have data() and size() member functions
				else if constexpr (core::has_data_and_size<U>::value) {
					size_in_bytes = (uint32_t)item.size();
				}
				// Classes
				else if constexpr (std::is_class_v<U>) {
					meta::each_member(item, [&](auto&&... items) {
						size_in_bytes += (bytes_one(items) + ...);
					});
				} else
					static_assert(!sizeof(U), "Type is not supported for serialization, yet");

				return size_in_bytes;
			}

			template <bool Write, typename Serializer, typename T>
			void ser_data_one(Serializer& s, T&& arg) {
				using U = core::raw_t<T>;

				// Custom save() & load() have precedence
				if constexpr (Write && has_save<U, Serializer&>::value) {
					arg.save(s);
				} else if constexpr (!Write && has_load<U, Serializer&>::value) {
					arg.load(s);
				}
				// Trivially serializable types
				else if constexpr (is_trivially_serializable<U>::value) {
					if constexpr (Write)
						s.save(GAIA_FWD(arg));
					else
						s.load(GAIA_FWD(arg));
				}
				// Types which have data() and size() member functions
				else if constexpr (core::has_data_and_size<U>::value) {
					if constexpr (Write) {
						const auto size = arg.size();
						s.save(size);

						for (const auto& e: arg)
							ser_data_one<Write>(s, e);
					} else {
						auto size = arg.size();
						s.load(size);

						if constexpr (has_resize<U, size_t>::value) {
							// If resize is present, use it
							arg.resize(size);
							for (auto& e: arg)
								ser_data_one<Write>(s, e);
						} else {
							// With no resize present, write directly into memory
							GAIA_FOR(size) {
								using arg_type = typename std::remove_pointer<decltype(arg.data())>::type;
								auto& e_ref = (arg_type&)arg[i];
								ser_data_one<Write>(s, e_ref);
							}
						}
					}
				}
				// Classes
				else if constexpr (std::is_class_v<U>) {
					meta::each_member(GAIA_FWD(arg), [&s](auto&&... items) {
						// TODO: Handle contiguous blocks of trivially copyable types
						(ser_data_one<Write>(s, items), ...);
					});
				} else
					static_assert(!sizeof(U), "Type is not supported for serialization, yet");
			}
		} // namespace detail

		//! Calculates the number of bytes necessary to serialize data using the "save" function.
		//! \warning Compile-time.
		template <typename T>
		GAIA_NODISCARD uint32_t bytes(const T& data) {
			return detail::bytes_one(data);
		}

		//! Write \param data using \tparam Writer at compile-time.
		//!
		//! \warning Writer has to implement a save function as follows:
		//! 					template <typename T> void save(const T& arg);
		template <typename Writer, typename T>
		void save(Writer& writer, const T& data) {
			detail::ser_data_one<true>(writer, data);
		}

		//! Read \param data using \tparam Reader at compile-time.
		//!
		//! \warning Reader has to implement a save function as follows:
		//! 					template <typename T> void load(T& arg);
		template <typename Reader, typename T>
		void load(Reader& reader, T& data) {
			detail::ser_data_one<false>(reader, data);
		}
	} // namespace ser
} // namespace gaia