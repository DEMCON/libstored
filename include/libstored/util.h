#ifndef __LIBSTORED_UTIL_H
#define __LIBSTORED_UTIL_H
/*
 * libstored, a Store for Embedded Debugger.
 * Copyright (C) 2020  Jochem Rutgers
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

/*!
 * \defgroup libstored_util util
 * \brief Misc helper functionality.
 * \ingroup libstored
 */

#include <libstored/macros.h>
#include <libstored/config.h>

#ifdef STORED_HAVE_ZTH
#  include <libzth/util.h>
#  include <libzth/worker.h>
#endif

/*!
 * \def likely(expr)
 * \brief Marks the given expression to likely be evaluated to true.
 * \details This may help compiler optimization.
 * \returns the evaluated \c expr
 * \ingroup libstored_util
 */
#ifndef likely
#  ifdef __GNUC__
#    define likely(expr) __builtin_expect(!!(expr), 1)
#  else
#    define likely(expr) (expr)
#  endif
#endif

/*!
 * \def unlikely(expr)
 * \brief Marks the given expression to likely be evaluated to true.
 * \details This may help compiler optimization.
 * \returns the evaluated \c expr
 * \ingroup libstored_util
 */
#ifndef unlikely
#  ifdef __GNUC__
#    define unlikely(expr) __builtin_expect(!!(expr), 0)
#  else
#    define unlikely(expr) (expr)
#  endif
#endif

/*!
 * \def stored_yield()
 * \brief Allow to yield the processor during long-running tasks.
 */
#ifndef stored_yield
#  ifdef STORED_HAVE_ZTH
#    ifdef __cplusplus
#      define stored_yield()	zth::yield()
#    else
#      define stored_yield()	zth_yield()
#    endif
#  else
#    define stored_yield()		do {} while(false)
#  endif
#endif

#ifdef __cplusplus
#include <cassert>
#include <cmath>
#include <limits>
#include <list>
#include <cstring>

#if __cplusplus >= 201103L
#  include <functional>
#  include <type_traits>

/*!
 * \def SFINAE_IS_FUNCTION
 * \param T the type to check
 * \param F the function type to match T to
 * \param T_OK the type that is returned by this macro when T matches F
 */
#  ifndef DOXYGEN
#    define SFINAE_IS_FUNCTION(T, F, T_OK) \
	typename std::enable_if<std::is_assignable<std::function<F>, T>::value, T_OK>::type
#  else
#    define SFINAE_IS_FUNCTION(T, F, T_OK) T_OK
#  endif
#else
#  define SFINAE_IS_FUNCTION(T, F, T_OK) T_OK
#endif

#if defined(__cplusplus) && __cplusplus < 201103L && !defined(static_assert) && !defined(STORED_COMPILER_MSVC)
#  define static_assert(expr, msg)	do { typedef __attribute__((unused)) int static_assert_[(expr) ? 1 : -1]; } while(0)
#endif

#ifndef CLASS_NOCOPY
/*!
 * \def CLASS_NOCOPY
 * \brief Emits the copy/move constructor/assignment such that copy/move is not allowed.
 * \details Put this macro inside the private section of your class.
 * \param Class the class this macro is embedded in
 */
#  if __cplusplus >= 201103L
#    define CLASS_NOCOPY(Class) \
	public: \
		/*! \brief Deleted copy constructor. */ \
		Class(Class const&) = delete; \
		/*! \brief Deleted move constructor. */ \
		Class(Class&&) = delete; /* NOLINT(misc-macro-parentheses) */ \
		/*! \brief Deleted assignment operator. */ \
		void operator=(Class const&) = delete; \
		/*! \brief Deleted move assignment operator. */ \
		void operator=(Class&&) = delete; /* NOLINT(misc-macro-parentheses) */
#  else
#    define CLASS_NOCOPY(Class) \
	private: \
		/*! \brief Deleted copy constructor. */ \
		Class(Class const&); \
		/*! \brief Deleted assignment operator. */ \
		void operator=(Class const&);
#  endif
#endif

#ifndef is_default
#  if __cplusplus >= 201103L
#    define is_default = default;
#  else
#    define is_default {}
#  endif
#endif

namespace stored {

	/*!
	 * \brief Like \c assert(), but only emits code when #stored::Config::EnableAssert.
	 * \ingroup libstored_util
	 */
#ifdef STORED_HAVE_ZTH
#  define stored_assert(expr)	do { if(::stored::Config::EnableAssert) { zth_assert(expr); } } while(false)
#else
#  define stored_assert(expr)	do { if(::stored::Config::EnableAssert) { assert(expr); } } while(false)
#endif

