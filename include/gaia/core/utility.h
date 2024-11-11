#pragma once
#include "../config/config.h"

#include <cstdio>
#include <tuple>
#include <type_traits>
#include <utility>

#include "iterator.h"
#include "span.h"

namespace gaia {
	constexpr uint32_t BadIndex = uint32_t(-1);

#if GAIA_COMPILER_MSVC || GAIA_PLATFORM_WINDOWS
	#define GAIA_STRCPY(var, max_len, text)                                                                              \
		strncpy_s((var), (text), (size_t)-1);                                                                              \
		(void)max_len
	#define GAIA_STRFMT(var, max_len, fmt, ...) sprintf_s((var), (max_len), fmt, __VA_ARGS__)
#else
	#define GAIA_STRCPY(var, max_len, text)                                                                              \
		{                                                                                                                  \
			strncpy((var), (text), (max_len));                                                                               \
			(var)[(max_len) - 1] = 0;                                                                                        \
		}
	#define GAIA_STRFMT(var, max_len, fmt, ...) snprintf((var), (max_len), fmt, __VA_ARGS__)
#endif

	namespace core {
		namespace detail {
			template <class T>
			struct rem_rp {
				using type = std::remove_reference_t<std::remove_pointer_t<T>>;
			};

			template <typename T>
			struct is_mut:
					std::bool_constant<
							!std::is_const_v<typename rem_rp<T>::type> &&
							(std::is_pointer<T>::value || std::is_reference<T>::value)> {};
		} // namespace detail

		template <class T>
		using rem_rp_t = typename detail::rem_rp<T>::type;

		template <typename T>
		inline constexpr bool is_mut_v = detail::is_mut<T>::value;

		template <class T>
		using raw_t = typename std::decay_t<std::remove_pointer_t<T>>;

		template <typename T>
		inline constexpr bool is_raw_v = std::is_same_v<T, raw_t<T>> && !std::is_array_v<T>;

		//! Obtains the actual address of the object \param obj or function arg, even in presence of overloaded operator&.
		template <typename T>
		constexpr T* addressof(T& obj) noexcept {
			return &obj;
		}

		//! Rvalue overload is deleted to prevent taking the address of const rvalues.
		template <class T>
		const T* addressof(const T&&) = delete;

		//----------------------------------------------------------------------
		// Bit-byte conversion
		//----------------------------------------------------------------------

		template <typename T>
		constexpr T as_bits(T value) {
			static_assert(std::is_integral_v<T>);
			return value * 8;
		}

		template <typename T>
		constexpr T as_bytes(T value) {
			static_assert(std::is_integral_v<T>);
			return value / 8;
		}

		//----------------------------------------------------------------------
		// Memory size helpers
		//----------------------------------------------------------------------

		template <typename T>
		constexpr uint32_t count_bits(T number) {
			uint32_t bits_needed = 0;
			while (number > 0) {
				number >>= 1;
				++bits_needed;
			}
			return bits_needed;
		}

		//----------------------------------------------------------------------
		// Element construction / destruction
		//----------------------------------------------------------------------

		//! Constructs an object of type \tparam T in the uninitialize storage at the memory address \param pData.
		template <typename T>
		void call_ctor_raw(T* pData) {
			GAIA_ASSERT(pData != nullptr);
			(void)::new (const_cast<void*>(static_cast<const volatile void*>(core::addressof(*pData)))) T;
		}

		//! Constructs \param cnt objects of type \tparam T in the uninitialize storage at the memory address \param pData.
		template <typename T>
		void call_ctor_raw_n(T* pData, size_t cnt) {
			GAIA_ASSERT(pData != nullptr);
			for (size_t i = 0; i < cnt; ++i) {
				auto* ptr = pData + i;
				(void)::new (const_cast<void*>(static_cast<const volatile void*>(core::addressof(*ptr)))) T;
			}
		}

		//! Value-constructs an object of type \tparam T in the uninitialize storage at the memory address \param pData.
		template <typename T>
		void call_ctor_val(T* pData) {
			GAIA_ASSERT(pData != nullptr);
			(void)::new (const_cast<void*>(static_cast<const volatile void*>(core::addressof(*pData)))) T();
		}

		//! Value-constructs \param cnt objects of type \tparam T in the uninitialize storage at the memory address \param
		//! pData.
		template <typename T>
		void call_ctor_val_n(T* pData, size_t cnt) {
			GAIA_ASSERT(pData != nullptr);
			for (size_t i = 0; i < cnt; ++i) {
				auto* ptr = pData + i;
				(void)::new (const_cast<void*>(static_cast<const volatile void*>(core::addressof(*ptr)))) T();
			}
		}

		//! Constructs an object of type \tparam T in at the memory address \param pData.
		template <typename T>
		void call_ctor(T* pData) {
			GAIA_ASSERT(pData != nullptr);
			if constexpr (!std::is_trivially_constructible_v<T>) {
				(void)::new (pData) T();
			}
		}

		//! Constructs \param cnt objects of type \tparam T starting at the memory address \param pData.
		template <typename T>
		void call_ctor_n(T* pData, size_t cnt) {
			GAIA_ASSERT(pData != nullptr);
			if constexpr (!std::is_trivially_constructible_v<T>) {
				for (size_t i = 0; i < cnt; ++i)
					(void)::new (pData + i) T();
			}
		}

		template <typename T, typename... Args>
		void call_ctor(T* pData, Args&&... args) {
			GAIA_ASSERT(pData != nullptr);
			if constexpr (std::is_constructible_v<T, Args...>)
				(void)::new (pData) T(GAIA_FWD(args)...);
			else
				(void)::new (pData) T{GAIA_FWD(args)...};
		}

		//! Constructs an object of type \tparam T at the memory address \param pData.
		template <typename T>
		void call_dtor(T* pData) {
			GAIA_ASSERT(pData != nullptr);
			if constexpr (!std::is_trivially_destructible_v<T>) {
				pData->~T();
			}
		}

		//! Constructs \param cnt objects of type \tparam T starting at the memory address \param pData.
		template <typename T>
		void call_dtor_n(T* pData, size_t cnt) {
			GAIA_ASSERT(pData != nullptr);
			if constexpr (!std::is_trivially_destructible_v<T>) {
				for (size_t i = 0; i < cnt; ++i)
					pData[i].~T();
			}
		}

		//----------------------------------------------------------------------
		// Element swapping
		//----------------------------------------------------------------------

