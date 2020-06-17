#ifndef __LIBSTORED_MACROS_H
#define __LIBSTORED_MACROS_H



//////////////////////////////////////////////////
// Preamble
//

#ifdef _DEBUG
#  undef NDEBUG
#endif
#if !defined(_DEBUG) && !defined(NDEBUG)
#  define _DEBUG
#endif





//////////////////////////////////////////////////
// Compiler
//

#ifdef __GNUC__
#  ifdef __clang__
// This is clang, which looks a lot like gcc
#    pragma clang diagnostic ignored "-Wunused-local-typedef"
#  else
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
#else
#  error Unsupported compiler. Please use gcc.
#endif



//////////////////////////////////////////////////
// Misc
//

#if defined(__cplusplus) && __cplusplus >= 201703L
#  define STORED_FALLTHROUGH [[fallthrough]];
#elif GCC_VERSION >= 70000L
#  define STORED_FALLTHROUGH __attribute__ ((fallthrough));
#else
#  define STORED_FALLTHROUGH
#endif


//////////////////////////////////////////////////
// Platform
//

#define __STDC_FORMAT_MACROS

#if defined(__BYTE_ORDER__)
#  if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#    define STORED_LITTLE_ENDIAN
#  else
#    define STORED_BIG_ENDIAN
#  endif
#else
#  error Unknown byte order
#endif

#if !defined(STORED_HAVE_VALGRIND) && defined(ZTH_HAVE_VALGRIND)
#  define STORED_HAVE_VALGRIND
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

#if defined(__cplusplus) && __cplusplus < 201103L
#  ifndef constexpr
#    define constexpr
#  endif
#  ifndef override
#    define override
#  endif
#  ifndef final
#    define final
#  endif
#  ifndef nullptr
#    define nullptr NULL
#  endif
#endif

#endif // __LIBSTORED_MACROS_H
