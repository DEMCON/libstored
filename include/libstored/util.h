#ifndef LIBSTORED_UTIL_H
#define LIBSTORED_UTIL_H
// SPDX-FileCopyrightText: 2020-2024 Jochem Rutgers
//
// SPDX-License-Identifier: MPL-2.0

#include <libstored/macros.h>
#include <libstored/config.h>

#if STORED_cplusplus < 201103L
#  include <stdint.h>
#else
#  include <cstdint>
#endif

#ifdef STORED_HAVE_ZTH
#  include <libzth/util.h>
#  include <libzth/worker.h>
#endif

#ifdef STORED_COMPILER_MSVC
#  include <stdlib.h>
#endif

#ifdef STORED_HAVE_VALGRIND
#  include <valgrind/memcheck.h>
#endif

#ifdef STORED_ENABLE_ASAN
#  include <sanitizer/asan_interface.h>
#endif

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
#    define unlikely(expr)                                                    \
	    __builtin_expect(                                                 \
		    !!(expr), /* NOLINT(readability-simplify-boolean-expr) */ \
		    0)
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
#      define stored_yield() zth::yield()
#    else
#      define stored_yield() zth_yield()
#    endif
#  else
#    define stored_yield()                                      \
	    do { /* NOLINT(cppcoreguidelines-avoid-do-while) */ \
	    } while(false)
#  endif
#endif

#define STORED_STRINGIFY_(x) #x
#define STORED_STRINGIFY(x)  STORED_STRINGIFY_(x)

#ifndef STORED_UNUSED
/*!
 * \def STORED_UNUSED
 * \brief Mark a variable or parameter as unused.
 *
 * Use this macro in your function body.
 */
#  define STORED_UNUSED(x) (void)x;
#endif

#ifndef UNUSED
/*!
 * \def UNUSED
 * \brief Mark a variable or parameter as unused.
 *
 * Use this macro in your function body.
 * \see STORED_UNUSED
 */
#  define UNUSED(x) STORED_UNUSED(x)
#endif

#ifndef EXPAND
/*!
 * \brief Force macro expansion before evaluating it.
 */
#  define EXPAND(x) x
#endif

#if defined(__cplusplus) && STORED_cplusplus >= 201103L
#  if defined(STORED_COMPILER_MSVC) && _MSC_VER >= 1900 && _MSC_FULL_VER >= 190023918
#    define STORED_EMPTY_BASES __declspec(empty_bases)
#  elif defined(STORED_COMPILER_CLANG)
#    if __has_declspec_attribute(empty_bases)
#      define STORED_EMPTY_BASES __declspec(empty_bases)
#    endif
#  endif
#endif
#ifndef STORED_EMPTY_BASES
#  define STORED_EMPTY_BASES
#endif

#ifdef STORED_HAVE_VALGRIND
#  define STORED_MAKE_MEM_NOACCESS_VALGRIND(buffer, size) \
	  (void)VALGRIND_MAKE_MEM_NOACCESS(buffer, size)
#  define STORED_MAKE_MEM_UNDEFINED_VALGRIND(buffer, size) \
	  (void)VALGRIND_MAKE_MEM_UNDEFINED(buffer, size)
#  define STORED_MAKE_MEM_DEFINED_VALGRIND(buffer, size) \
	  (void)VALGRIND_MAKE_MEM_DEFINED(buffer, size)
#else // !STORED_HAVE_VALGRIND
#  define STORED_MAKE_MEM_NOACCESS_VALGRIND(buffer, size)  (void)0
#  define STORED_MAKE_MEM_UNDEFINED_VALGRIND(buffer, size) (void)0
#  define STORED_MAKE_MEM_DEFINED_VALGRIND(buffer, size)   (void)0
#  ifndef RUNNING_ON_VALGRIND
#    define RUNNING_ON_VALGRIND 0
#  endif
#endif // STORED_HAVE_VALGRIND