		template <typename T>
		constexpr void swap(T& left, T& right) {
			T tmp = GAIA_MOV(left);
			left = GAIA_MOV(right);
			right = GAIA_MOV(tmp);
		}

		template <typename T, typename TCmpFunc>
		constexpr void swap_if(T& lhs, T& rhs, TCmpFunc cmpFunc) noexcept {
			if (!cmpFunc(lhs, rhs))
				core::swap(lhs, rhs);
		}

		template <typename T, typename TCmpFunc>
		constexpr void swap_if_not(T& lhs, T& rhs, TCmpFunc cmpFunc) noexcept {
			if (cmpFunc(lhs, rhs))
				core::swap(lhs, rhs);
		}

		template <typename C, typename TCmpFunc, typename TSortFunc>
		constexpr void try_swap_if(
				C& c, typename C::size_type lhs, typename C::size_type rhs, TCmpFunc cmpFunc, TSortFunc sortFunc) noexcept {
			if (!cmpFunc(c[lhs], c[rhs]))
				sortFunc(lhs, rhs);
		}

		template <typename C, typename TCmpFunc, typename TSortFunc>
		constexpr void try_swap_if_not(
				C& c, typename C::size_type lhs, typename C::size_type rhs, TCmpFunc cmpFunc, TSortFunc sortFunc) noexcept {
			if (cmpFunc(c[lhs], c[rhs]))
				sortFunc(lhs, rhs);
		}

		//----------------------------------------------------------------------
		// Value filling
		//----------------------------------------------------------------------

		template <class ForwardIt, class T>
		constexpr void fill(ForwardIt first, ForwardIt last, const T& value) {
			for (; first != last; ++first) {
				*first = value;
			}
		}

		//----------------------------------------------------------------------
		// Value range checking
		//----------------------------------------------------------------------

		template <class T>
		constexpr const T& get_min(const T& a, const T& b) {
			return (b < a) ? b : a;
		}

		template <class T>
		constexpr const T& get_max(const T& a, const T& b) {
			return (b > a) ? b : a;
		}

		//----------------------------------------------------------------------
		// Checking if a template arg is unique among the rest
		//----------------------------------------------------------------------

		template <typename...>
		inline constexpr auto is_unique = std::true_type{};

		template <typename T, typename... Rest>
		inline constexpr auto is_unique<T, Rest...> =
				std::bool_constant<(!std::is_same_v<T, Rest> && ...) && is_unique<Rest...>>{};

		namespace detail {
			template <typename T>
			struct type_identity {
				using type = T;
			};
		} // namespace detail

		template <typename T, typename... Ts>
		struct unique: detail::type_identity<T> {}; // TODO: In C++20 we could use std::type_identity

		template <typename... Ts, typename U, typename... Us>
		struct unique<std::tuple<Ts...>, U, Us...>:
				std::conditional_t<
						(std::is_same_v<U, Ts> || ...), unique<std::tuple<Ts...>, Us...>, unique<std::tuple<Ts..., U>, Us...>> {};

		template <typename... Ts>
		using unique_tuple = typename unique<std::tuple<>, Ts...>::type;

		//----------------------------------------------------------------------
		// Calculating total size of all types of tuple
		//----------------------------------------------------------------------

		template <typename... Args>
		constexpr unsigned get_args_size(std::tuple<Args...> const& /*no_name*/) {
			return (sizeof(Args) + ...);
		}

		//----------------------------------------------------------------------
		// Member function checks
		//----------------------------------------------------------------------

		template <typename... Type>
		struct func_type_list {};

		template <typename Class, typename Ret, typename... Args>
		func_type_list<Args...> func_args(Ret (Class::*)(Args...) const);

		namespace detail {
			template <class Default, class AlwaysVoid, template <class...> class Op, class... Args>
			struct member_func_checker {
				using value_t = std::false_type;
				using type = Default;
			};

			template <class Default, template <class...> class Op, class... Args>
			struct member_func_checker<Default, std::void_t<Op<Args...>>, Op, Args...> {
				using value_t = std::true_type;
				using type = Op<Args...>;
			};

			struct member_func_none {
				~member_func_none() = delete;
				member_func_none(member_func_none const&) = delete;
				void operator=(member_func_none const&) = delete;
			};
		} // namespace detail

		template <template <class...> class Op, typename... Args>
		using has_member_func = typename detail::member_func_checker<detail::member_func_none, void, Op, Args...>::value_t;

		template <typename Func, typename... Args>
		struct has_global_func {
			template <typename F, typename... A>
			static auto test(F&& f, A&&... args) -> decltype(GAIA_FWD(f)(GAIA_FWD(args)...), std::true_type{});

			template <typename...>
			static std::false_type test(...);

			static constexpr bool value = decltype(test(std::declval<Func>(), std::declval<Args>()...))::value;
		};

#define GAIA_DEFINE_HAS_FUNCTION(function_name)                                                                        \
	template <typename T, typename... Args>                                                                              \
	using has_##function_name##_check = decltype(std::declval<T>().function_name(std::declval<Args>()...));              \
                                                                                                                       \
	template <typename T, typename... Args>                                                                              \
	struct has_##function_name {                                                                                         \
		static constexpr bool value = gaia::core::has_member_func<has_##function_name##_check, T, Args...>::value;         \
	};

#define GAIA_HAS_MEMBER_FUNC(function_name, T, ...)                                                                    \
	gaia::core::has_member_func<has_##function_name##_check, T, __VA_ARGS__>::value

		// TODO: Try replacing the above with following:
		//
		//       #define GAIA_HAS_MEMBER_FUNC(function_name, T, ...)
		//         gaia::core::has_member_func<
		//           decltype(std::declval<T>().function_name(std::declval<Args>()...)),
		//					 T, __VA_ARGS__
		//         >::value
		//
		//       This way we could drop GAIA_DEFINE_HAS. However, the issue is that std::declval<Args>
		//       would have to be replaced with a variadic macro that expands into a series
		//       of std::declval<Arg> which is very inconvenient to do and always has a hard limit
		//       on the number of arguments which is super limiting.

#define GAIA_HAS_GLOBAL_FUNC(function_name, ...)                                                                       \
	gaia::core::has_global_func<decltype(&function_name), __VA_ARGS__>::value

		GAIA_DEFINE_HAS_FUNCTION(find)
		GAIA_DEFINE_HAS_FUNCTION(find_if)
		GAIA_DEFINE_HAS_FUNCTION(find_if_not)

