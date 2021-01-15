#ifndef LIBSTORED_FIFO_H
#define LIBSTORED_FIFO_H
/*
 * libstored, distributed debuggable data stores.
 * Copyright (C) 2020-2021  Jochem Rutgers
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

#if STORED_cplusplus >= 201103L

#include <libstored/util.h>

#include <type_traits>
#include <atomic>
#include <cstring>
#include <new>
#include <iterator>
#include <array>
#include <vector>
#include <utility>

namespace stored {

	namespace impl {
		template <typename B, typename pointer>
		void buffer_set(B& buffer, pointer p, typename B::value_type const* x, size_t len) noexcept {
			if(std::is_trivially_copyable<typename B::value_type>::value)
				memcpy(&buffer[p], x, len);
			else
				for(pointer i = 0; i < (pointer)len; i++)
					buffer[p + i] = x[i];
		}

		template <typename B, typename pointer>
		void buffer_move(B& buffer, pointer dst, pointer src, size_t len) noexcept {
			if(std::is_trivially_copyable<typename B::value_type>::value)
				memmove(&buffer[dst], &buffer[src], len);
			else if(dst == src)
				;
			else if(dst < src)
				for(pointer i = 0; i < (pointer)len; i++)
					buffer[dst + i] = buffer[src + i];
			else
				for(pointer i = 1; i <= (pointer)len; i++)
					buffer[dst + len - i] = buffer[src + len - i];
		}
	}

	/*!
	 * \brief Generic buffer.
	 *
	 * This is essentially a wrapper around either \c std::array or
	 * \c std::vector with a few convenience functions and types.  Depending on
	 * \p Capacity, the buffer can resize (when \p Capacity is 0), or has a
	 * fixed size.
	 */
	template <typename T, size_t Capacity>
	class Buffer {
	public:
		typedef T type;

		typedef typename std::conditional<
			sizeof(typename value_type<Capacity>::type) < sizeof(uintptr_t),
			typename value_type<Capacity>::type,
			uintptr_t>
			::type pointer;

		size_t size() const noexcept { return m_buffer.size(); }
		void resize(size_t count) { (void)count; assert(count <= Capacity); }
		constexpr bool bounded() const noexcept { return true; }

		type const& operator[](pointer p) const noexcept { assert((size_t)p < size()); return m_buffer[p]; }
		type& operator[](pointer p) noexcept { assert((size_t)p < size()); return m_buffer[p]; }

		void set(pointer p, type const* x, size_t len) noexcept {
			assert(len <= size());
			assert(p + (pointer)len >= p);
			assert((size_t)p + len <= size());
			assert(x);

			impl::buffer_set(m_buffer, p, x, len);
		}

		void move(pointer dst, pointer src, size_t len) noexcept {
			assert(len <= size());
			assert(dst + (pointer)len >= dst);
			assert(src + (pointer)len >= src);
			assert((size_t)dst + len < size());
			assert((size_t)src + len < size());

			impl::buffer_move(m_buffer, dst, src, len);
		}

	private:
		std::array<type, Capacity> m_buffer;
	};

	template <typename T>
	class Buffer<T,0> {
	public:
		typedef T type;
		typedef size_t pointer;

		size_t size() const noexcept { return m_buffer.size(); }
		void resize(size_t count) { if(count > m_buffer.size()) m_buffer.resize(count); }
		constexpr bool bounded() const noexcept { return false; }

		type const& operator[](pointer p) const noexcept { assert((size_t)p < size()); return m_buffer[p]; }
		type& operator[](pointer p) noexcept { assert((size_t)p < size()); return m_buffer[p]; }

		void set(pointer p, type const* x, size_t len) noexcept {
			assert(p + len >= p);
			assert((size_t)p + len <= size());
			assert(x);

			impl::buffer_set(m_buffer, p, x, len);
		}

		void move(pointer dst, pointer src, size_t len) noexcept {
			assert(dst + (pointer)len >= dst);
			assert(src + (pointer)len >= src);
			assert((size_t)dst + len < size());
			assert((size_t)src + len < size());

			impl::buffer_move(m_buffer, dst, src, len);
		}

	private:
		std::vector<type> m_buffer;
	};

	/*!
	 * \brief LegacyInputIterator for FIFOs.
	 */
	template <typename Fifo>
	class PopIterator {
	public:
		typedef typename Fifo::type value_type;
		typedef value_type* pointer;
		typedef value_type& reference;
		typedef value_type const& const_reference;
		typedef std::input_iterator_tag iterator_category;

		constexpr PopIterator() noexcept = default;

		explicit constexpr PopIterator(Fifo& fifo) noexcept
			: m_fifo(&fifo), m_count(fifo.available())
		{}

		decltype(std::declval<Fifo>().front()) operator*() const noexcept {
			assert(m_fifo);
			return m_fifo->front();
		}

		PopIterator& operator++() noexcept {
			assert(m_fifo && m_count > 0);
			m_count--;
			m_fifo->pop_front();
			return *this;
		}

		class ValueWrapper {
		public:
			typedef PopIterator::value_type value_type;
			explicit constexpr ValueWrapper(value_type x) noexcept : m_x(x) {}
			constexpr value_type const& operator*() const noexcept { return m_x; }
		private:
			value_type m_x;
		};

		ValueWrapper operator++(int) noexcept { ValueWrapper v(**this); ++*this; return v; }

#ifdef NDEBUG
		constexpr
#endif
		bool operator==(PopIterator const& rhs) const noexcept {
#ifndef NDEBUG
			assert(!m_fifo || !rhs.m_fifo || m_fifo == rhs.m_fifo);
#endif
			return m_count == rhs.m_count;
		}

#ifdef NDEBUG
		constexpr
#endif
		bool operator!=(PopIterator const& rhs) const noexcept { return !(*this == rhs); }

	private:
		Fifo* m_fifo = nullptr;
		size_t m_count = 0;
	};

	/*!
	 * \brief FIFO that is optionally bounded in size and optionally thread-safe.
	 *
	 * This is a single-producer single-consumer FIFO.
	 *
	 * If bounded (\p Capacity is non-zero), it implements a circular buffer
	 * and does not do dynamic memory allocation during operation.  Then, it is
	 * thread-safe and async-signal safe. Therefore, it can be used to
	 * communicate between threads, but also by an interrupt handler.
	 *
	 * If unbounded, it cannot be thread-safe.
	 */
	template <typename T, size_t Capacity = 0, bool ThreadSafe = (Capacity > 0)>
	class Fifo {
		static_assert(!(ThreadSafe && Capacity == 0), "Unbounded tread-safe FIFOs are not supported");

		typedef Buffer<T,Capacity == 0 ? 0 : Capacity + 1> Buffer_type;
	public:
		typedef T type;
		typedef typename Buffer_type::pointer pointer;

		constexpr bool bounded() const noexcept {
			return m_buffer.bounded();
		}

		constexpr size_t capacity() const noexcept {
			return bounded() ? m_buffer.size() : std::numeric_limits<size_t>::max();
		}

		constexpr size_t size() const noexcept {
			return m_buffer.size();
		}

		bool empty() const noexcept {
			return m_wp.load(std::memory_order_relaxed) == m_rp.load(std::memory_order_relaxed);
		}

		bool full() const noexcept {
			return space() == 0;
		}

		size_t available() const noexcept {
			pointer wp = m_wp.load(std::memory_order_relaxed);
			pointer rp = m_rp.load(std::memory_order_relaxed);
			return wp >= rp ? wp - rp : wp + m_buffer.size() - rp;
		}

		size_t space() const noexcept {
			return capacity() - available() - 1;
		}

		type const& front() const noexcept {
			assert(!empty());
			pointer rp = m_rp.load(ThreadSafe ? std::memory_order_consume : std::memory_order_relaxed);
			return m_buffer[rp];
		}

		void pop_front() noexcept {
			pointer wp = m_wp.load(std::memory_order_relaxed);
			pointer rp = m_rp.load(std::memory_order_relaxed);
			assert(wp != rp);

			rp++;
			if(wp < rp && rp >= m_buffer.size())
				rp = (pointer)(rp - m_buffer.size());

			if(!bounded() && wp == rp) {
				// Reset to the start of the buffer, as it became empty.
				assert(!ThreadSafe);
				m_wp.store(0, std::memory_order_relaxed);
				m_rp.store(0, std::memory_order_relaxed);
			} else {
				m_rp.store(rp, std::memory_order_relaxed);
			}
		}

		void push_back(T const& x) {
			pointer wp;
			pointer wp_next;
			reserve_back(wp, wp_next);

			m_buffer[wp] = x;
			m_wp.store(wp_next, ThreadSafe ? std::memory_order_release : std::memory_order_relaxed);
		}

		template <typename... Arg>
		void emplace_back(Arg&&... arg) {
			pointer wp;
			pointer wp_next;
			reserve_back(wp, wp_next);

			m_buffer[wp].~type();
			new(&m_buffer[wp]) type(std::forward<Arg>(arg)...);
			m_wp.store(wp_next, ThreadSafe ? std::memory_order_release : std::memory_order_relaxed);
		}

		template <typename It>
		void push_back(It start, It end) {
			for(; start != end; ++start)
				push_back(*start);
		}

		void push_back(std::initializer_list<type> init) {
			for(auto const& x : init)
				push_back(x);
		}

		typedef PopIterator<Fifo> iterator;
		iterator begin() noexcept { return PopIterator<Fifo>(*this); }
		iterator end() noexcept { return PopIterator<Fifo>(); }

		void clear() {
			if(!bounded()) {
				assert(!ThreadSafe);
				m_wp.store(0, std::memory_order_relaxed);
				m_rp.store(0, std::memory_order_relaxed);
			} else {
				m_rp.store(m_wp.load(std::memory_order_relaxed), std::memory_order_relaxed);
			}
		}

	protected:
		enum {
			UnboundedMoveThreshold = 64,
		};

		void reserve_back(pointer& wp, pointer& wp_next) {
			assert(!full());

			wp = m_wp.load(std::memory_order_relaxed);
			pointer rp = m_rp.load(std::memory_order_relaxed);

			if(bounded()) {
				// Bounded buffer. Do wrap-around.
				wp_next = (pointer)(wp + 1u);
				if(wp_next >= m_buffer.size())
					wp_next = (pointer)(wp_next - m_buffer.size());
			} else {
				// Unbounded buffer.
				assert(!ThreadSafe);

				if(unlikely(rp > UnboundedMoveThreshold)) {
					// Large part of the start of the buffer is empty.
					// Move all data to prevent an infinitely growing FIFO, which is almost empty.
					m_buffer.move(0, rp, wp - rp);
					wp = (pointer)(wp - rp);
					rp = 0;
				}

				// Make sure the buffer can contain the new item.
				m_buffer.resize(wp + 1);
				wp_next = (pointer)(wp + 1);
			}
		}

	private:
		Buffer_type m_buffer;
		std::atomic<pointer> m_wp{0};
		std::atomic<pointer> m_rp{0};
	};

	struct Message {
		char const* message;
		size_t length;
	};

	/*!
	 * \brief FIFO for arbitrary-length messages.
	 *
	 * This is a single-producer single-consumer FIFO.
	 *
	 * The FIFO can be unbounded (\p Capacity is zero), which allows queuing
	 * any number and length of messages. However, it cannot be thread-safe in
	 * that case.
	 *
	 * If the FIFO is bounded (\p Capacity is set to the total buffer size for
	 * all queued messages), it can queue any number of messages (but limited
	 * by \p Messages) of any size (up to \p Capacity). Messages of different
	 * length can be used. It does not use dynamic memory allocation.  It can
	 * be configured to be thread-safe, which is also async-signal safe.
	 * Therefore, it can be used to pass messages between threads, but also
	 * from/to an interrupt handler.
	 */