#ifdef STORED_ENABLE_ASAN
#  define STORED_MAKE_MEM_NOACCESS_ASAN(buffer, size)  ASAN_POISON_MEMORY_REGION(buffer, size)
#  define STORED_MAKE_MEM_UNDEFINED_ASAN(buffer, size) ASAN_UNPOISON_MEMORY_REGION(buffer, size)
#  define STORED_MAKE_MEM_DEFINED_ASAN(buffer, size)   ASAN_UNPOISON_MEMORY_REGION(buffer, size)
#else // !STORED_ENABLE_ASAN
#  define STORED_MAKE_MEM_NOACCESS_ASAN(buffer, size)  (void)0
#  define STORED_MAKE_MEM_UNDEFINED_ASAN(buffer, size) (void)0
#  define STORED_MAKE_MEM_DEFINED_ASAN(buffer, size)   (void)0
#endif // STORED_ENABLE_ASAN

#if !defined(NDEBUG) \
	&& ((defined(STORED_HAVE_VALGRIND) && !defined(NVALGRIND)) || defined(STORED_ENABLE_ASAN))
#  define STORED_MAKE_MEM_NOACCESS(buffer, size)              \
	  do { /* NOLINT(cppcoreguidelines-avoid-do-while) */ \
		  void* b_ = (void*)(buffer);                 \
		  size_t s_ = (size_t)(size);                 \
		  STORED_MAKE_MEM_NOACCESS_VALGRIND(b_, s_);  \
		  STORED_MAKE_MEM_NOACCESS_ASAN(b_, s_);      \
	  } while(0)

#  define STORED_MAKE_MEM_UNDEFINED(buffer, size)                 \
	  do { /* NOLINT(cppcoreguidelines-avoid-do-while) */     \
		  void* b_ = (void*)(buffer);                     \
		  size_t s_ = (size_t)(size);                     \
		  STORED_MAKE_MEM_UNDEFINED_VALGRIND(b_, s_);     \
		  STORED_MAKE_MEM_UNDEFINED_ASAN(b_, s_);         \
		  if(Config::Debug && !RUNNING_ON_VALGRIND && b_) \
			  memset(b_, 0xef, s_);                   \
	  } while(0)
#  define STORED_MAKE_MEM_DEFINED(buffer, size)               \
	  do { /* NOLINT(cppcoreguidelines-avoid-do-while) */ \
		  void* b_ = (void*)(buffer);                 \
		  size_t s_ = (size_t)(size);                 \
		  STORED_MAKE_MEM_DEFINED_VALGRIND(b_, s_);   \
		  STORED_MAKE_MEM_DEFINED_ASAN(b_, s_);       \
	  } while(0)
#else
#  define STORED_MAKE_MEM_NOACCESS(buffer, size)              \
	  do { /* NOLINT(cppcoreguidelines-avoid-do-while) */ \
	  } while(0)
#  define STORED_MAKE_MEM_UNDEFINED(buffer, size)             \
	  do { /* NOLINT(cppcoreguidelines-avoid-do-while) */ \
	  } while(0)
#  define STORED_MAKE_MEM_DEFINED(buffer, size)               \
	  do { /* NOLINT(cppcoreguidelines-avoid-do-while) */ \
	  } while(0)
#endif



#ifdef __cplusplus
#  include <cassert>
#  include <cmath>
#  include <cstring>
#  include <limits>
#  include <list>

#  if STORED_cplusplus >= 201103L
#    include <functional>
#    include <type_traits>

/*!
 * \def SFINAE_IS_FUNCTION
 * \param T the type to check
 * \param F the function type to match T to
 * \param T_OK the type that is returned by this macro when T matches F
 */
#    ifndef DOXYGEN
#      define SFINAE_IS_FUNCTION(T, F, T_OK) \
	      typename std::enable_if<std::is_assignable<std::function<F>, T>::value, T_OK>::type
#    else
#      define SFINAE_IS_FUNCTION(T, F, T_OK) T_OK
#    endif
#  else
#    define SFINAE_IS_FUNCTION(T, F, T_OK) T_OK
#  endif

#  if defined(STORED_cplusplus) && STORED_cplusplus < 201103L && !defined(static_assert) \
	  && !defined(STORED_COMPILER_MSVC)