		namespace detail {
			template <typename T>
			constexpr auto has_member_equals_check(int)
					-> decltype(std::declval<T>().operator==(std::declval<T>()), std::true_type{});
			template <typename T, typename... Args>
			constexpr std::false_type has_member_equals_check(...);

			template <typename T>
			constexpr auto has_global_equals_check(int)
					-> decltype(operator==(std::declval<T>(), std::declval<T>()), std::true_type{});
			template <typename T, typename... Args>
			constexpr std::false_type has_global_equals_check(...);
		} // namespace detail

		template <typename T>
		struct has_member_equals {
			static constexpr bool value = decltype(detail::has_member_equals_check<T>(0))::value;
		};

		template <typename T>
		struct has_global_equals {
			static constexpr bool value = decltype(detail::has_global_equals_check<T>(0))::value;
		};

		//----------------------------------------------------------------------
		// Type helpers
		//----------------------------------------------------------------------

		template <typename... Type>
		struct type_list {
			using types = type_list;
			static constexpr auto size = sizeof...(Type);
		};

		template <typename TypesA, typename TypesB>
		struct type_list_concat;

		template <typename... TypesA, typename... TypesB>
		struct type_list_concat<type_list<TypesA...>, type_list<TypesB...>> {
			using type = type_list<TypesA..., TypesB...>;
		};

		//----------------------------------------------------------------------
		// Looping
		//----------------------------------------------------------------------

		namespace detail {
			template <auto FirstIdx, auto Iters, typename Func, auto... Is>
			constexpr void each_impl(Func func, std::integer_sequence<decltype(Iters), Is...> /*no_name*/) {
				if constexpr ((std::is_invocable_v<Func&&, std::integral_constant<decltype(Is), Is>> && ...))
					(func(std::integral_constant<decltype(Is), FirstIdx + Is>{}), ...);
				else
					(((void)Is, func()), ...);
			}

			template <auto FirstIdx, typename Tuple, typename Func, auto... Is>
			void each_tuple_impl(Func func, std::integer_sequence<decltype(FirstIdx), Is...> /*no_name*/) {
				if constexpr ((std::is_invocable_v<
													 Func&&, decltype(std::tuple_element_t<FirstIdx + Is, Tuple>{}),
													 std::integral_constant<decltype(FirstIdx), Is>> &&
											 ...))
					// func(Args&& arg, uint32_t idx)
					(func(
							 std::tuple_element_t<FirstIdx + Is, Tuple>{},
							 std::integral_constant<decltype(FirstIdx), FirstIdx + Is>{}),
					 ...);
				else
					// func(Args&& arg)
					(func(std::tuple_element_t<FirstIdx + Is, Tuple>{}), ...);
			}

			template <auto FirstIdx, typename Tuple, typename Func, auto... Is>
			void each_tuple_impl(Tuple&& tuple, Func func, std::integer_sequence<decltype(FirstIdx), Is...> /*no_name*/) {
				if constexpr ((std::is_invocable_v<
													 Func&&, decltype(std::get<FirstIdx + Is>(tuple)),
													 std::integral_constant<decltype(FirstIdx), Is>> &&
											 ...))
					// func(Args&& arg, uint32_t idx)
					(func(std::get<FirstIdx + Is>(tuple), std::integral_constant<decltype(FirstIdx), FirstIdx + Is>{}), ...);
				else
					// func(Args&& arg)
					(func(std::get<FirstIdx + Is>(tuple)), ...);
			}
		} // namespace detail

		//! Compile-time for loop. Performs \tparam Iters iterations.
		//!
		//! Example 1 (index argument):
		//! sarray<int, 10> arr = { ... };
		//! each<arr.size()>([&arr](auto i) {
		//!    GAIA_LOG_N("%d", i);
		//! });
		//!
		//! Example 2 (no index argument):
		//! uint32_t cnt = 0;
		//! each<10>([&cnt]() {
		//!    GAIA_LOG_N("Invocation number: %u", cnt++);
		//! });
		template <auto Iters, typename Func>
		constexpr void each(Func func) {
			using TIters = decltype(Iters);
			constexpr TIters First = 0;
			detail::each_impl<First, Iters, Func>(func, std::make_integer_sequence<TIters, Iters>());
		}

		//! Compile-time for loop with adjustable range.
		//! Iteration starts at \tparam FirstIdx and ends at \tparam LastIdx (excluding).
		//!
		//! Example 1 (index argument):
		//! sarray<int, 10> arr;
		//! each_ext<0, 10>([&arr](auto i) {
		//!    GAIA_LOG_N("%d", i);
		//! });
		//!
		//! Example 2 (no argument):
		//! uint32_t cnt = 0;
		//! each_ext<0, 10>([&cnt]() {
		//!    GAIA_LOG_N("Invocation number: %u", cnt++);
		//! });
		template <auto FirstIdx, auto LastIdx, typename Func>
		constexpr void each_ext(Func func) {
			static_assert(LastIdx >= FirstIdx);
			const auto Iters = LastIdx - FirstIdx;
			detail::each_impl<FirstIdx, Iters, Func>(func, std::make_integer_sequence<decltype(Iters), Iters>());
		}

		//! Compile-time for loop with adjustable range and iteration size.
		//! Iteration starts at \tparam FirstIdx and ends at \tparam LastIdx
		//! (excluding) at increments of \tparam Inc.
		//!
		//! Example 1 (index argument):
		//! sarray<int, 10> arr;
		//! each_ext<0, 10, 2>([&arr](auto i) {
		//!    GAIA_LOG_N("%d", i);
		//! });
		//!
		//! Example 2 (no argument):
		//! uint32_t cnt = 0;
		//! each_ext<0, 10, 2>([&cnt]() {
		//!    GAIA_LOG_N("Invocation number: %u", cnt++);
		//! });
		template <auto FirstIdx, auto LastIdx, auto Inc, typename Func>
		constexpr void each_ext(Func func) {
			if constexpr (FirstIdx < LastIdx) {
				if constexpr (std::is_invocable_v<Func&&, std::integral_constant<decltype(FirstIdx), FirstIdx>>)
					func(std::integral_constant<decltype(FirstIdx), FirstIdx>());
				else
					func();

				each_ext<FirstIdx + Inc, LastIdx, Inc>(func);
			}
		}

