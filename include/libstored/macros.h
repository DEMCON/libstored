#ifndef __LIBSTORED_MACROS_H
#define __LIBSTORED_MACROS_H
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
#  endif
#  ifdef __cplusplus
#    if __cplusplus < 201103L && !defined(decltype)
#      define decltype(x) __typeof__(x) // Well, not really true when references are involved...
#    endif
#  endif
#  ifndef _GNU_SOURCE
#    define _GNU_SOURCE
#  endif
#  ifndef GCC_VERSION
#    define GCC_VERSION (__GNUC__ * 10000L + __GNUC_MINOR__ * 100L + __GNUC_PATCHLEVEL__)
#  endif
#  ifndef UNUSED_PAR
#    define UNUSED_PAR(name)	name __attribute__((unused))
#  endif
#elif defined(_MSC_VER)
#  define STORED_COMPILER_MSVC
#  ifndef UNUSED_PAR
#    ifdef __clang__
// That's odd. Probably clang-tidy.
#      define UNUSED_PAR(name)	name /* NOLINT(clang-diagnostic-unused-parameter,misc-unused-parameters) */
#    else
#      define UNUSED_PAR(name)	name
#   endif
#  endif
#  define NOMINMAX
#  define _USE_MATH_DEFINES
#  pragma warning(disable: 4061 4068 4100 4127 4200 4201 4296 4324 4355 4514 4571 4625 4626 4706 4710 4711 4774 4789 4820 5026 5027 5039 5045)
#  if _MSC_VER >= 1925
#    pragma warning(disable: 5204)
#  endif
#  include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#  define __attribute__(...)
#  ifndef __restrict__
#    define __restrict__ __restrict
#  endif
// MSVC always defines __cplusplus as 199711L, even though it actually compiles a different language version.
#  ifdef __cplusplus
#    undef STORED_cplusplus
#    define STORED_cplusplus _MSVC_LANG
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
#  define STORED_FALLTHROUGH __attribute__ ((fallthrough));
#else
#  define STORED_FALLTHROUGH
#endif


//////////////////////////////////////////////////
// Platform
//

#if defined(_WIN32) || defined(__CYGWIN__)
#  define STORED_OS_WINDOWS 1
#  define _WANT_IO_C99_FORMATS 1
#  define __USE_MINGW_ANSI_STDIO 1
#  if defined(UNICODE) || defined(_UNICODE)
#    error Do not use UNICODE. Use ANSI with UTF-8 instead.
#  endif
#  ifdef STORED_COMPILER_MSVC
#    pragma warning(push)
#    pragma warning(disable: 4668)
#  endif
#  ifdef STORED_HAVE_ZMQ
#    include <winsock2.h>
#  endif
#  include <Windows.h>
#  ifdef STORED_COMPILER_MSVC
#    pragma warning(pop)
#  endif
#elif defined(__linux__)
#  define STORED_OS_LINUX 1
#  define STORED_OS_POSIX 1
#elif defined(STORED_COMPILER_ARMCC)
#  define STORED_OS_BAREMETAL 1
#elif defined(__APPLE__)
#  define STORED_OS_OSX 1
#  define STORED_OS_POSIX 1
#else
#  define STORED_OS_GENERIC 1
#endif

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
#  define STORED_HAVE_VALGRIND
#endif

#ifdef CLANG_TIDY
#  undef STORED_HAVE_VALGRIND
#endif

#if defined(NDEBUG) && defined(STORED_HAVE_VALGRIND) && !defined(NVALGRIND)
#  define NVALGRIND
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

#if defined(STORED_cplusplus) && STORED_cplusplus < 201103L
#  ifndef STORED_COMPILER_MSVC
#    ifndef constexpr
#      define constexpr
#    endif
#    ifndef override
#      define override
#    endif
#    ifndef final
#      define final
#    endif
#    ifndef nullptr
#      define nullptr NULL
#    endif
#  endif
#endif

#endif // __LIBSTORED_MACROS_H