#    define static_assert(expr, msg)                                                     \
	    do { /* NOLINT(cppcoreguidelines-avoid-do-while) */                          \
		    typedef __attribute__((unused)) int static_assert_[(expr) ? 1 : -1]; \
	    } while(0)
#  endif

#  ifndef STORED_CLASS_NOCOPY
/*!
 * \def STORED_CLASS_NOCOPY
 * \brief Emits the copy/move constructor/assignment such that copy is not allowed.
 *
 * Move is allowed anyway.
 *
 * Put this macro inside the private section of your class.
 *
 * \param Class the class this macro is embedded in
 */
#    if STORED_cplusplus >= 201103L
#      define STORED_CLASS_NOCOPY(Class)                      \
      public:                                                 \
	      /*! \brief Deleted copy constructor. */         \
	      Class(Class const&) = delete;                   \
	      /*! \brief Default move constructor. */         \
	      Class(Class&&) noexcept = default; /* NOLINT */ \
	      /*! \brief Deleted assignment operator. */      \
	      void operator=(Class const&) = delete;          \
	      /*! \brief Default move assignment operator. */ \
	      Class& operator=(Class&&) noexcept = default; /* NOLINT */
#    else
#      define STORED_CLASS_NOCOPY(Class)                 \
      private:                                           \
	      /*! \brief Deleted copy constructor. */    \
	      Class(Class const&);                       \
	      /*! \brief Deleted assignment operator. */ \
	      void operator=(Class const&);
#    endif
#  endif

#  if STORED_cplusplus >= 201103L && !defined(STORED_CLASS_DEFAULT_COPY_MOVE)
#    define STORED_CLASS_DEFAULT_COPY_MOVE(type)        \
    public:                                             \
	    type(type const&) = default;                \
	    type(type&&) noexcept = default;            \
	    type& operator=(type const&) = default;     \
	    type& operator=(type&&) noexcept = default; \
                                                        \
    private:
#  endif

#  ifndef CLASS_NO_WEAK_VTABLE
/*!
 * \brief Macro to make sure that the containing class emits its vtable in one translation unit.
 * \details Use CLASS_NO_WEAK_VTABLE_DEF() in one .cpp file.
 */
#    define CLASS_NO_WEAK_VTABLE \
    protected:                   \
	    void force_to_translation_unit();
/*!
 * \see CLASS_NO_WEAK_VTABLE
 */
// cppcheck-suppress-macro duplInheritedMember
#    define CLASS_NO_WEAK_VTABLE_DEF(Class)                       \
	    /*! \brief Dummy function to force the vtable of this \
	     * class to this translation unit. Don't call. */     \
	    void Class::force_to_translation_unit()               \
	    {                                                     \
		    abort();                                      \
	    }
#  endif

