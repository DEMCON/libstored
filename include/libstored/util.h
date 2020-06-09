#ifndef __LIBSTORED_UTIL_H
#define __LIBSTORED_UTIL_H

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
#include <assert.h>

namespace stored {

#ifdef STORED_HAVE_ZTH
#  define stored_assert(expr)	do { if(::stored::Config::EnableAssert) zth_assert(expr); } while(false)
#else
#  define stored_assert(expr)	do { if(::stored::Config::EnableAssert) assert(expr); } while(false)
#endif

} // namespace
#endif // __cplusplus
#endif // __LIBSTORED_UTIL_H