		//! Compile-time for loop over parameter packs.
		//!
		//! Example:
		//! template<typename... Args>
		//! void print(const Args&... args) {
		//!  each_pack([](const auto& value) {
		//!    std::cout << value << std::endl;
		//!  });
		//! }
		//! print(69, "likes", 420.0f);
		template <typename Func, typename... Args>
		constexpr void each_pack(Func func, Args&&... args) {
			(func(GAIA_FWD(args)), ...);
		}

		//! Compile-time for loop over tuples and other objects implementing
		//! tuple_size (sarray, std::pair etc).
		//!
		//! Example:
		//! each_tuple(
		//!		std::make_tuple(69, "likes", 420.0f),
		//!		[](const auto& value) {
		//! 		std::cout << value << std::endl;
		//! 	});
		//! Output:
		//! 69
		//! likes
		//! 420.0f
		template <typename Tuple, typename Func>
		constexpr void each_tuple(Tuple&& tuple, Func func) {
			using TTSize = uint32_t;
			constexpr auto TSize = (TTSize)std::tuple_size<std::remove_reference_t<Tuple>>::value;
			detail::each_tuple_impl<(TTSize)0>(GAIA_FWD(tuple), func, std::make_integer_sequence<TTSize, TSize>{});
		}

		//! Compile-time for loop over tuples and other objects implementing
		//! tuple_size (sarray, std::pair etc).
		//! \warning This does not use a tuple instance, only the type.
		//!          Use for compile-time operations only.
		//!
		//! Example:
		//! each_tuple(
		//!		std::make_tuple(69, "likes", 420.0f),
		//!		[](const auto& value) {
		//! 		std::cout << value << std::endl;
		//! 	});
		//! Output:
		//! 0
		//! nullptr
		//! 0.0f
		template <typename Tuple, typename Func>
		constexpr void each_tuple(Func func) {
			using TTSize = uint32_t;
			constexpr auto TSize = (TTSize)std::tuple_size<std::remove_reference_t<Tuple>>::value;
			detail::each_tuple_impl<(TTSize)0, Tuple>(func, std::make_integer_sequence<TTSize, TSize>{});
		}

		//! Compile-time for loop over tuples and other objects implementing
		//! tuple_size (sarray, std::pair etc).
		//! Iteration starts at \tparam FirstIdx and ends at \tparam LastIdx (excluding).
		//!
		//! Example:
		//! each_tuple_ext<1, 3>(
		//!		std::make_tuple(69, "likes", 420.0f),
		//!		[](const auto& value) {
		//! 		std::cout << value << std::endl;
		//! 	});
		//! Output:
		//! likes
		//! 420.0f
		template <auto FirstIdx, auto LastIdx, typename Tuple, typename Func>
		constexpr void each_tuple_ext(Tuple&& tuple, Func func) {
			constexpr auto TSize = std::tuple_size<std::remove_reference_t<Tuple>>::value;
			static_assert(LastIdx >= FirstIdx);
			static_assert(LastIdx <= TSize);
			constexpr auto Iters = LastIdx - FirstIdx;
			detail::each_tuple_impl<FirstIdx>(GAIA_FWD(tuple), func, std::make_integer_sequence<decltype(FirstIdx), Iters>{});
		}

		//! Compile-time for loop over tuples and other objects implementing
		//! tuple_size (sarray, std::pair etc).
		//! Iteration starts at \tparam FirstIdx and ends at \tparam LastIdx (excluding).
		//! \warning This does not use a tuple instance, only the type.
		//!          Use for compile-time operations only.
		//!
		//! Example:
		//! each_tuple(
		//!		1, 3, std::make_tuple(69, "likes", 420.0f),
		//!		[](const auto& value) {
		//! 		std::cout << value << std::endl;
		//! 	});
		//! Output:
		//! nullptr
		//! 0.0f
		template <auto FirstIdx, auto LastIdx, typename Tuple, typename Func>
		constexpr void each_tuple_ext(Func func) {
			constexpr auto TSize = std::tuple_size<std::remove_reference_t<Tuple>>::value;
			static_assert(LastIdx >= FirstIdx);
			static_assert(LastIdx <= TSize);
			constexpr auto Iters = LastIdx - FirstIdx;
			detail::each_tuple_impl<FirstIdx, Tuple>(func, std::make_integer_sequence<decltype(FirstIdx), Iters>{});
		}

		template <typename InputIt, typename Func>
		constexpr Func each(InputIt first, InputIt last, Func func) {
			for (; first != last; ++first)
				func(*first);
			return func;
		}

		template <typename C, typename Func>
		constexpr auto each(const C& arr, Func func) {
			return each(arr.begin(), arr.end(), func);
		}

		//----------------------------------------------------------------------
		// Lookups
		//----------------------------------------------------------------------

		template <typename InputIt, typename T>
		constexpr InputIt find(InputIt first, InputIt last, const T& value) {
			if constexpr (std::is_pointer_v<InputIt>) {
				auto size = distance(first, last);
				for (decltype(size) i = 0; i < size; ++i) {
					if (first[i] == value)
						return &first[i];
				}
			} else if constexpr (std::is_same_v<typename InputIt::iterator_category, core::random_access_iterator_tag>) {
				auto size = distance(first, last);
				for (decltype(size) i = 0; i < size; ++i) {
					if (*(first[i]) == value)
						return first[i];
				}
			} else {
				for (; first != last; ++first) {
					if (*first == value)
						return first;
				}
			}
			return last;
		}

		template <typename C, typename V>
		constexpr auto find(const C& arr, const V& item) {
			if constexpr (has_find<C>::value)
				return arr.find(item);
			else
				return core::find(arr.begin(), arr.end(), item);
		}

		template <typename InputIt, typename Func>
		constexpr InputIt find_if(InputIt first, InputIt last, Func func) {
			if constexpr (std::is_pointer_v<InputIt>) {
				auto size = distance(first, last);
				for (decltype(size) i = 0; i < size; ++i) {
					if (func(first[i]))
						return &first[i];
				}
			} else if constexpr (std::is_same_v<typename InputIt::iterator_category, core::random_access_iterator_tag>) {
				auto size = distance(first, last);
				for (decltype(size) i = 0; i < size; ++i) {
					if (func(*(first[i])))
						return first[i];
				}
			} else {
				for (; first != last; ++first) {
					if (func(*first))
						return first;
				}
			}
			return last;
		}

		template <typename UnaryPredicate, typename C>
		constexpr auto find_if(const C& arr, UnaryPredicate predicate) {
			if constexpr (has_find_if<C, UnaryPredicate>::value)
				return arr.find_id(predicate);
			else
				return core::find_if(arr.begin(), arr.end(), predicate);
		}