namespace stored {

/*!
 * \brief Like \c assert(), but only emits code when #stored::Config::EnableAssert.
 */
#  ifdef STORED_HAVE_ZTH
#    define stored_assert(expr)                                 \
	    do { /* NOLINT(cppcoreguidelines-avoid-do-while) */ \
		    if(::stored::Config::EnableAssert) {        \
			    zth_assert(expr);                   \
		    }                                           \
	    } while(false)
#  else
#    define stored_assert(expr)                                 \
	    do { /* NOLINT(cppcoreguidelines-avoid-do-while) */ \
		    if(::stored::Config::EnableAssert) {        \
			    assert(expr);                       \
		    }                                           \
	    } while(false)
#  endif

void swap_endian(void* buffer, size_t len) noexcept;
void memcpy_swap(void* __restrict__ dst, void const* __restrict__ src, size_t len) noexcept;
int memcmp_swap(void const* a, void const* b, size_t len) noexcept;

template <size_t S>
static inline void swap_endian_(void* buffer) noexcept
{
	swap_endian(buffer, S);
}

#  ifdef STORED_COMPILER_GCC
template <>
inline void swap_endian_<2>(void* buffer) noexcept
{
	uint16_t* x = static_cast<uint16_t*>(buffer);
	*x = __builtin_bswap16(*x);
}

template <>
inline void swap_endian_<4>(void* buffer) noexcept
{
	uint32_t* x = static_cast<uint32_t*>(buffer);
	*x = __builtin_bswap32(*x);
}

template <>
inline void swap_endian_<8>(void* buffer) noexcept
{
	uint64_t* x = static_cast<uint64_t*>(buffer);
	*x = __builtin_bswap64(*x);
}
#  endif // STORED_COMPILER_GCC

#  ifdef STORED_COMPILER_MSVC
template <>
inline void swap_endian_<sizeof(unsigned short)>(void* buffer) noexcept
{
	unsigned short* x = static_cast<unsigned short*>(buffer);
	*x = _byteswap_ushort(*x);
}

template <>
inline void swap_endian_<sizeof(unsigned long)>(void* buffer) noexcept
{
	unsigned long* x = static_cast<unsigned long*>(buffer);
	*x = _byteswap_ulong(*x);
}

template <>
inline void swap_endian_<sizeof(unsigned __int64)>(void* buffer) noexcept
{
	unsigned __int64* x = static_cast<unsigned __int64*>(buffer);
	*x = _byteswap_uint64(*x);
}
#  endif // STORED_COMPILER_MSVC

/*!
 * \brief Swap endianness of the given value.
 */
template <typename T>
static inline T swap_endian(T value) noexcept
{
	swap_endian_<sizeof(T)>(&value);
	return value;
}

/*!
 * \brief Swap host to big endianness.
 */
template <typename T>
static inline T endian_h2b(T value) noexcept
{
#  ifdef STORED_LITTLE_ENDIAN
	swap_endian_<sizeof(T)>(&value);
#  endif
	return value;
}

/*!
 * \brief Swap host to network (big) endianness.
 */
template <typename T>
static inline T endian_h2n(T value) noexcept
{
	return endian_h2b<T>(value);
}

/*!
 * \brief Swap host to little endianness.
 */
template <typename T>
static inline T endian_h2l(T value) noexcept
{
#  ifdef STORED_BIG_ENDIAN
	swap_endian_<sizeof(T)>(&value);
#  endif
	return value;
}

/*!
 * \brief Swap host to store endianness.
 */
template <typename T>
static inline T endian_h2s(T value) noexcept
{
	if(Config::StoreInLittleEndian)
		return endian_h2l<T>(value);
	else
		return endian_h2b<T>(value);
}

/*!
 * \brief Swap big to host endianness.
 */
template <typename T>
static inline T endian_b2h(T value) noexcept
{
#  ifdef STORED_LITTLE_ENDIAN
	swap_endian_<sizeof(T)>(&value);
#  endif
	return value;
}

/*!
 * \brief Swap network (big) to host endianness.
 */
template <typename T>
static inline T endian_n2h(T value) noexcept
{
	return endian_b2h<T>(value);
}

/*!
 * \brief Swap little to host endianness.
 */
template <typename T>
static inline T endian_l2h(T value) noexcept
{
#  ifdef STORED_BIG_ENDIAN
	swap_endian_<sizeof(T)>(&value);
#  endif
	return value;
}

/*!
 * \brief Swap store to host endianness.
 */
template <typename T>
static inline T endian_s2h(T value) noexcept
{
	if(Config::StoreInLittleEndian)
		return endian_l2h<T>(value);
	else
		return endian_b2h<T>(value);
}

/*!
 * \brief Load from (possibly unaligned) buffer and swap little to host endianness.
 */
template <typename T, typename P>
static inline T endian_l2h(P const* p) noexcept
{
	T x;
	memcpy(&x, p, sizeof(T));
	return endian_l2h(x);
}

/*!
 * \brief Load from (possibly unaligned) buffer and swap big to host endianness.
 */
template <typename T, typename P>
static inline T endian_b2h(P const* p) noexcept
{
	T x;
	memcpy(&x, p, sizeof(T));
	return endian_b2h(x);
}

/*!
 * \brief Load from (possibly unaligned) buffer and swap network (big) to host endianness.
 */
template <typename T, typename P>
static inline T endian_n2h(P const* p) noexcept
{
	T x;
	memcpy(&x, p, sizeof(T));
	return endian_n2h(x);
}

/*!
 * \brief Load from (possibly unaligned) buffer and swap store to host endianness.
 */
template <typename T, typename P>
static inline T endian_s2h(P const* p) noexcept
{
	T x;
	memcpy(&x, p, sizeof(T));
	return endian_s2h(x);
}

/*!
 * \brief Determine the number of bytes to save the given unsigned value.
 */
template <uintmax_t N>
struct value_bytes {
	enum { value = value_bytes<(N >> 8U)>::value + 1U };
};
template <>
struct value_bytes<0> {
	enum { value = 0 };
};

/*!
 * \brief Determines a type that can hold the given unsigned value.
 */
template <uintmax_t N, int bytes = value_bytes<N>::value>
struct value_type {
	typedef uintmax_t type;
	typedef uintmax_t fast_type;
};
#  ifdef UINT_LEAST64_MAX
template <uintmax_t N>
struct value_type<N, 8> {
	typedef uint_least64_t type;
	typedef uint_fast64_t fast_type;
};
template <uintmax_t N>
struct value_type<N, 7> {
	typedef uint_least64_t type;
	typedef uint_fast64_t fast_type;
};
template <uintmax_t N>
struct value_type<N, 6> {
	typedef uint_least64_t type;
	typedef uint_fast64_t fast_type;
};
template <uintmax_t N>
struct value_type<N, 5> {
	typedef uint_least64_t type;
	typedef uint_fast64_t fast_type;
};
#  endif
template <uintmax_t N>
struct value_type<N, 4> {
	typedef uint_least32_t type;
	typedef uint_fast32_t fast_type;
};
template <uintmax_t N>
struct value_type<N, 3> {
	typedef uint_least32_t type;
	typedef uint_fast32_t fast_type;
};
template <uintmax_t N>
struct value_type<N, 2> {
	typedef uint_least16_t type;
	typedef uint_fast16_t fast_type;
};
template <uintmax_t N>
struct value_type<N, 1> {
	typedef uint_least8_t type;
	typedef uint_fast8_t fast_type;
};
template <uintmax_t N>
struct value_type<N, 0> {
	typedef uint8_t type;
	typedef uint8_t fast_type;
};

/*!
 * \private
 */
namespace impl {
template <typename T>
struct signedness_helper {
	typedef T signed_type;
	typedef T unsigned_type;
};
template <>
struct signedness_helper<short> {
	typedef short signed_type;
	typedef unsigned short unsigned_type;
};
template <>
struct signedness_helper<unsigned short> {
	typedef short signed_type;
	typedef unsigned short unsigned_type;
};
template <>
struct signedness_helper<int> {
	typedef int signed_type;
	typedef unsigned int unsigned_type;
};
template <>
struct signedness_helper<unsigned int> {
	typedef int signed_type;
	typedef unsigned int unsigned_type;
};
template <>
struct signedness_helper<long> {
	typedef long signed_type;
	typedef unsigned long unsigned_type;
};
template <>
struct signedness_helper<unsigned long> {
	typedef long signed_type;
	typedef unsigned long unsigned_type;
};
#  ifdef ULLONG_MAX
// long long only exist since C99 and C++11, but many compilers do support them anyway.
template <>
struct signedness_helper<long long> {
	typedef long long signed_type;
	typedef unsigned long long unsigned_type;
};
template <>
struct signedness_helper<unsigned long long> {
	typedef long long signed_type;
	typedef unsigned long long unsigned_type;
};
#  endif

template <typename R>
struct saturated_cast_helper {
	template <typename T>
	__attribute__((pure)) static R cast(T value) noexcept
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
				if(static_cast<typename signedness_helper<T>::signed_type>(value)
				   <= static_cast<typename signedness_helper<R>::signed_type>(
					   std::numeric_limits<R>::min()))
					return std::numeric_limits<R>::min();
			}
		} else {
#  ifdef STORED_COMPILER_MSVC
#    pragma warning(push)
#    pragma warning(disable : 4146) // This code path is not triggered
				    // in case of unsigned ints.
#  endif

#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Woverflow" // This error triggers when R is integer, but
					      // this code path is not triggered then.
#  ifdef STORED_COMPILER_CLANG
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wsign-conversion"
#  endif
			// NOLINTNEXTLINE(clang-diagnostic-sign-conversion,clang-diagnostic-constant-conversion)
			if(value <= -std::numeric_limits<R>::max())
				// NOLINTNEXTLINE(clang-diagnostic-sign-conversion,clang-diagnostic-constant-conversion)
				return -std::numeric_limits<R>::max();
#  ifdef STORED_COMPILER_CLANG
#    pragma clang diagnostic pop
#  endif
#  pragma GCC diagnostic pop

#  ifdef STORED_COMPILER_MSVC
#    pragma warning(pop)
#  endif
		}

