#ifndef __LIBSTORED_SPM_H
#define __LIBSTORED_SPM_H

#ifdef __cplusplus

#include <libstored/macros.h>
#include <libstored/util.h>

#include <list>
#include <new>

#ifdef STORED_HAVE_VALGRIND
#  include <valgrind/memcheck.h>
#endif

namespace stored {

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

		template <typename T>
		__attribute__((malloc,returns_nonnull,warn_unused_result))
		T* alloc(size_t count) {
			size_t size = count * sizeof(T);
			if(unlikely(size == 0))
				return reinterpret_cast<T*>(m_buffer);

			size_t const align = sizeof(void*);
			size = (size + align - 1) & ~(align - 1);
			reserve(size);

			char* p = m_buffer + m_size;
			m_size += size;
			m_total += size;

#ifdef STORED_HAVE_VALGRIND
			VALGRIND_MAKE_MEM_UNDEFINED(p, size);
#endif
			return reinterpret_cast<T*>(p);
		}

	private:
		char* m_buffer;
		size_t m_size;
		size_t m_capacity;
		size_t m_total;
		std::list<char*> m_old;
	};

} // namespace
#endif // __cplusplus
#endif // __LIBSTORED_SPM_H