		template <typename InputIt, typename Func>
		constexpr InputIt find_if_not(InputIt first, InputIt last, Func func) {
			if constexpr (std::is_pointer_v<InputIt>) {
				auto size = distance(first, last);
				for (decltype(size) i = 0; i < size; ++i) {
					if (!func(first[i]))
						return &first[i];
				}
			} else if constexpr (std::is_same_v<typename InputIt::iterator_category, core::random_access_iterator_tag>) {
				auto size = distance(first, last);
				for (decltype(size) i = 0; i < size; ++i) {
					if (!func(*(first[i])))
						return first[i];
				}
			} else {
				for (; first != last; ++first) {
					if (!func(*first))
						return first;
				}
			}
			return last;
		}

		template <typename UnaryPredicate, typename C>
		constexpr auto find_if_not(const C& arr, UnaryPredicate predicate) {
			if constexpr (has_find_if_not<C, UnaryPredicate>::value)
				return arr.find_if_not(predicate);
			else
				return core::find_if_not(arr.begin(), arr.end(), predicate);
		}

		//----------------------------------------------------------------------

		template <typename C, typename V>
		constexpr bool has(const C& arr, const V& item) {
			const auto it = find(arr, item);
			return it != arr.end();
		}

		template <typename UnaryPredicate, typename C>
		constexpr bool has_if(const C& arr, UnaryPredicate predicate) {
			const auto it = find_if(arr, predicate);
			return it != arr.end();
		}

		//----------------------------------------------------------------------

		template <typename C>
		constexpr auto get_index(const C& arr, typename C::const_reference item) {
			const auto it = find(arr, item);
			if (it == arr.end())
				return BadIndex;

			return (decltype(BadIndex))core::distance(arr.begin(), it);
		}

		template <typename C>
		constexpr auto get_index_unsafe(const C& arr, typename C::const_reference item) {
			return (decltype(BadIndex))core::distance(arr.begin(), find(arr, item));
		}

		template <typename UnaryPredicate, typename C>
		constexpr auto get_index_if(const C& arr, UnaryPredicate predicate) {
			const auto it = find_if(arr, predicate);
			if (it == arr.end())
				return BadIndex;

			return (decltype(BadIndex))core::distance(arr.begin(), it);
		}

		template <typename UnaryPredicate, typename C>
		constexpr auto get_index_if_unsafe(const C& arr, UnaryPredicate predicate) {
			return (decltype(BadIndex))core::distance(arr.begin(), find_if(arr, predicate));
		}

		//----------------------------------------------------------------------
		// Erasure
		//----------------------------------------------------------------------

		//! Replaces the item at \param idx in the array \param arr with the last item of the array if possible and
		//! removes its last item. Use when shifting of the entire array is not wanted. \warning If the item order is
		//! important and the size of the array changes after calling this function you need to sort the array.
		//! \warning Does not do bound checks. Undefined behavior when \param idx is out of bounds.
		template <typename C>
		void swap_erase_unsafe(C& arr, typename C::size_type idx) {
			GAIA_ASSERT(idx < arr.size());

			if (idx + 1 != arr.size())
				arr[idx] = arr[arr.size() - 1];

			arr.pop_back();
		}

		//! Replaces the item at \param idx in the array \param arr with the last item of the array if possible and
		//! removes its last item. Use when shifting of the entire array is not wanted. \warning If the item order is
		//! important and the size of the array changes after calling this function you need to sort the array.
		template <typename C>
		void swap_erase(C& arr, typename C::size_type idx) {
			if (idx >= arr.size())
				return;

			if (idx + 1 != arr.size())
				arr[idx] = arr[arr.size() - 1];

			arr.pop_back();
		}

		//----------------------------------------------------------------------
		// Comparison
		//----------------------------------------------------------------------

		template <typename T>
		struct equal_to {
			constexpr bool operator()(const T& lhs, const T& rhs) const {
				return lhs == rhs;
			}
		};

		template <typename T>
		struct is_smaller {
			constexpr bool operator()(const T& lhs, const T& rhs) const {
				return lhs < rhs;
			}
		};

		template <typename T>
		struct is_smaller_or_equal {
			constexpr bool operator()(const T& lhs, const T& rhs) const {
				return lhs <= rhs;
			}
		};

		template <typename T>
		struct is_greater {
			constexpr bool operator()(const T& lhs, const T& rhs) const {
				return lhs > rhs;
			}
		};

		//----------------------------------------------------------------------
		// Sorting
		//----------------------------------------------------------------------

		namespace detail {
			template <typename Array, typename TCmpFunc>
			constexpr void comb_sort_impl(Array& array_, TCmpFunc cmpFunc) noexcept {
				constexpr double Factor = 1.247330950103979;
				using size_type = typename Array::size_type;

				size_type gap = array_.size();
				bool swapped = false;
				while ((gap > size_type{1}) || swapped) {
					if (gap > size_type{1}) {
						gap = static_cast<size_type>(gap / Factor);
					}
					swapped = false;
					for (size_type i = size_type{0}; gap + i < static_cast<size_type>(array_.size()); ++i) {
						if (!cmpFunc(array_[i], array_[i + gap])) {
							auto swap = array_[i];
							array_[i] = array_[i + gap];
							array_[i + gap] = swap;
							swapped = true;
						}
					}
				}
			}

			template <typename Container, typename TCmpFunc>
			int quick_sort_partition(Container& arr, int low, int high, TCmpFunc cmpFunc) {
				const auto& pivot = arr[(uint32_t)high];
				int i = low - 1;
				for (int j = low; j <= high - 1; ++j) {
					if (cmpFunc(arr[(uint32_t)j], pivot))
						core::swap(arr[(uint32_t)++i], arr[(uint32_t)j]);
				}
				core::swap(arr[(uint32_t)++i], arr[(uint32_t)high]);
				return i;
			}

			template <typename Container, typename TCmpFunc>
			void quick_sort(Container& arr, int low, int high, TCmpFunc cmpFunc) {
				if (low >= high)
					return;
				auto pos = quick_sort_partition(arr, low, high, cmpFunc);
				quick_sort(arr, low, pos - 1, cmpFunc);
				quick_sort(arr, pos + 1, high, cmpFunc);
			}

