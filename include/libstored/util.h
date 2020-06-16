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
#include <cassert>
#include <math.h>
#include <limits>
#include <list>

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

#ifdef STORED_HAVE_VALGRIND
#  include <valgrind/memcheck.h>
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

	class ScratchPad {
	public:
		ScratchPad(size_t reserve = 0)
			: m_buffer()
			, m_size()
			, m_capacity()
			, m_total()
		{
			this->reserve(reserve);
		}

		~ScratchPad() {
			free(m_buffer);

			for(std::list<char*>::iterator it = m_old.begin(); it != m_old.end(); ++it)
				free(*it);
		}

		void reset() {
			if(likely(!m_total))
				return;

			if(unlikely(!m_old.empty())) {
				for(std::list<char*>::iterator it = m_old.begin(); it != m_old.end(); ++it)
					free(*it);

				m_old.clear();
				reserve(m_total);
			}

			m_size = 0;
			m_total = 0;

#ifdef STORED_HAVE_VALGRIND
			VALGRIND_MAKE_MEM_NOACCESS(&m_buffer[m_size], m_capacity - m_size);
#endif
		}

		bool empty() const {
			return m_total == 0;
		}

		size_t size() const {
			return m_total;
		}

		class Snapshot {
		public:
			Snapshot(ScratchPad& spm, void* buffer) : m_spm(&spm), m_buffer(buffer) {}
			~Snapshot() { if(m_spm) m_spm->rollback(m_buffer); }
			void reset() { m_spm = nullptr; }
		private:
			ScratchPad* m_spm;
			void* m_buffer;
		};
		friend class Snapshot;

		Snapshot snapshot() {
			return Snapshot(*this, empty() ? nullptr : &m_buffer[m_size]);
		}

private:
		void rollback(void* snapshot) {
			if(unlikely(!snapshot)) {
				reset();
			} else if(likely((uintptr_t)snapshot >= (uintptr_t)m_buffer && (uintptr_t)snapshot < (uintptr_t)&m_buffer[m_size])) {
				m_size = (size_t)((uintptr_t)snapshot - (uintptr_t)m_buffer);
#ifdef STORED_HAVE_VALGRIND
				VALGRIND_MAKE_MEM_NOACCESS(&m_buffer[m_size], m_capacity - m_size);
#endif
			}
		}

public:
		void reserve(size_t more) {
			size_t new_cap = m_size + more;

			if(likely(new_cap <= m_capacity))
				return;

			void* p;
			if(m_size == 0) {
				p = realloc(m_buffer, new_cap);
			} else {
				// realloc() may move the memory, which makes all previously alloced pointers
				// invalid. Push current buffer on m_old and alloc a new buffer.
				m_old.push_back(m_buffer);
				new_cap = more * 2 + sizeof(void*) * 8; // plus some extra reserve space
				p = malloc(new_cap);
				m_size = 0;
			}

			if(unlikely(!p))
				throw std::bad_alloc();

			m_buffer = (char*)p;
			m_capacity = new_cap;

#ifdef STORED_HAVE_VALGRIND
			VALGRIND_MAKE_MEM_NOACCESS(m_buffer, m_capacity);
#endif
		}

		void shrink_to_fit() {
			// Don't realloc to 0; behavior is implementation-defined.
			if(m_capacity <= 1)
				return;

			void* p = realloc(m_buffer, m_size);

			if(unlikely(!p))
				throw std::bad_alloc();

			m_buffer = (char*)p;
			m_capacity = m_size;
		}

		__attribute__((malloc,returns_nonnull,warn_unused_result))
		void* alloc(size_t size) {
			if(unlikely(size == 0))
				return m_buffer;

			size_t const align = sizeof(void*);
			size = (size + align - 1) & ~(align - 1);
			reserve(size);

			char* p = m_buffer + m_size;
			m_size += size;
			m_total += size;

#ifdef STORED_HAVE_VALGRIND
			VALGRIND_MAKE_MEM_UNDEFINED(p, size);
#endif
			return p;
		}

	private:
		char* m_buffer;
		size_t m_size;
		size_t m_capacity;
		size_t m_total;
		std::list<char*> m_old;
	};
} // namespace

template <typename R, typename T>
__attribute__((pure)) R saturated_cast(T value) { return stored::impl::saturated_cast_helper<R>::cast(value); }

#endif // __cplusplus
#endif // __LIBSTORED_UTIL_H