	/*!
	 * \brief Determine the number of bytes to save the given unsigned value.
	 */
	template <uint64_t N>
	struct value_bytes { enum { value = value_bytes<(N >> 8u)>::value + 1u }; };
	template <> struct value_bytes<0> { enum { value = 0 }; };

	/*!
	 * \brief Determines a type that can hold the given unsigned value.
	 */
	template <uint64_t N, int bytes = value_bytes<N>::value>
	struct value_type { typedef uint64_t type; };
	template <uint64_t N> struct value_type<N, 4> { typedef uint32_t type; };
	template <uint64_t N> struct value_type<N, 3> { typedef uint32_t type; };
	template <uint64_t N> struct value_type<N, 2> { typedef uint16_t type; };
	template <uint64_t N> struct value_type<N, 1> { typedef uint8_t type; };
	template <uint64_t N> struct value_type<N, 0> { typedef uint8_t type; };

	/*!
	 * \private
	 */
	namespace impl {
		template <typename T> struct signedness_helper { typedef T signed_type; typedef T unsigned_type; };
		template <> struct signedness_helper<short> { typedef short signed_type; typedef unsigned short unsigned_type; };
		template <> struct signedness_helper<unsigned short> { typedef short signed_type; typedef unsigned short unsigned_type; };
		template <> struct signedness_helper<int> { typedef int signed_type; typedef unsigned int unsigned_type; };
		template <> struct signedness_helper<unsigned int> { typedef int signed_type; typedef unsigned int unsigned_type; };
		template <> struct signedness_helper<long> { typedef long signed_type; typedef unsigned long unsigned_type; };
		template <> struct signedness_helper<unsigned long> { typedef long signed_type; typedef unsigned long unsigned_type; };
		template <> struct signedness_helper<long long> { typedef long long signed_type; typedef unsigned long long unsigned_type; };
		template <> struct signedness_helper<unsigned long long> { typedef long long signed_type; typedef unsigned long long unsigned_type; };

		template <typename R> struct saturated_cast_helper
		{
			template <typename T> __attribute__((pure)) static R cast(T value)
			{
				// Lower bound check
				if(std::numeric_limits<R>::is_integer) {
					if(!std::numeric_limits<T>::is_signed) {
						// No need to check
					} else if(!std::numeric_limits<R>::is_signed) {
						if(value <= 0)
							return 0;
					} else {
						// Both are signed.
						if(static_cast<typename signedness_helper<T>::signed_type>(value) <= static_cast<typename signedness_helper<R>::signed_type>(std::numeric_limits<R>::min()))
							return std::numeric_limits<R>::min();
					}
				} else {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Woverflow" // This error triggers when R is integer, but this code path is not trigger then.
					if(value <= -std::numeric_limits<R>::max())
						return -std::numeric_limits<R>::max();
#pragma GCC diagnostic pop
				}

				// Upper bound check
				if(value > 0)
					if(static_cast<typename signedness_helper<T>::unsigned_type>(value) >= static_cast<typename signedness_helper<R>::unsigned_type>(std::numeric_limits<R>::max()))
						return std::numeric_limits<R>::max();

				// Default conversion
				return static_cast<R>(value);
			}

			__attribute__((pure)) static R cast(float value) { return cast(llroundf(value)); }
			__attribute__((pure)) static R cast(double value) { return cast(llround(value)); }
			__attribute__((pure)) static R cast(long double value) { return cast(llroundl(value)); }
			__attribute__((pure)) static R cast(bool value) { return static_cast<R>(value); }
			__attribute__((pure)) static R cast(R value) { return value; }
		};

		template <> struct saturated_cast_helper<float>  { template <typename T> constexpr static float cast(T value) { return static_cast<float>(value); } };
		template <> struct saturated_cast_helper<double> { template <typename T> constexpr static double cast(T value) { return static_cast<double>(value); } };
		template <> struct saturated_cast_helper<long double> { template <typename T> constexpr static long double cast(T value) { return static_cast<long double>(value); } };
		template <> struct saturated_cast_helper<bool>   { template <typename T> __attribute__((pure)) static bool cast(T value) { return static_cast<bool>(saturated_cast_helper<int>::cast(value)); } };
	}

	size_t strncpy(char* __restrict__ dst, char const* __restrict__ src, size_t len);
	int strncmp(char const* __restrict__ str1, size_t len1, char const* __restrict__ str2, size_t len2 = std::numeric_limits<size_t>::max());

} // namespace

/*!
 * \brief Converts a number type to another one, with proper rounding and saturation.
 * \ingroup libstored_util
 */
template <typename R, typename T>
__attribute__((pure)) R saturated_cast(T value) { return stored::impl::saturated_cast_helper<R>::cast(value); }

#endif // __cplusplus
#endif // __LIBSTORED_UTIL_H