			template <typename Container, typename TCmpFunc, typename TSortFunc>
			int quick_sort_partition(Container& arr, int low, int high, TCmpFunc cmpFunc, TSortFunc sortFunc) {
				const auto& pivot = arr[(uint32_t)high];
				int i = low - 1;
				for (int j = low; j <= high - 1; ++j) {
					if (cmpFunc(arr[(uint32_t)j], pivot))
						sortFunc((uint32_t)++i, (uint32_t)j);
				}
				sortFunc((uint32_t)++i, (uint32_t)high);
				return i;
			}

			template <typename Container, typename TCmpFunc, typename TSortFunc>
			void quick_sort(Container& arr, int low, int high, TCmpFunc cmpFunc, TSortFunc sortFunc) {
				if (low >= high)
					return;
				auto pos = quick_sort_partition(arr, low, high, cmpFunc, sortFunc);
				quick_sort(arr, low, pos - 1, cmpFunc, sortFunc);
				quick_sort(arr, pos + 1, high, cmpFunc, sortFunc);
			}
		} // namespace detail

		//! Compile-time sort.
		//! Implements a sorting network for \tparam N up to 8
		template <typename Container, typename TCmpFunc>
		constexpr void sort_ct(Container& arr, TCmpFunc cmpFunc) noexcept {
			constexpr size_t NItems = std::tuple_size<Container>::value;
			if constexpr (NItems <= 1) {
				return;
			} else if constexpr (NItems == 2) {
				swap_if(arr[0], arr[1], cmpFunc);
			} else if constexpr (NItems == 3) {
				swap_if(arr[1], arr[2], cmpFunc);
				swap_if(arr[0], arr[2], cmpFunc);
				swap_if(arr[0], arr[1], cmpFunc);
			} else if constexpr (NItems == 4) {
				swap_if(arr[0], arr[1], cmpFunc);
				swap_if(arr[2], arr[3], cmpFunc);

				swap_if(arr[0], arr[2], cmpFunc);
				swap_if(arr[1], arr[3], cmpFunc);

				swap_if(arr[1], arr[2], cmpFunc);
			} else if constexpr (NItems == 5) {
				swap_if(arr[0], arr[1], cmpFunc);
				swap_if(arr[3], arr[4], cmpFunc);

				swap_if(arr[2], arr[4], cmpFunc);

				swap_if(arr[2], arr[3], cmpFunc);
				swap_if(arr[1], arr[4], cmpFunc);

				swap_if(arr[0], arr[3], cmpFunc);

				swap_if(arr[0], arr[2], cmpFunc);
				swap_if(arr[1], arr[3], cmpFunc);

				swap_if(arr[1], arr[2], cmpFunc);
			} else if constexpr (NItems == 6) {
				swap_if(arr[1], arr[2], cmpFunc);
				swap_if(arr[4], arr[5], cmpFunc);

				swap_if(arr[0], arr[2], cmpFunc);
				swap_if(arr[3], arr[5], cmpFunc);

				swap_if(arr[0], arr[1], cmpFunc);
				swap_if(arr[3], arr[4], cmpFunc);
				swap_if(arr[2], arr[5], cmpFunc);

				swap_if(arr[0], arr[3], cmpFunc);
				swap_if(arr[1], arr[4], cmpFunc);

				swap_if(arr[2], arr[4], cmpFunc);
				swap_if(arr[1], arr[3], cmpFunc);

				swap_if(arr[2], arr[3], cmpFunc);
			} else if constexpr (NItems == 7) {
				swap_if(arr[1], arr[2], cmpFunc);
				swap_if(arr[3], arr[4], cmpFunc);
				swap_if(arr[5], arr[6], cmpFunc);

				swap_if(arr[0], arr[2], cmpFunc);
				swap_if(arr[3], arr[5], cmpFunc);
				swap_if(arr[4], arr[6], cmpFunc);

				swap_if(arr[0], arr[1], cmpFunc);
				swap_if(arr[4], arr[5], cmpFunc);
				swap_if(arr[2], arr[6], cmpFunc);

				swap_if(arr[0], arr[4], cmpFunc);
				swap_if(arr[1], arr[5], cmpFunc);

				swap_if(arr[0], arr[3], cmpFunc);
				swap_if(arr[2], arr[5], cmpFunc);

				swap_if(arr[1], arr[3], cmpFunc);
				swap_if(arr[2], arr[4], cmpFunc);

				swap_if(arr[2], arr[3], cmpFunc);
			} else if constexpr (NItems == 8) {
				swap_if(arr[0], arr[1], cmpFunc);
				swap_if(arr[2], arr[3], cmpFunc);
				swap_if(arr[4], arr[5], cmpFunc);
				swap_if(arr[6], arr[7], cmpFunc);

				swap_if(arr[0], arr[2], cmpFunc);
				swap_if(arr[1], arr[3], cmpFunc);
				swap_if(arr[4], arr[6], cmpFunc);
				swap_if(arr[5], arr[7], cmpFunc);

				swap_if(arr[1], arr[2], cmpFunc);
				swap_if(arr[5], arr[6], cmpFunc);
				swap_if(arr[0], arr[4], cmpFunc);
				swap_if(arr[3], arr[7], cmpFunc);

				swap_if(arr[1], arr[5], cmpFunc);
				swap_if(arr[2], arr[6], cmpFunc);

				swap_if(arr[1], arr[4], cmpFunc);
				swap_if(arr[3], arr[6], cmpFunc);

				swap_if(arr[2], arr[4], cmpFunc);
				swap_if(arr[3], arr[5], cmpFunc);

				swap_if(arr[3], arr[4], cmpFunc);
			} else if constexpr (NItems == 9) {
				swap_if(arr[0], arr[1], cmpFunc);
				swap_if(arr[3], arr[4], cmpFunc);
				swap_if(arr[6], arr[7], cmpFunc);

				swap_if(arr[1], arr[2], cmpFunc);
				swap_if(arr[4], arr[5], cmpFunc);
				swap_if(arr[7], arr[8], cmpFunc);

				swap_if(arr[0], arr[1], cmpFunc);
				swap_if(arr[3], arr[4], cmpFunc);
				swap_if(arr[6], arr[7], cmpFunc);

				swap_if(arr[0], arr[3], cmpFunc);
				swap_if(arr[3], arr[6], cmpFunc);
				swap_if(arr[0], arr[3], cmpFunc);

				swap_if(arr[1], arr[4], cmpFunc);
				swap_if(arr[4], arr[7], cmpFunc);
				swap_if(arr[1], arr[4], cmpFunc);

				swap_if(arr[5], arr[8], cmpFunc);
				swap_if(arr[2], arr[5], cmpFunc);
				swap_if(arr[5], arr[8], cmpFunc);

				swap_if(arr[2], arr[4], cmpFunc);
				swap_if(arr[4], arr[6], cmpFunc);
				swap_if(arr[2], arr[4], cmpFunc);

				swap_if(arr[1], arr[3], cmpFunc);
				swap_if(arr[2], arr[3], cmpFunc);
				swap_if(arr[5], arr[7], cmpFunc);
				swap_if(arr[5], arr[6], cmpFunc);
			} else {
				GAIA_MSVC_WARNING_PUSH()
				GAIA_MSVC_WARNING_DISABLE(4244)
				detail::comb_sort_impl(arr, cmpFunc);
				GAIA_MSVC_WARNING_POP()
			}
		}

