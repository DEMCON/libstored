#ifndef LIBSTORED_MACROS_H
#define LIBSTORED_MACROS_H
// SPDX-FileCopyrightText: 2020-2024 Jochem Rutgers
//
// SPDX-License-Identifier: MPL-2.0



//////////////////////////////////////////////////
// Preamble

#ifdef STORED_HAVE_ZTH
#  include <libzth/macros.h>
#endif

#ifdef _DEBUG
#  undef NDEBUG
#endif
#if !defined(_DEBUG) && !defined(NDEBUG)
#  define _DEBUG
#endif

#ifdef __cplusplus
#  define STORED_cplusplus __cplusplus
#endif



//////////////////////////////////////////////////
// Compiler
//

#ifdef __GNUC__
#  ifdef __ARMCC_VERSION
#    define STORED_COMPILER_ARMCC
// This is ARMCC/Keil
#    pragma clang diagnostic ignored "-Wold-style-cast"
#    pragma clang diagnostic ignored "-Wused-but-marked-unused"
#    pragma clang diagnostic ignored "-Wpadded"
#    if STORED_cplusplus >= 201103L
#      pragma clang diagnostic ignored "-Wc++98-compat"
#    endif
#  elif defined(__clang__)
#    define STORED_COMPILER_CLANG
// This is clang, which looks a lot like gcc
#    pragma clang diagnostic ignored "-Wunused-local-typedef"
#  else
#    define STORED_COMPILER_GCC
// This is gcc
#    if defined(__MINGW32__) || defined(__MINGW64__)
#      define STORED_COMPILER_MINGW
#    endif
#  endif
#  ifdef __cplusplus
#    if __cplusplus < 201103L && !defined(decltype)
#      define decltype(x) \
	__typeof__(x) // Well, not really true when references are
		      // involved...
#    endif
#  endif
#  ifndef _GNU_SOURCE
#    define _GNU_SOURCE
#  endif
#  ifndef GCC_VERSION
#    define GCC_VERSION (__GNUC__ * 10000L + __GNUC_MINOR__ * 100L + __GNUC_PATCHLEVEL__)
#  endif
#  define STORED_DEPRECATED(msg) __attribute__((deprecated(msg)))
#elif defined(_MSC_VER)
#  define STORED_COMPILER_MSVC
// MSVC always defines __cplusplus as 199711L, even though it actually compiles a different language
// version.
#  ifdef __cplusplus
#    undef STORED_cplusplus
#    define STORED_cplusplus _MSVC_LANG
#  endif
#  if STORED_cplusplus >= 201402L && _MSC_VER < 1910
#    error Unsupported Visual Studio version. Upgrade to 2017, or do not compile for C++14.
// Because of this:
// https://devblogs.microsoft.com/cppblog/expression-sfinae-improvements-in-vs-2015-update-3/
//
// However, even with VS 2017, using constexpr calls in an std::enable_if_t
// template parameter still does not work similar to gcc. As a workaround,
// these enable_if_ts are wrapped in a decltype and used as return type or
// dummy parameter. Not very readable, but it works at least.
#  endif

#  define NOMINMAX
#  define _USE_MATH_DEFINES
#  pragma warning( \
	  disable : 4061 4068 4100 4127 4200 4201 4296 4324 4355 4459 4514 4571 4625 4626 4706 4710 4711 4774 4789 4820 5026 5027 5039 5045)
#  if _MSC_VER >= 1800
#    ifdef STORED_HAVE_QT
// Qt's moc generates relative paths.
#      pragma warning(disable : 4464)
#    endif
#  endif
#  if _MSC_FULL_VER >= 190023918
#    pragma warning(disable : 4868)
#  endif
#  if _MSC_VER >= 1915
#    pragma warning(disable : 4866 5105)
#  endif
#  if _MSC_VER >= 1925
#    pragma warning(disable : 5204)
#  endif
#  if _MSC_VER >= 1933
#    pragma warning(disable : 5262 5264)
#  endif
#  include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#  define __attribute__(...)
#  ifndef __restrict__
#    define __restrict__ __restrict
#  endif
#  define STORED_DEPRECATED(msg) __declspec(deprecated(msg))
#  define STORED_cpp_exceptions	 199711L
#  ifdef _CPPRTTI
#    define STORED_cpp_rtti 199711L
#  endif
#else
#  error Unsupported compiler.
#endif