		// Upper bound check
		if(value > 0)
			if(static_cast<typename signedness_helper<T>::unsigned_type>(value)
			   >= static_cast<typename signedness_helper<R>::unsigned_type>(
				   std::numeric_limits<R>::max()))
				return std::numeric_limits<R>::max();

		// Default conversion
		return static_cast<R>(value);
	}

	__attribute__((pure)) static R cast(float value) noexcept
	{
		return cast(llroundf(value));
	}
	__attribute__((pure)) static R cast(double value) noexcept
	{
		return cast(llround(value));
	}
	__attribute__((pure)) static R cast(long double value) noexcept
	{
		return cast(llroundl(value));
	}
	__attribute__((pure)) static R cast(bool value) noexcept
	{
		return static_cast<R>(value);
	}
	__attribute__((pure)) static R cast(R value) noexcept
	{
		return value;
	}
};

template <>
struct saturated_cast_helper<float> {
	template <typename T>
	constexpr static float cast(T value) noexcept
	{
		return static_cast<float>(value);
	}
};
template <>
struct saturated_cast_helper<double> {
	template <typename T>
	constexpr static double cast(T value) noexcept
	{
		return static_cast<double>(value);
	}
};
template <>
struct saturated_cast_helper<long double> {
	template <typename T>
	constexpr static long double cast(T value) noexcept
	{
		return static_cast<long double>(value);
	}
};
template <>
struct saturated_cast_helper<bool> {
	template <typename T>
	__attribute__((pure)) static bool cast(T value) noexcept
	{
		return static_cast<bool>(saturated_cast_helper<int>::cast(value));
	}
};
} // namespace impl