		//! Sort the array \param arr given a comparison function \param cmpFunc.
		//! Sorts using a sorting network up to 8 elements. Quick sort above 32.
		//! \tparam Container Container to sort
		//! \tparam TCmpFunc Comparision function
		//! \param arr Container to sort
		//! \param cmpFunc Comparision function
		template <typename Container, typename TCmpFunc>
		void sort(Container& arr, TCmpFunc cmpFunc) {
			if (arr.size() <= 1) {
				// Nothing to sort with just one item
			} else if (arr.size() == 2) {
				swap_if(arr[0], arr[1], cmpFunc);
			} else if (arr.size() == 3) {
				swap_if(arr[1], arr[2], cmpFunc);
				swap_if(arr[0], arr[2], cmpFunc);
				swap_if(arr[0], arr[1], cmpFunc);
			} else if (arr.size() == 4) {
				swap_if(arr[0], arr[1], cmpFunc);
				swap_if(arr[2], arr[3], cmpFunc);

				swap_if(arr[0], arr[2], cmpFunc);
				swap_if(arr[1], arr[3], cmpFunc);

				swap_if(arr[1], arr[2], cmpFunc);
			} else if (arr.size() == 5) {
				swap_if(arr[0], arr[1], cmpFunc);
				swap_if(arr[3], arr[4], cmpFunc);

				swap_if(arr[2], arr[4], cmpFunc);

				swap_if(arr[2], arr[3], cmpFunc);
				swap_if(arr[1], arr[4], cmpFunc);

				swap_if(arr[0], arr[3], cmpFunc);

				swap_if(arr[0], arr[2], cmpFunc);
				swap_if(arr[1], arr[3], cmpFunc);

				swap_if(arr[1], arr[2], cmpFunc);
			} else if (arr.size() == 6) {
				swap_if(arr[1], arr[2], cmpFunc);
				swap_if(arr[4], arr[5], cmpFunc);

				swap_if(arr[0], arr[2], cmpFunc);
				swap_if(arr[3], arr[5], cmpFunc);

				swap_if(arr[0], arr[1], cmpFunc);
				swap_if(arr[3], arr[4], cmpFunc);
				swap_if(arr[2], arr[5], cmpFunc);

				swap_if(arr[0], arr[3], cmpFunc);
				swap_if(arr[1], arr[4], cmpFunc);

				swap_if(arr[2], arr[4], cmpFunc);
				swap_if(arr[1], arr[3], cmpFunc);

				swap_if(arr[2], arr[3], cmpFunc);
			} else if (arr.size() == 7) {
				swap_if(arr[1], arr[2], cmpFunc);
				swap_if(arr[3], arr[4], cmpFunc);
				swap_if(arr[5], arr[6], cmpFunc);

				swap_if(arr[0], arr[2], cmpFunc);
				swap_if(arr[3], arr[5], cmpFunc);
				swap_if(arr[4], arr[6], cmpFunc);

				swap_if(arr[0], arr[1], cmpFunc);
				swap_if(arr[4], arr[5], cmpFunc);
				swap_if(arr[2], arr[6], cmpFunc);

				swap_if(arr[0], arr[4], cmpFunc);
				swap_if(arr[1], arr[5], cmpFunc);

				swap_if(arr[0], arr[3], cmpFunc);
				swap_if(arr[2], arr[5], cmpFunc);

				swap_if(arr[1], arr[3], cmpFunc);
				swap_if(arr[2], arr[4], cmpFunc);

				swap_if(arr[2], arr[3], cmpFunc);
			} else if (arr.size() == 8) {
				swap_if(arr[0], arr[1], cmpFunc);
				swap_if(arr[2], arr[3], cmpFunc);
				swap_if(arr[4], arr[5], cmpFunc);
				swap_if(arr[6], arr[7], cmpFunc);

				swap_if(arr[0], arr[2], cmpFunc);
				swap_if(arr[1], arr[3], cmpFunc);
				swap_if(arr[4], arr[6], cmpFunc);
				swap_if(arr[5], arr[7], cmpFunc);

				swap_if(arr[1], arr[2], cmpFunc);
				swap_if(arr[5], arr[6], cmpFunc);
				swap_if(arr[0], arr[4], cmpFunc);
				swap_if(arr[3], arr[7], cmpFunc);

				swap_if(arr[1], arr[5], cmpFunc);
				swap_if(arr[2], arr[6], cmpFunc);

				swap_if(arr[1], arr[4], cmpFunc);
				swap_if(arr[3], arr[6], cmpFunc);

				swap_if(arr[2], arr[4], cmpFunc);
				swap_if(arr[3], arr[5], cmpFunc);

				swap_if(arr[3], arr[4], cmpFunc);
			} else if (arr.size() <= 32) {
				auto n = arr.size();
				for (decltype(n) i = 0; i < n - 1; ++i) {
					for (decltype(n) j = 0; j < n - i - 1; ++j)
						swap_if(arr[j], arr[j + 1], cmpFunc);
				}
			} else {
				const auto n = (int)arr.size();
				detail::quick_sort(arr, 0, n - 1, cmpFunc);
			}
		}