//////////////////////////////////////////////////
// Misc
//

#if defined(STORED_cplusplus) && STORED_cplusplus >= 201703L
#  define STORED_FALLTHROUGH [[fallthrough]];
#elif defined(GCC_VERSION) && GCC_VERSION >= 70000L
#  define STORED_FALLTHROUGH __attribute__((fallthrough));
#else
#  define STORED_FALLTHROUGH
#endif



//////////////////////////////////////////////////
// Platform
//

#ifdef __ZEPHYR__
#  define STORED_OS_BAREMETAL 1
#  include <zephyr/toolchain.h>
// By default, turn off; picolibc does not provide it by default.
#  define STORED_NO_STDIO 1
#elif defined(_WIN32) || defined(__CYGWIN__)
#  define STORED_OS_WINDOWS	 1
#  define _WANT_IO_C99_FORMATS	 1
#  define __USE_MINGW_ANSI_STDIO 1
#  if defined(UNICODE) || defined(_UNICODE)
#    error Do not use UNICODE. Use ANSI with UTF-8 instead.
#  endif
#  ifndef WINVER
#    define WINVER 0x0501
#  endif
#  ifndef _WIN32_WINNT
#    define _WIN32_WINNT WINVER
#  endif
#  ifdef STORED_COMPILER_MSVC
#    pragma warning(disable : 5031 5032)
#    pragma warning(push)
#    pragma warning(disable : 4668)
#  endif
#  include <winsock2.h>
// Include order is important.
#  include <windows.h>
#  ifdef STORED_COMPILER_MSVC
#    pragma warning(pop)
#    pragma warning(default : 5031 5032)
#  endif
#elif defined(__linux__)
#  define STORED_OS_LINUX 1
#  define STORED_OS_POSIX 1
#  if defined(_DEBUG) && !defined(_FORTIFY_SOURCE)
#    if defined(STORED_COMPILER_GCC) && GCC_VERSION >= 120000
#      define _FORTIFY_SOURCE 3
#    else
#      define _FORTIFY_SOURCE 2
#    endif
#  endif
#elif defined(STORED_COMPILER_ARMCC)
#  define STORED_OS_BAREMETAL 1
#elif defined(__APPLE__)
#  define STORED_OS_OSX	  1
#  define STORED_OS_POSIX 1
#else
#  define STORED_OS_GENERIC 1
#endif

#ifdef STORED_HAVE_STDIO
#  ifdef STORED_NO_STDIO
#    undef STORED_NO_STDIO
#  endif
#endif

#ifndef STORED_HAVE_STDIO
#  ifndef STORED_OS_BAREMETAL
#    define STORED_HAVE_STDIO 1
#    ifdef STORED_NO_STDIO
#      undef STORED_NO_STDIO
#    endif
#  elif !defined(STORED_NO_STDIO)
#    define STORED_HAVE_STDIO 1
#  endif
#endif

// NOLINTNEXTLINE(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp)
#define __STDC_FORMAT_MACROS

#if defined(__BYTE_ORDER__)
#  if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#    define STORED_LITTLE_ENDIAN
#  else
#    define STORED_BIG_ENDIAN
#  endif
#elif defined(_M_IX86) || defined(_M_X64)
#  define STORED_LITTLE_ENDIAN
#else
#  error Unknown byte order
#endif

#if !defined(STORED_HAVE_VALGRIND) && defined(ZTH_HAVE_VALGRIND)
#  define STORED_HAVE_VALGRIND 1
#endif

#ifdef CLANG_TIDY
#  undef STORED_HAVE_VALGRIND
#endif

#if defined(NDEBUG) && defined(STORED_HAVE_VALGRIND) && !defined(NVALGRIND)
#  define NVALGRIND
#endif

