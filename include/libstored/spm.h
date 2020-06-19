#ifndef __LIBSTORED_SPM_H
#define __LIBSTORED_SPM_H
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

#ifdef __cplusplus

#include <libstored/macros.h>
#include <libstored/util.h>

#include <list>
#include <new>

#ifdef STORED_HAVE_VALGRIND
#  include <valgrind/memcheck.h>
#endif

namespace stored {

	/*!
	 * \ingroup libstored_util
	 */
	class ScratchPad {
		CLASS_NOCOPY(ScratchPad)
	public:
		explicit ScratchPad(size_t reserve = 0)
			: m_buffer()
			, m_size()
			, m_capacity()
			, m_total()
		{
			this->reserve(reserve);
		}

		~ScratchPad() {
			// NOLINTNEXTLINE(cppcoreguidelines-owning-memory,cppcoreguidelines-no-malloc)
			free(m_buffer);

			for(std::list<char*>::iterator it = m_old.begin(); it != m_old.end(); ++it)
				free(*it); // NOLINT(cppcoreguidelines-owning-memory,cppcoreguidelines-no-malloc)
		}

		void reset() {
			if(likely(!m_total))
				return;

			if(unlikely(!m_old.empty())) {
				for(std::list<char*>::iterator it = m_old.begin(); it != m_old.end(); ++it)
					free(*it); // NOLINT(cppcoreguidelines-owning-memory,cppcoreguidelines-no-malloc)

				m_old.clear();
				reserve(m_total);
			}

			m_size = 0;
			m_total = 0;

#ifdef STORED_HAVE_VALGRIND
			// NOLINTNEXTLINE(cppcoreguidelines-pro-type-cstyle-cast,hicpp-no-assembler)
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
		protected:
			friend class ScratchPad;
			Snapshot(ScratchPad& spm, void* buffer) : m_spm(&spm), m_buffer(buffer) {}
		public:
			~Snapshot() { if(m_spm) m_spm->rollback(m_buffer); }
			void reset() { m_spm = nullptr; }

#if __cplusplus >= 201103L
			Snapshot(Snapshot&& s) noexcept : m_spm(s.m_spm), m_buffer(s.m_buffer) { s.reset(); }
			Snapshot& operator=(Snapshot&& s) noexcept {
				reset();
				m_spm = s.m_spm;
				m_buffer = s.m_buffer;
				s.reset();
				return *this;
			}
#endif
			Snapshot(Snapshot const& s) : m_spm(s.m_spm), m_buffer(s.m_buffer) { s.reset(); }

		private:
			void reset() const { m_spm = nullptr; }

#if __cplusplus >= 201103L
		public:
			Snapshot& operator=(Snapshot const& s) = delete;
#else
		private:
			Snapshot& operator=(Snapshot const& s);
#endif

		private:
			mutable ScratchPad* m_spm;
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
			} else if(likely(static_cast<char*>(snapshot) >= m_buffer
				&& static_cast<char*>(snapshot) < &m_buffer[m_size]))
			{
				m_size = (size_t)(static_cast<char*>(snapshot) - static_cast<char*>(m_buffer));
#ifdef STORED_HAVE_VALGRIND
				// NOLINTNEXTLINE(cppcoreguidelines-pro-type-cstyle-cast,hicpp-no-assembler)
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
				// clang-analyzer-unix.API: clang-tidy thinks new_cap can be 0, but that's not true.
				// NOLINTNEXTLINE(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory,clang-analyzer-unix.API)
				p = realloc(m_buffer, new_cap);
			} else {
				// realloc() may move the memory, which makes all previously alloced pointers
				// invalid. Push current buffer on m_old and alloc a new buffer.
				m_old.push_back(m_buffer);
				new_cap = more * 2 + sizeof(void*) * 8; // plus some extra reserve space
				// NOLINTNEXTLINE(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory)
				p = malloc(new_cap);
				m_size = 0;
			}

			if(unlikely(!p))
				throw std::bad_alloc();

			m_buffer = static_cast<char*>(p);
			m_capacity = new_cap;

#ifdef STORED_HAVE_VALGRIND
			// NOLINTNEXTLINE(cppcoreguidelines-pro-type-cstyle-cast,hicpp-no-assembler)
			VALGRIND_MAKE_MEM_NOACCESS(m_buffer, m_capacity);
#endif
		}

		void shrink_to_fit() {
			// Don't realloc to 0; behavior is implementation-defined.
			if(m_capacity <= 1)
				return;

			// NOLINTNEXTLINE(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory)
			void* p = realloc(m_buffer, m_size);

			if(unlikely(!p))
				throw std::bad_alloc();

			m_buffer = static_cast<char*>(p);
			m_capacity = m_size;
		}

		template <typename T>
		__attribute__((malloc,returns_nonnull,warn_unused_result))
		T* alloc(size_t count) {
			size_t size = count * sizeof(T);
			if(unlikely(size == 0))
				// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
				return reinterpret_cast<T*>(m_buffer);

			size_t const align = sizeof(void*);
			size = (size + align - 1) & ~(align - 1);
			reserve(size);

			char* p = m_buffer + m_size;
			m_size += size;
			m_total += size;

#ifdef STORED_HAVE_VALGRIND
			// NOLINTNEXTLINE(cppcoreguidelines-pro-type-cstyle-cast,hicpp-no-assembler)
			VALGRIND_MAKE_MEM_UNDEFINED(p, size);
#endif
			// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
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