		//! Sort the array \param arr given a comparison function \param cmpFunc.
		//! If cmpFunc returns true it performs \param sortFunc which can perform the sorting.
		//! Use when it is necessary to sort multiple arrays at once.
		//! Sorts using a sorting network up to 8 elements.
		//! \warning Currently only up to 32 elements are supported.
		//! \tparam Container Container to sort
		//! \tparam TCmpFunc Comparision function
		//! \tparam TSortFunc Sorting function
		//! \param arr Container to sort
		//! \param cmpFunc Comparision function
		//! \param sortFunc Sorting function
		template <typename Container, typename TCmpFunc, typename TSortFunc>
		void sort(Container& arr, TCmpFunc cmpFunc, TSortFunc sortFunc) {
			if (arr.size() <= 1) {
				// Nothing to sort with just one item
			} else if (arr.size() == 2) {
				try_swap_if(arr, 0, 1, cmpFunc, sortFunc);
			} else if (arr.size() == 3) {
				try_swap_if(arr, 1, 2, cmpFunc, sortFunc);
				try_swap_if(arr, 0, 2, cmpFunc, sortFunc);
				try_swap_if(arr, 0, 1, cmpFunc, sortFunc);
			} else if (arr.size() == 4) {
				try_swap_if(arr, 0, 1, cmpFunc, sortFunc);
				try_swap_if(arr, 2, 3, cmpFunc, sortFunc);

				try_swap_if(arr, 0, 2, cmpFunc, sortFunc);
				try_swap_if(arr, 1, 3, cmpFunc, sortFunc);

				try_swap_if(arr, 1, 2, cmpFunc, sortFunc);
			} else if (arr.size() == 5) {
				try_swap_if(arr, 0, 1, cmpFunc, sortFunc);
				try_swap_if(arr, 3, 4, cmpFunc, sortFunc);

				try_swap_if(arr, 2, 4, cmpFunc, sortFunc);

				try_swap_if(arr, 2, 3, cmpFunc, sortFunc);
				try_swap_if(arr, 1, 4, cmpFunc, sortFunc);

				try_swap_if(arr, 0, 3, cmpFunc, sortFunc);

				try_swap_if(arr, 0, 2, cmpFunc, sortFunc);
				try_swap_if(arr, 1, 3, cmpFunc, sortFunc);

				try_swap_if(arr, 1, 2, cmpFunc, sortFunc);
			} else if (arr.size() == 6) {
				try_swap_if(arr, 1, 2, cmpFunc, sortFunc);
				try_swap_if(arr, 4, 5, cmpFunc, sortFunc);

				try_swap_if(arr, 0, 2, cmpFunc, sortFunc);
				try_swap_if(arr, 3, 5, cmpFunc, sortFunc);

				try_swap_if(arr, 0, 1, cmpFunc, sortFunc);
				try_swap_if(arr, 3, 4, cmpFunc, sortFunc);
				try_swap_if(arr, 2, 5, cmpFunc, sortFunc);

				try_swap_if(arr, 0, 3, cmpFunc, sortFunc);
				try_swap_if(arr, 1, 4, cmpFunc, sortFunc);

				try_swap_if(arr, 2, 4, cmpFunc, sortFunc);
				try_swap_if(arr, 1, 3, cmpFunc, sortFunc);

				try_swap_if(arr, 2, 3, cmpFunc, sortFunc);
			} else if (arr.size() == 7) {
				try_swap_if(arr, 1, 2, cmpFunc, sortFunc);
				try_swap_if(arr, 3, 4, cmpFunc, sortFunc);
				try_swap_if(arr, 5, 6, cmpFunc, sortFunc);

				try_swap_if(arr, 0, 2, cmpFunc, sortFunc);
				try_swap_if(arr, 3, 5, cmpFunc, sortFunc);
				try_swap_if(arr, 4, 6, cmpFunc, sortFunc);

				try_swap_if(arr, 0, 1, cmpFunc, sortFunc);
				try_swap_if(arr, 4, 5, cmpFunc, sortFunc);
				try_swap_if(arr, 2, 6, cmpFunc, sortFunc);

				try_swap_if(arr, 0, 4, cmpFunc, sortFunc);
				try_swap_if(arr, 1, 5, cmpFunc, sortFunc);

				try_swap_if(arr, 0, 3, cmpFunc, sortFunc);
				try_swap_if(arr, 2, 5, cmpFunc, sortFunc);

				try_swap_if(arr, 1, 3, cmpFunc, sortFunc);
				try_swap_if(arr, 2, 4, cmpFunc, sortFunc);

				try_swap_if(arr, 2, 3, cmpFunc, sortFunc);
			} else if (arr.size() == 8) {
				try_swap_if(arr, 0, 1, cmpFunc, sortFunc);
				try_swap_if(arr, 2, 3, cmpFunc, sortFunc);
				try_swap_if(arr, 4, 5, cmpFunc, sortFunc);
				try_swap_if(arr, 6, 7, cmpFunc, sortFunc);

				try_swap_if(arr, 0, 2, cmpFunc, sortFunc);
				try_swap_if(arr, 1, 3, cmpFunc, sortFunc);
				try_swap_if(arr, 4, 6, cmpFunc, sortFunc);
				try_swap_if(arr, 5, 7, cmpFunc, sortFunc);

				try_swap_if(arr, 1, 2, cmpFunc, sortFunc);
				try_swap_if(arr, 5, 6, cmpFunc, sortFunc);
				try_swap_if(arr, 0, 4, cmpFunc, sortFunc);
				try_swap_if(arr, 3, 7, cmpFunc, sortFunc);

				try_swap_if(arr, 1, 5, cmpFunc, sortFunc);
				try_swap_if(arr, 2, 6, cmpFunc, sortFunc);

				try_swap_if(arr, 1, 4, cmpFunc, sortFunc);
				try_swap_if(arr, 3, 6, cmpFunc, sortFunc);

				try_swap_if(arr, 2, 4, cmpFunc, sortFunc);
				try_swap_if(arr, 3, 5, cmpFunc, sortFunc);

				try_swap_if(arr, 3, 4, cmpFunc, sortFunc);
			} else if (arr.size() <= 32) {
				auto n = arr.size();
				for (decltype(n) i = 0; i < n - 1; ++i)
					for (decltype(n) j = 0; j < n - i - 1; ++j)
						try_swap_if(arr, j, j + 1, cmpFunc, sortFunc);
			} else {
				const int n = (int)arr.size();
				detail::quick_sort(arr, 0, n - 1, cmpFunc, sortFunc);
			}
		}

		//----------------------------------------------------------------------
		// Strings
		//----------------------------------------------------------------------

		inline auto trim(std::span<const char> expr) {
			uint32_t beg = 0;
			while (expr[beg] == ' ')
				++beg;
			uint32_t end = (uint32_t)expr.size() - 1;
			while (end > beg && expr[end] == ' ')
				--end;
			return expr.subspan(beg, end - beg + 1);
		};

	} // namespace core
} // namespace gaia
