#ifndef __LIBSTORED_UTIL_H
#define __LIBSTORED_UTIL_H

#include <libstored/macros.h>

/*!
 * \def likely(expr)
 * \brief Marks the given expression to likely be evaluated to true.
 * \details This may help compiler optimization.
 * \returns the evaluated \c expr
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
 */
#ifndef unlikely
#  ifdef __GNUC__
#    define unlikely(expr) __builtin_expect(!!(expr), 0)
#  else
#    define unlikely(expr) (expr)
#  endif
#endif

#ifdef __cplusplus
#include <cassert>
#include <math.h>
#include <limits>
#include <list>
#include <cstring>

#if __cplusplus >= 201103L
#  include <functional>
#  include <type_traits>

#  ifndef DOXYGEN
#    define SFINAE_IS_FUNCTION(T, F, T_OK) \
	typename std::enable_if<std::is_assignable<std::function<F>, T>::value, T_OK>::type
#  else
#    define SFINAE_IS_FUNCTION(T, F, T_OK) T_OK
#  endif
#else
#  define SFINAE_IS_FUNCTION(T, F, T_OK) T_OK
#endif

#if defined(__cplusplus) && __cplusplus < 201103L && !defined(static_assert)
#  define static_assert(expr, msg)	do { typedef __attribute__((unused)) int static_assert_[(expr) ? 1 : -1]; } while(0)
#endif


namespace stored {

#ifdef STORED_HAVE_ZTH
#  define stored_assert(expr)	do { if(::stored::Config::EnableAssert) zth_assert(expr); } while(false)
#else
#  define stored_assert(expr)	do { if(::stored::Config::EnableAssert) assert(expr); } while(false)
#endif

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
					if(value <= -std::numeric_limits<R>::max())
						return -std::numeric_limits<R>::max();
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

} // namespace

template <typename R, typename T>
__attribute__((pure)) R saturated_cast(T value) { return stored::impl::saturated_cast_helper<R>::cast(value); }

#endif // __cplusplus
#endif // __LIBSTORED_UTIL_H