size_t strncpy(char* __restrict__ dst, char const* __restrict__ src, size_t len) noexcept;
int strncmp(
	char const* __restrict__ str1, size_t len1, char const* __restrict__ str2,
	size_t len2 = std::numeric_limits<size_t>::max()) noexcept;

char const* banner() noexcept;

template <typename T>
struct identity {
	typedef T self;
};

/*!
 * \brief Type constructor for (wrapped) store base class types.
 *
 * When deriving from the base store class, instantiate your class as follows:
 *
 * \code
 * class MyStore : public stored::store<MyStore, stored::MyStoreBase>::type {
 *	STORE_CLASS(MyStore, stored::MyStoreBase)
 * public:
 *	...
 * };
 * \endcode
 *
 * When wrappers are used, inject them in the sequence of inheritance:
 *
 * \code
 * class MyStore : public stored::store_t<MyStore, stored::Synchronizable, stored::MyStoreBase> {
 *	STORE_CLASS(MyStore, stored::Synchronizable, stored::MyStoreBase)
 * public:
 *	...
 * };
 * \endcode
 *
 * Note the usage of \c stored::stored_t, which is equivalent to \c
 * stored::store, but only available for C++11 and later.
 *
 * N.B. Although \c store_t is perfectly fine to use for C++11 and later,
 * doxygen and MSVC get confused about it. It does not seem to process the
 * inheritance properly. Try using \c STORE_T(...) instead of \c
 * stored::store<...>::type or \c stored::store_t<...>.
 */
template <
	typename Impl,
	// ad infinitum
	template <typename> class Wrapper7 = identity,
	template <typename> class Wrapper6 = identity,
	template <typename> class Wrapper5 = identity,
	template <typename> class Wrapper4 = identity,
	template <typename> class Wrapper3 = identity,
	template <typename> class Wrapper2 = identity,
	template <typename> class Wrapper1 = identity,
	template <typename> class Wrapper0 = identity, template <typename> class Base = identity>