#ifdef STORED_ENABLE_ASAN
#  ifdef STORED_COMPILER_CLANG
#    if !__has_feature(address_sanitizer)
#      undef STORED_ENABLE_ASAN
#    endif
#  elif defined(STORED_COMPILER_GCC)
#    ifndef __SANITIZE_ADDRESS__
#      undef STORED_ENABLE_ASAN
#    endif
#  else
#    undef STORED_ENABLE_ASAN
#  endif
#endif

#ifdef STORED_HAVE_QT
#  if STORED_HAVE_QT == 6
#    define STORED_HAVE_QT6
#    if STORED_cplusplus < 201703L
#      error Qt6 requires C++17 or later.
#    endif
#  elif STORED_HAVE_QT == 5
#    define STORED_HAVE_QT5
#    if STORED_cplusplus < 201103L
#      error Qt5 requires C++11 or later.
#    endif
#  else
#    error Unsupported QT version.
#  endif
#endif



//////////////////////////////////////////////////
// C/C++ version support
//

#ifndef EXTERN_C
#  ifdef __cplusplus
#    define EXTERN_C extern "C"
#  else
#    define EXTERN_C
#  endif
#endif

#if defined(STORED_cplusplus)
// All C++
#  if !defined(STORED_cpp_exceptions) && defined(__cpp_exceptions)
#    define STORED_cpp_exceptions __cpp_exceptions
#  endif
#  if !defined(STORED_cpp_exceptions)
#    define try	       if(true)
#    define catch(...) if(false)
#  endif

#  if !defined(STORED_cpp_rtti) && defined(__cpp_rtti)
#    define STORED_cpp_rtti __cpp_rtti
#  endif

#  if STORED_cplusplus < 201103L // < C++11
#    ifndef STORED_COMPILER_MSVC
#      ifndef constexpr
#	define constexpr inline
#      endif
#      ifndef override
#	define override
#      endif
#      ifndef final
#	define final
#      endif
#      ifndef nullptr
#	define nullptr NULL
#      endif
#      ifndef noexcept
#	define noexcept throw()
#      endif
#    endif
#    ifndef is_default
#      define is_default \
	{}
#    endif
#    ifndef constexpr14
#      define constexpr14 inline
#    endif
#    ifndef if_constexpr
#      define if_constexpr(...) if(__VA_ARGS__)
#    endif
#    ifndef thread_local
#      ifdef STORED_COMPILER_GCC
#	define thread_local __thread
#      elif !defined(STORED_COMPILER_MSVC)
#	define thread_local
#      endif
#    endif
#  else // C++11 or higher
#    ifndef is_default
#      define is_default = default;
#    endif
#    ifndef constexpr14
#      if STORED_cplusplus >= 201402L
#	define constexpr14 constexpr
#      else
#	define constexpr14 inline
#      endif
#    endif
#    ifndef if_constexpr
#      if STORED_cplusplus >= 201703L
#	define if_constexpr(...) if constexpr(__VA_ARGS__)
#      else
#	define if_constexpr(...) if(__VA_ARGS__)
#      endif
#    endif
#    ifndef inline17
#      if STORED_cplusplus >= 201703L
#	define inline17 inline
#      else
#	define inline17
#      endif
#    endif
#  endif
#endif

#ifndef STORED_thread_local
#  if defined(STORED_OS_BAREMETAL) || defined(STORED_OS_GENERIC)
#    define STORED_thread_local
#  else
#    define STORED_thread_local thread_local
#  endif
#endif

#if defined(STORED_cplusplus) && STORED_cplusplus >= 201402L
#  undef STORED_DEPRECATED
#  define STORED_DEPRECATED(msg) [[deprecated(msg)]]
#endif
#ifndef STORED_DEPRECATED
#  define STORED_DEPRECATED(msg)
#endif

#ifdef STORED_NO_DEPRECATED
#  undef STORED_DEPRECATED
#  define STORED_DEPRECATED(msg)
#endif

#endif // LIBSTORED_MACROS_H