#ifdef DOXYGEN
	// Simplify a bit
	template <size_t Capacity = 0, size_t Messages = 0, bool ThreadSafe = (Capacity > 0)>
#else
	template <size_t Capacity = 0, size_t Messages = Capacity == 0 ? 0 : std::max<size_t>(2, Capacity / sizeof(void*)), bool ThreadSafe = (Capacity > 0)>
#endif
	class MessageFifo {
		static_assert(!(ThreadSafe && Capacity == 0), "Unbounded tread-safe FIFOs are not supported");

		typedef Buffer<char, Capacity> Buffer_type;
		typedef typename Buffer_type::pointer buffer_pointer;
		typedef std::pair<buffer_pointer, buffer_pointer> Msg;
	public:
		typedef Message type;

		MessageFifo() = default;

		constexpr bool bounded() const noexcept { return Capacity > 0 || Messages > 0; }
		bool empty() const noexcept { return m_msg.empty(); }
		size_t available() const noexcept { return m_msg.available(); }
		constexpr size_t size() const noexcept { return m_buffer.size(); }

		Message front() const noexcept {
			Msg const& msg = m_msg.front();
			return Message{&m_buffer[msg.first], msg.second};
		}

		void pop_front() noexcept {
			Msg const& msg = m_msg.front();
			m_rp.store((buffer_pointer)(msg.first + msg.second), std::memory_order_relaxed);
			m_msg.pop_front();
		}

		bool push_back(char const* message, size_t length) {
			return push_back(Message{message, length});
		}

		bool push_back(Message const& message) {
			if(unlikely(Capacity > 0 && message.length > Capacity)) {
				// Will never fit.
#ifdef __cpp_exceptions
				throw std::bad_alloc();
#else
				std::abort();
#endif
			}

			if(m_msg.full())
				return false;

			buffer_pointer wp = m_wp.load(std::memory_order_relaxed);
			buffer_pointer rp = m_rp.load(std::memory_order_relaxed);

			bool e = empty();

			if(e) {
				// When empty, the other side ignores the wp/rp, so we
				// can safely tinker with it.
				wp = 0;
				rp = 0;
				m_rp.store(rp, std::memory_order_relaxed);
			}

			if(Capacity == 0) {
				// Make buffer larger.
				m_buffer.resize((size_t)wp + message.length);
			} else if(e) {
				// Empty, always fits.
			} else if(wp > rp) {
				// [rp,wp[ is in use.
				if((size_t)wp + message.length <= m_buffer.size()) {
					// Ok, fits in the buffer.
				} else if((size_t)rp >= message.length) {
					// The message fits at the start of the buffer.
					wp = 0;
				} else {
					// Does not fit.
					return false;
				}
			} else {
				// [0,wp[ and [rp,size()[ is in use.
				if((size_t)(rp - wp) >= message.length) {
					// Fits here.
				} else {
					// Does not fit.
					return false;
				}
			}

			// Write message content.
			buffer_pointer m = wp;
			m_buffer.set(m, message.message, message.length);
			m_wp.store((buffer_pointer)(wp + message.length), std::memory_order_relaxed);

			// Write message meta data.
			m_msg.emplace_back(m, (buffer_pointer)message.length);
			return true;
		}

		template <typename It>
		size_t push_back(It start, It end) {
			size_t cnt = 0;

			for(; start != end; ++start, ++cnt)
				if(!push_back(*start))
					return cnt;

			return cnt;
		}

		size_t push_back(std::initializer_list<type> init) {
			size_t cnt = 0;

			for(auto const& x : init) {
				if(!push_back(x))
					return cnt;
				++cnt;
			}

			return cnt;
		}

		typedef PopIterator<MessageFifo> iterator;
		iterator begin() noexcept { return PopIterator<MessageFifo>(*this); }
		iterator end() noexcept { return PopIterator<MessageFifo>(); }

		void clear() {
			m_rp.store(m_wp.load(std::memory_order_relaxed), std::memory_order_relaxed);
			m_msg.clear();
		}

	private:
		Buffer_type m_buffer;
		std::atomic<buffer_pointer> m_rp{0};
		std::atomic<buffer_pointer> m_wp{0};

		Fifo<Msg, Messages> m_msg;
	};

} // namespace

#endif // STORED_cplusplus
#endif // __cplusplus
#endif // LIBSTORED_FIFO_H