struct store {
	typedef typename Wrapper7<typename Wrapper6<typename Wrapper5<
		typename Wrapper4<typename Wrapper3<typename Wrapper2<typename Wrapper1<
			typename Wrapper0<typename Base<Impl>::self>::self>::self>::self>::self>::
			self>::self>::self>::self type;
};

#  if STORED_cplusplus >= 201103L
/*!
 * \brief Convenience typedef for <tt>typename storeD::store<...>::type</tt>.
 */
template <typename Impl, template <typename> class... Base>
using store_t = typename store<Impl, Base...>::type;
#  endif

// There seem to be issues with the store type above. Both MSVC and Doxygen do not understand it.
// Use a macro as fallback.
// Make sure to match the number of template arguments of stored::store.
#  define STORE_T_1(Impl, x)	  x<Impl /**/>
#  define STORE_T_2(Impl, x, ...) x<EXPAND(STORE_T_1(Impl, __VA_ARGS__)) /**/>
#  define STORE_T_3(Impl, x, ...) x<EXPAND(STORE_T_2(Impl, __VA_ARGS__)) /**/>
#  define STORE_T_4(Impl, x, ...) x<EXPAND(STORE_T_3(Impl, __VA_ARGS__)) /**/>
#  define STORE_T_5(Impl, x, ...) x<EXPAND(STORE_T_4(Impl, __VA_ARGS__)) /**/>
#  define STORE_T_6(Impl, x, ...) x<EXPAND(STORE_T_5(Impl, __VA_ARGS__)) /**/>
#  define STORE_T_7(Impl, x, ...) x<EXPAND(STORE_T_6(Impl, __VA_ARGS__)) /**/>
#  define STORE_T_8(Impl, x, ...) x<EXPAND(STORE_T_7(Impl, __VA_ARGS__)) /**/>
#  define STORE_T_9(Impl, x, ...) x<EXPAND(STORE_T_8(Impl, __VA_ARGS__)) /**/>

#  define STORED_GET_MACRO_ARGN(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, m, ...) m

/*!
 * \brief Type constructor for (wrapped) store base class types.
 *
 * This is the macro version of \c stored::store_t and #stored::store.
 *
 * \hideinitializer
 */
#  define STORE_T(Impl, ...)                                                                  \
	  EXPAND(STORED_GET_MACRO_ARGN(                                                       \
		  Impl, ##__VA_ARGS__, STORE_T_9, STORE_T_8, STORE_T_7, STORE_T_6, STORE_T_5, \
		  STORE_T_4, STORE_T_3, STORE_T_2, STORE_T_1)(Impl, ##__VA_ARGS__))

// Make sure to match the number of template arguments of stored::store.
#  define STORE_BASE_CLASS_1(x)	     x
#  define STORE_BASE_CLASS_2(x, ...) EXPAND(STORE_BASE_CLASS_1(__VA_ARGS__))
#  define STORE_BASE_CLASS_3(x, ...) EXPAND(STORE_BASE_CLASS_2(__VA_ARGS__))
#  define STORE_BASE_CLASS_4(x, ...) EXPAND(STORE_BASE_CLASS_3(__VA_ARGS__))
#  define STORE_BASE_CLASS_5(x, ...) EXPAND(STORE_BASE_CLASS_4(__VA_ARGS__))
#  define STORE_BASE_CLASS_6(x, ...) EXPAND(STORE_BASE_CLASS_5(__VA_ARGS__))
#  define STORE_BASE_CLASS_7(x, ...) EXPAND(STORE_BASE_CLASS_6(__VA_ARGS__))
#  define STORE_BASE_CLASS_8(x, ...) EXPAND(STORE_BASE_CLASS_7(__VA_ARGS__))
#  define STORE_BASE_CLASS_9(x, ...) EXPAND(STORE_BASE_CLASS_8(__VA_ARGS__))

#  define STORE_CLASS_BASE(Impl, ...)                                                             \
	  EXPAND(STORED_GET_MACRO_ARGN(                                                           \
		  0, Impl, ##__VA_ARGS__, STORE_BASE_CLASS_9, STORE_BASE_CLASS_8,                 \
		  STORE_BASE_CLASS_7, STORE_BASE_CLASS_6, STORE_BASE_CLASS_5, STORE_BASE_CLASS_4, \
		  STORE_BASE_CLASS_3, STORE_BASE_CLASS_2,                                         \
		  STORE_BASE_CLASS_1)(Impl, ##__VA_ARGS__))<Impl /**/>

#  ifdef STORED_COMPILER_MSVC
// https://developercommunity.visualstudio.com/t/compile-error-when-using-using-declaration-referen/486683
// MSVC is always C++14 or later, so this is safe.
#    define STORE_CLASS_USING_BASE_TYPE(type, ...) using type = typename __VA_ARGS__::type;
#  else
#    define STORE_CLASS_USING_BASE_TYPE(type, ...) using typename __VA_ARGS__::type;
#  endif

#  define STORE_CLASS_(Impl, ...)                                  \
	  STORED_CLASS_NOCOPY(Impl)                                \
	  STORED_CLASS_NEW_DELETE(Impl)                            \
  public:                                                          \
	  typedef Impl self;                                       \
	  typedef __VA_ARGS__ base;                                \
	  STORE_CLASS_USING_BASE_TYPE(root, __VA_ARGS__)           \
	  STORE_CLASS_USING_BASE_TYPE(Implementation, __VA_ARGS__) \
                                                                   \
  private:

/*!
 * \brief Class helper macro to get a store implementation class right.
 * \see #stored::store
 * \hideinitializer
 */
#  define STORE_CLASS(Impl, ...)                         \
	  STORE_CLASS_(Impl, STORE_T(Impl, __VA_ARGS__)) \
	  friend class STORE_CLASS_BASE(Impl, ##__VA_ARGS__);

/*!
 * \brief Class helper macro to get a store wrapper class right.
 * \hideinitializer
 */
#  define STORE_WRAPPER_CLASS(Impl, Base) STORE_CLASS_(Impl, Base)

} // namespace stored

#  include <libstored/allocator.h>

namespace stored {
String::type string_literal(void const* buffer, size_t len, char const* prefix = nullptr);
} // namespace stored

/*!
 * \brief Converts a number type to another one, with proper rounding and saturation.
 */
template <typename R, typename T>
__attribute__((pure)) R saturated_cast(T value) noexcept
{
	return stored::impl::saturated_cast_helper<R>::cast(value);
}

template <
	typename Sub, typename Base
#  if STORED_cplusplus >= 201103L
	,
	typename std::enable_if<
		std::is_base_of<Base, typename std::remove_pointer<Sub>::type>::value, int>::type =
		0
#  endif
	>
Sub down_cast(Base* p) noexcept
{
#  ifdef STORED_cpp_rtti
	stored_assert(dynamic_cast<Sub>(p) != nullptr);
#  endif
	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
	return static_cast<Sub>(p);
}

template <
	typename Sub, typename Base
#  if STORED_cplusplus >= 201103L
	,
	typename std::enable_if<
		std::is_base_of<Base, typename std::remove_reference<Sub>::type>::value,
		int>::type = 0
#  endif
	>
Sub down_cast(Base& p) noexcept
{
#  ifdef STORED_cpp_rtti
	if(stored::Config::EnableAssert) {
		try {
			stored_assert(((void)dynamic_cast<Sub>(p), true)); // This may throw.
		} catch(...) {
			stored_assert(false); // NOLINT
		}
	}
#  endif
	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
	return static_cast<Sub>(p);
}

/*! \deprecated Use \c stored::store or \c STORE_T instead. */
#  define STORE_BASE_CLASS(Base, Impl) ::stored::Base</**/ Impl /**/>

/*! \deprecated Use \c STORE_CLASS instead. */
#  define STORE_CLASS_BODY(Base, Impl) STORE_CLASS(Impl, ::stored::Base)

#endif // __cplusplus
#endif // LIBSTORED_UTIL_H
