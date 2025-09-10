#ifndef LIBSTORED_FIFO_H
#define LIBSTORED_FIFO_H
// SPDX-FileCopyrightText: 2020-2025 Jochem Rutgers
//
// SPDX-License-Identifier: MPL-2.0

#ifdef __cplusplus

#  include <libstored/macros.h>

#  if STORED_cplusplus >= 201103L

#    include <libstored/protocol.h>
#    include <libstored/util.h>

#    include <array>
#    include <atomic>
#    include <cstring>
#    include <functional>
#    include <iterator>
#    include <new>
#    include <string>
#    include <type_traits>
#    include <utility>
#    if STORED_cplusplus >= 201703L
#      include <string_view>
#    endif

namespace stored {

namespace impl {

template <
	typename B, typename pointer,
	bool trivial = std::is_trivially_copyable<typename B::value_type>::value>
struct buffer_ops {
	static void set(B& buffer, pointer p, typename B::value_type const* x, size_t len) noexcept
	{
		if(len)
			memcpy(&buffer[p], x, len);
	}

	static void move(B& buffer, pointer dst, pointer src, size_t len) noexcept
	{
		if(len)
			memmove(&buffer[dst], &buffer[src], len);
	}
};

template <typename B, typename pointer>
struct buffer_ops<B, pointer, false> {
	static void set(B& buffer, pointer p, typename B::value_type const* x, size_t len) noexcept
	{
		for(pointer i = 0; i < (pointer)len; i++)
			buffer[p + i] = x[i];
	}

	static void move(B& buffer, pointer dst, pointer src, size_t len) noexcept
	{
		if(dst == src)
			;
		else if(dst < src)
			for(pointer i = 0; i < (pointer)len; i++)
				buffer[(size_t)(dst + i)] = buffer[(size_t)(src + i)];
		else
			for(pointer i = 1; i <= (pointer)len; i++)
				buffer[(size_t)(dst + len - i)] = buffer[(size_t)(src + len - i)];
	}
};

template <typename B, typename pointer>
void buffer_set(B& buffer, pointer p, typename B::value_type const* x, size_t len) noexcept
{
	buffer_ops<B, pointer>::set(buffer, p, x, len);
}

template <typename B, typename pointer>
void buffer_move(B& buffer, pointer dst, pointer src, size_t len) noexcept
{
	buffer_ops<B, pointer>::move(buffer, dst, src, len);
}
} // namespace impl

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
		typename value_type<Capacity>::type, uintptr_t>::type pointer;

	constexpr size_t size() const noexcept
	{
		return m_buffer.size();
	}

	void resize(size_t count)
	{
		(void)count;
		stored_assert(count <= Capacity);
	}

	constexpr bool bounded() const noexcept
	{
		return true;
	}

	type const& operator[](pointer p) const noexcept
	{
		stored_assert((size_t)p < size());
		return m_buffer[p];
	}

	type& operator[](pointer p) noexcept
	{
		stored_assert((size_t)p < size());
		return m_buffer[p];
	}

	void set(pointer p, type const* x, size_t len) noexcept
	{
		stored_assert(len <= size());
		stored_assert(p + (pointer)len >= p);
		stored_assert((size_t)p + len <= size());
		stored_assert(x);

		impl::buffer_set(m_buffer, p, x, len);
	}

	void move(pointer dst, pointer src, size_t len) noexcept
	{
		stored_assert(len <= size());
		stored_assert(dst + (pointer)len >= dst);
		stored_assert(src + (pointer)len >= src);
		stored_assert((size_t)dst + len <= size());
		stored_assert((size_t)src + len <= size());

		impl::buffer_move(m_buffer, dst, src, len);
	}

private:
	std::array<type, Capacity> m_buffer;
};

template <typename T>
class Buffer<T, 0> {
public:
	typedef T type;
	typedef size_t pointer;

	size_t size() const noexcept
	{
		return m_buffer.size();
	}

	void resize(size_t count)
	{
		if(count > m_buffer.size())
			m_buffer.resize(count);
	}

	constexpr bool bounded() const noexcept
	{
		return false;
	}

	type const& operator[](pointer p) const noexcept
	{
		stored_assert((size_t)p < size());
		return m_buffer[p];
	}

	type& operator[](pointer p) noexcept
	{
		stored_assert((size_t)p < size());
		return m_buffer[p];
	}

	void set(pointer p, type const* x, size_t len) noexcept
	{
		stored_assert(p + len >= p);
		stored_assert((size_t)p + len <= size());
		stored_assert(x);

		impl::buffer_set(m_buffer, p, x, len);
	}

	void move(pointer dst, pointer src, size_t len) noexcept
	{
		stored_assert(dst + (pointer)len >= dst);
		stored_assert(src + (pointer)len >= src);
		stored_assert((size_t)dst + len < size());
		stored_assert((size_t)src + len < size());

		impl::buffer_move(m_buffer, dst, src, len);
	}

private:
	typename Vector<type>::type m_buffer;
};

/*!
 * \brief Read-only view on a Buffer.
 *
 * It supports wrap-arounds, such that the Fifo can also provide a view on its valid contents.
 */
template <typename B>
class BufferView {
public:
	using Buffer = typename std::decay<B>::type;
	using type = typename Buffer::type;
	using pointer = typename Buffer::pointer;

	BufferView(Buffer const& b, pointer from, pointer to)
		: m_b{&b}
		, m_from{from}
		, m_to{to}
	{}

	size_t size() const
	{
		if(!m_b)
			return 0;

		return m_from <= m_to ? m_to - m_from : m_b->size() - m_from + m_to;
	}

	bool empty() const
	{
		return m_from == m_to;
	}

	BufferView subview(size_t offset, size_t len) const
	{
		return BufferView{*m_b, absolute(offset), absolute(offset + len)};
	}

	BufferView subview(size_t offset) const
	{
		return BufferView{*m_b, absolute(offset), m_to};
	}

	void lstrip(size_t amount)
	{
		stored_assert(amount <= size());
		m_from = absolute((pointer)amount);
	}

	void rstrip(size_t amount)
	{
		stored_assert(amount <= size());
		m_to = absolute(size() - (pointer)amount);
	}

	type const& operator[](size_t i) const
	{
		stored_assert(m_b);
		return (*m_b)[absolute((pointer)i)];
	}

	void copy(type* dst) const
	{
		if(m_from == m_to) {
			// Done.
		} else if(m_from < m_to) {
			pointer len = m_to - m_from;
			type const* src = &(*m_b)[m_from];
			for(pointer i = 0; i < len; i++)
				dst[i] = src[i];
		} else {
			copy2(dst);
		}
	}

	type const* contiguous(void* scratchpad) const
	{
		auto* sp = static_cast<type*>(scratchpad);

		if(m_from == m_to) {
			// Null-range. Don't dereference.
			return sp;
		} else if(m_from < m_to) {
			// No copy required.
			return &(*m_b)[m_from];
		} else {
			copy2(sp);
			return sp;
		}
	}

	class Iterator {
	protected:
		Iterator(BufferView const& b, BufferView::pointer i = 0)
			: m_b{&b}
			, m_i{i}
		{}

	public:
		void operator++()
		{
			++m_i;
		}

		BufferView::type const& operator*() const
		{
			stored_assert(m_b);
			return (*m_b)[m_i];
		}

		bool operator!=(Iterator const& rhs) const
		{
			return m_i != rhs.m_i || m_b != rhs.m_b;
		}

	private:
		BufferView const* m_b{};
		BufferView::pointer m_i{};

		friend class BufferView;
	};

	Iterator begin() const
	{
		return Iterator(*this);
	}

	Iterator end() const
	{
		return Iterator(*this, (pointer)size());
	}

protected:
	pointer absolute(pointer relative) const
	{
		stored_assert(relative <= size());

		if(m_from <= m_to)
			return m_from + relative;

		auto s = (pointer)m_b->size();
		auto c = s - m_from;
		return relative < c ? m_from + relative : (pointer)(relative - c);
	}

	void copy2(type* dst) const
	{
		pointer sz = (pointer)m_b->size();
		stored_assert(sz > 0 && m_to < m_from && m_b);

		pointer len0 = sz - m_from;
		pointer len1 = m_to;
		type const* src0 = &(*m_b)[m_from];
		type const* src1 = &(*m_b)[0];

		for(pointer i = 0; i < len0; i++)
			dst[i] = src0[i];
		for(pointer i = 0; i < len1; i++)
			dst[i + len0] = src1[i];
	}

private:
	Buffer const* m_b;
	pointer m_from;
	pointer m_to;
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

	constexpr explicit PopIterator(Fifo& fifo) noexcept
		: m_fifo(&fifo)
		, m_count(fifo.available())
	{}

	decltype(std::declval<Fifo>().front()) operator*() const noexcept
	{
		stored_assert(m_fifo);
		return m_fifo->front();
	}

	PopIterator& operator++() noexcept
	{
		stored_assert(m_fifo && m_count > 0);
		m_count--;
		m_fifo->pop_front();
		return *this;
	}

	class ValueWrapper {
	public:
		typedef PopIterator::value_type value_type;
		constexpr explicit ValueWrapper(value_type x) noexcept
			: m_x(x)
		{}
		constexpr value_type const& operator*() const noexcept
		{
			return m_x;
		}

	private:
		value_type m_x;
	};

	ValueWrapper operator++(int) noexcept
	{
		ValueWrapper v(**this);
		++*this;
		return v;
	}

#    ifdef NDEBUG
	constexpr
#    endif
		bool
		operator==(PopIterator const& rhs) const noexcept
	{
#    ifndef NDEBUG
		stored_assert(!m_fifo || !rhs.m_fifo || m_fifo == rhs.m_fifo);
#    endif
		return m_count == rhs.m_count;
	}

#    ifdef NDEBUG
	constexpr
#    endif
		bool
		operator!=(PopIterator const& rhs) const noexcept
	{
		return !(*this == rhs);
	}

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
	static_assert(
		!(ThreadSafe && Capacity == 0), "Unbounded tread-safe FIFOs are not supported");

	typedef Buffer<T, Capacity == 0 ? 0 : Capacity + 1> Buffer_type;

public:
	typedef T type;
	typedef typename Buffer_type::pointer pointer;

	constexpr bool bounded() const noexcept
	{
		return m_buffer.bounded();
	}

	constexpr size_t capacity() const noexcept
	{
		return bounded() ? m_buffer.size() : std::numeric_limits<size_t>::max();
	}

	constexpr size_t size() const noexcept
	{
		return m_buffer.size();
	}

	bool empty() const noexcept
	{
		return m_wp.load(std::memory_order_relaxed) == m_rp.load(std::memory_order_relaxed);
	}

	bool full() const noexcept
	{
		return space() == 0;
	}

	size_t available() const noexcept
	{
		pointer wp = m_wp.load(std::memory_order_relaxed);
		pointer rp = m_rp.load(std::memory_order_relaxed);
		return wp >= rp ? wp - rp : wp + m_buffer.size() - rp;
	}

	size_t available_chunk() const noexcept
	{
		pointer wp = m_wp.load(std::memory_order_relaxed);
		pointer rp = m_rp.load(std::memory_order_relaxed);
		return wp >= rp ? wp - rp : m_buffer.size() - rp;
	}

	size_t space() const noexcept
	{
		return capacity() - available() - 1;
	}

	type const& front() const noexcept
	{
		stored_assert(!empty());
		pointer rp = m_rp.load(
			ThreadSafe ? std::memory_order_consume : std::memory_order_relaxed);
		return m_buffer[rp];
	}

	type& front() noexcept
	{
		stored_assert(!empty());
		pointer rp = m_rp.load(
			ThreadSafe ? std::memory_order_consume : std::memory_order_relaxed);
		return m_buffer[rp];
	}

	type const& peek(size_t offset) const noexcept
	{
		stored_assert(offset < available());
		pointer rp = m_rp.load(
			ThreadSafe ? std::memory_order_consume : std::memory_order_relaxed);
		return m_buffer[(pointer)((rp + offset) % m_buffer.size())];
	}

	type& peek(size_t offset) noexcept
	{
		stored_assert(offset < available());
		pointer rp = m_rp.load(
			ThreadSafe ? std::memory_order_consume : std::memory_order_relaxed);
		return m_buffer[(pointer)((rp + offset) % m_buffer.size())];
	}

	type const& operator[](size_t offset) const noexcept
	{
		return peek(offset);
	}

	type& operator[](size_t offset) noexcept
	{
		return peek(offset);
	}

	void pop_front(size_t count = 1) noexcept
	{
		pointer wp = m_wp.load(std::memory_order_relaxed);
		pointer rp = m_rp.load(std::memory_order_relaxed);
		stored_assert(count <= available());

		rp = (pointer)(rp + count);
		if(wp < rp && rp >= m_buffer.size())
			rp = (pointer)(rp - m_buffer.size());

		if(!bounded() && wp == rp) {
			// Reset to the start of the buffer, as it became empty.
			stored_assert(!ThreadSafe);
			m_wp.store(0, std::memory_order_relaxed);
			m_rp.store(0, std::memory_order_relaxed);
		} else {
			m_rp.store(rp, std::memory_order_relaxed);
		}
	}

	void push_back(T const& x)
	{
		pointer wp;
		pointer wp_next;
		reserve_back(wp, wp_next);

		m_buffer[wp] = x;
		m_wp.store(
			wp_next,
			ThreadSafe ? std::memory_order_release : std::memory_order_relaxed);
	}

	template <typename... Arg>
	void emplace_back(Arg&&... arg)
	{
		pointer wp;
		pointer wp_next;
		reserve_back(wp, wp_next);

		m_buffer[wp].~type();
		new(&m_buffer[wp]) type(std::forward<Arg>(arg)...);
		m_wp.store(
			wp_next,
			ThreadSafe ? std::memory_order_release : std::memory_order_relaxed);
	}

	template <typename It>
	void push_back(It start, It end)
	{
		for(; start != end; ++start)
			push_back(*start);
	}

	void push_back(std::initializer_list<type> init)
	{
		pointer wp;
		pointer wp_next;
		reserve_back(wp, wp_next, init.size());

		for(auto const& x : init) {
			m_buffer[wp] = x;

			if(bounded())
				wp = (pointer)((wp + 1U) % m_buffer.size());
			else
				wp++;
		}

		stored_assert(wp == wp_next);

		m_wp.store(
			wp_next,
			ThreadSafe ? std::memory_order_release : std::memory_order_relaxed);
	}

	void push_back(type const* value, size_t len)
	{
		stored_assert(len == 0 || value);

		pointer wp;
		pointer wp_next;
		reserve_back(wp, wp_next, len);

		size_t size = m_buffer.size();

		while(len--) {
			m_buffer[wp++] = *value++;

			if(bounded() && wp == size)
				wp = 0;
		}

		stored_assert(wp == wp_next);

		m_wp.store(
			wp_next,
			ThreadSafe ? std::memory_order_release : std::memory_order_relaxed);
	}

	typedef PopIterator<Fifo> iterator;
	constexpr iterator begin() noexcept
	{
		return PopIterator<Fifo>(*this);
	}

	constexpr iterator end() noexcept
	{
		return PopIterator<Fifo>();
	}

	void clear()
	{
		if(!bounded()) {
			stored_assert(!ThreadSafe);
			m_wp.store(0, std::memory_order_relaxed);
			m_rp.store(0, std::memory_order_relaxed);
		} else {
			m_rp.store(m_wp.load(std::memory_order_relaxed), std::memory_order_relaxed);
		}
	}

	typedef BufferView<Buffer_type> View;

	View view() const
	{
		pointer rp = m_rp.load(
			ThreadSafe ? std::memory_order_consume : std::memory_order_relaxed);
		pointer wp = m_wp.load(std::memory_order_relaxed);
		return View{m_buffer, rp, wp};
	}

	operator View() const
	{
		return view();
	}

protected:
	enum {
		UnboundedMoveThreshold = 64,
	};

	void reserve_back(pointer& wp, pointer& wp_next, size_t count = 1)
	{
		stored_assert(space() >= count);

		wp = m_wp.load(std::memory_order_relaxed);
		pointer rp = m_rp.load(std::memory_order_relaxed);

		if(bounded()) {
			// Bounded buffer. Do wrap-around.
			wp_next = (pointer)(wp + count);
			if(wp_next >= m_buffer.size())
				wp_next = (pointer)(wp_next - m_buffer.size());
		} else {
			// Unbounded buffer.
			stored_assert(!ThreadSafe);

			if(unlikely(rp > UnboundedMoveThreshold)) {
				// Large part of the start of the buffer is empty.
				// Move all data to prevent an infinitely growing FIFO, which is
				// almost empty.
				m_buffer.move(0, rp, (size_t)(wp - rp));
				wp = (pointer)(wp - rp);
				m_wp.store(wp, std::memory_order_relaxed);
				m_rp.store(0, std::memory_order_relaxed);
			}

			// Make sure the buffer can contain the new item(s).
			m_buffer.resize((size_t)(wp + count));
			wp_next = (pointer)(wp + count);
		}
	}

private:
	Buffer_type m_buffer;
	std::atomic<pointer> m_wp{0};
	std::atomic<pointer> m_rp{0};
};

/*!
 * \brief Kind of modifiable C++17's std::string_view on a message within the MessageFifo.
 */
class Message {
public:
	constexpr Message(char* message, size_t length) noexcept
		: m_message(message)
		, m_length(length)
	{}

	constexpr char* data() const noexcept
	{
		return m_message;
	}

	constexpr size_t size() const noexcept
	{
		return m_length;
	}

#    if STORED_cplusplus >= 201703L
	constexpr operator std::string_view() const noexcept
	{
		return std::string_view(data(), size());
	}
#    endif

private:
	char* m_message;
	size_t m_length;
};

#    if STORED_cplusplus < 201703L
/*!
 * \brief A minimal C++17's std::string_view for MessageFifo.
 */
class MessageView {
public:
	constexpr MessageView(char const* message, size_t length) noexcept
		: m_message(message)
		, m_length(length)
	{}

	template <typename Traits, typename Allocator>
	// cppcheck-suppress noExplicitConstructor
	MessageView(std::basic_string<char, Traits, Allocator> const& str) noexcept
		: m_message(str.data())
		, m_length(str.size())
	{}

	constexpr char const* data() const noexcept
	{
		return m_message;
	}

	constexpr size_t size() const noexcept
	{
		return m_length;
	}

private:
	char const* m_message;
	size_t m_length;
};
#    else
using MessageView = std::string_view;
#    endif

namespace impl {
static constexpr size_t defaultMessages(size_t capacity)
{
	return capacity == 0 ? 0 : std::max<size_t>(2, capacity / sizeof(void*));
}
} // namespace impl

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
template <
	size_t Capacity = 0, size_t Messages = impl::defaultMessages(Capacity),
	bool ThreadSafe = (Capacity > 0)>
class MessageFifo {
	static_assert(
		!(ThreadSafe && Capacity == 0), "Unbounded tread-safe FIFOs are not supported");

	typedef Buffer<char, Capacity> Buffer_type;
	typedef typename Buffer_type::pointer buffer_pointer;
	typedef std::pair<buffer_pointer, buffer_pointer> Msg;

public:
	typedef Message type;
	typedef MessageView const_type;

	constexpr bool bounded() const noexcept
	{
		return Capacity > 0 || Messages > 0;
	}

	bool empty() const noexcept
	{
		return m_msg.empty();
	}

	size_t available() const noexcept
	{
		return m_msg.available();
	}

	constexpr size_t size() const noexcept
	{
		return m_buffer.size();
	}

	constexpr size_t capacity() const noexcept
	{
		return Capacity > 0 ? size() - 1U : size();
	}

	bool full() const noexcept
	{
		return m_msg.full() || space() == 0;
	}

	size_t space() const noexcept
	{
		if(!bounded())
			return std::numeric_limits<size_t>::max();
		if(m_msg.full())
			return 0;

		size_t rp = m_rp.load(std::memory_order_relaxed);
		size_t wp = m_wp.load(std::memory_order_relaxed);
		size_t partial = m_wp_partial - wp;
		size_t sz = size();

		if(wp < rp)
			return rp - wp - partial - 1U;
		else
			return rp == 0 ? sz - wp - partial - 1U
				       : std::max(sz - wp, rp - 1U) - partial;
	}

	const_type front() const noexcept
	{
		Msg const& msg = m_msg.front();
		return const_type{&m_buffer[msg.first], msg.second};
	}

	type front() noexcept
	{
		Msg& msg = m_msg.front();
		return type{&m_buffer[msg.first], msg.second};
	}

	void pop_front() noexcept
	{
		Msg const& msg = m_msg.front();
		m_rp.store((buffer_pointer)(msg.first + msg.second), std::memory_order_relaxed);
		m_msg.pop_front();
	}

	bool push_back(char const* message, size_t length)
	{
		return push_back(const_type{message, length});
	}

	bool push_back(const_type const& message)
	{
		if(m_msg.full())
			return false;

		if(!append_back(message))
			return false;

		push_back_partial();
		return true;
	}

	bool push_back()
	{
		if(m_msg.full())
			return false;

		push_back_partial();
		return true;
	}

protected:
	void push_back_partial()
	{
		stored_assert(!m_msg.full());
		buffer_pointer wp = m_wp.load(std::memory_order_relaxed);
		stored_assert(m_wp_partial >= wp);
		m_wp.store(m_wp_partial, std::memory_order_relaxed);
		m_msg.emplace_back(wp, (buffer_pointer)(m_wp_partial - wp));
	}

public:
	// The name suggests that we can pop any item from the back, but that
	// is not true.  It only drops the partial/appended data from the back.
	// reset_back() is a better name.  We should deprecate this function.
	void pop_back()
	{
		reset_back();
	}

	void reset_back()
	{
		m_wp_partial = m_wp.load(std::memory_order_relaxed);
	}

	bool append_back(char const* message, size_t length)
	{
		return append_back(const_type{message, length});
	}

	bool append_back(const_type const& message)
	{
		if(!message.size())
			return true;

		buffer_pointer rp = m_rp.load(std::memory_order_relaxed);
		buffer_pointer wp = m_wp.load(std::memory_order_relaxed);
		buffer_pointer wp_partial = m_wp_partial;
		stored_assert(wp_partial >= wp);
		size_t partial = (size_t)(wp_partial - wp);

		if(unlikely(Capacity > 0 && message.size() + partial > Capacity)) {
			// Will never fit.
#    ifdef STORED_cpp_exceptions
			throw std::bad_alloc();
#    else
			std::terminate();
#    endif
		}

		if(wp == rp && partial == 0) {
			// Note: Because of race conditions in a threaded
			// environment, it might be the case that empty() is
			// not yet true, as pop_front() first updates rp and
			// then pops the message.

			// When empty, the other side ignores the wp/rp, so we
			// can safely tinker with it.
			wp = m_wp_partial = wp_partial = 0;
			rp = 0;
			m_wp.store(wp, std::memory_order_relaxed);
			m_rp.store(rp, std::memory_order_relaxed);
		}

		if(Capacity == 0) {
			// Make buffer larger.
			m_buffer.resize((size_t)wp_partial + message.size());
		} else if(wp >= rp) {
			// [rp,wp_partial[ is in use.
			if((size_t)wp_partial + message.size() < m_buffer.size()) {
				// Ok, fits in the remaining buffer.
			} else if(
				(size_t)wp_partial + message.size() <= m_buffer.size() && rp > 0) {
				// Ok, fits in the remaining buffer.
			} else if((size_t)rp > partial + message.size()) {
				// The message fits at the start of the buffer.
				m_buffer.move(0, wp, partial);
				wp = 0;
				m_wp_partial = wp_partial = (buffer_pointer)partial;
				m_wp.store(wp, std::memory_order_relaxed);
			} else {
				// Does not fit.
				return false;
			}
		} else {
			// [0,wp_partial[ and [rp,size()[ is in use.
			if((size_t)(rp - wp_partial) > message.size()) {
				// Fits here.
			} else {
				// Does not fit.
				return false;
			}
		}

		// Write message content.
		m_buffer.set(wp_partial, message.data(), message.size());
		m_wp_partial = (buffer_pointer)(m_wp_partial + message.size());
		return true;
	}

	template <typename It>
	size_t push_back(It start, It end)
	{
		size_t cnt = 0;

		for(; start != end; ++start, ++cnt)
			if(!push_back(*start))
				return cnt;

		return cnt;
	}

	size_t push_back(std::initializer_list<const_type> init)
	{
		size_t cnt = 0;

		for(auto const& x : init) {
			if(!push_back(x))
				return cnt;
			++cnt;
		}

		return cnt;
	}

	typedef PopIterator<MessageFifo> iterator;
	constexpr iterator begin() noexcept
	{
		return PopIterator<MessageFifo>(*this);
	}

	constexpr iterator end() noexcept
	{
		return PopIterator<MessageFifo>();
	}

	void clear()
	{
		m_rp.store(
			m_wp_partial = m_wp.load(std::memory_order_relaxed),
			std::memory_order_relaxed);
		m_msg.clear();
	}

private:
	Buffer_type m_buffer;
	std::atomic<buffer_pointer> m_rp{0};
	std::atomic<buffer_pointer> m_wp{0};
	buffer_pointer m_wp_partial{0};

	Fifo<Msg, Messages> m_msg;
};

#    ifdef STORED_COMPILER_GCC
// We seem to trigger https://gcc.gnu.org/bugzilla/show_bug.cgi?id=105469
#      pragma GCC push_options
#      pragma GCC optimize("no-devirtualize")
#    endif

/*!
 * \brief A ProtocolLayer that buffers downstream messages.
 *
 * To get the messages from the fifo, call #recv(). If there are any, the
 * are passed upstream.  Blocking on a #recv() is not supported; always use
 * 0 as timeout (no waiting).
 *
 * This fifo is thread-safe by default. Only encode() messages from one
 * context, and only recv() (and therefore decode()) from another context.
 * Do not mix or have multiple encoding/decoding contexts.
 */
template <size_t Capacity, size_t Messages = impl::defaultMessages(Capacity)>
class FifoLoopback1 : public PolledLayer {
	STORED_CLASS_NOCOPY(FifoLoopback1)
public:
	static_assert(Capacity > 0, "Only bounded fifos are supported");

	typedef PolledLayer base;
	typedef MessageFifo<Capacity, Messages, true> Fifo_type;

	explicit FifoLoopback1(ProtocolLayer* up = nullptr, ProtocolLayer* down = nullptr)
		: base(up, down)
	{}

	virtual ~FifoLoopback1() override = default;

	/*!
	 * \brief Pass at most one message in the FIFO to #decode().
	 *
	 * \p timeout_us is here for compatibility with the ProtocolLayer
	 * interface, but must be 0. Actual blocking is not supported.
	 *
	 * The return value is either 0 on success or \c EAGAIN in case the
	 * FIFO is empty.  This value is not saved in #lastError(), as that
	 * field is only used by #encode() and is not thread-safe.
	 */
	virtual int recv(long timeout_us = 0) override
	{
		STORED_UNUSED(timeout_us)
		stored_assert(timeout_us == 0);

		if(m_fifo.empty())
			return EAGAIN;

		auto m = m_fifo.front();
		decode(m.data(), m.size());
		m_fifo.pop_front();

		return 0;
	}

	/*!
	 * \brief Pass all available messages in the FIFO to #decode().
	 */
	virtual void recvAll()
	{
		for(auto m : m_fifo)
			decode(m.data(), m.size());
	}

	/*!
	 * \copydoc stored::PolledLayer::encode(void const*, size_t, bool)
	 *
	 * When the FIFO is full and new messages are dropped, the #overflow() is
	 * invoked. As long as it returns \c true, the FIFO push is retried.
	 *
	 * \see #setOverflowHandler()
	 */
	virtual void encode(void const* buffer, size_t len, bool last = true) override
	{
		bool res = false;

		do {
			if(last)
				res = m_fifo.push_back((char const*)buffer, len);
			else
				res = m_fifo.append_back((char const*)buffer, len);
		} while(!res && overflow());

		base::encode(buffer, len, last);
	}

	/*!
	 * \brief Invoke overflow handler.
	 *
	 * If no callback is set, #lastError() is set to \c ENOMEM, and \c
	 * false is returned. This flag is only reset by #reset().
	 *
	 * \return \c true if the overflow situation might be resolved, \c
	 *         false when no other fifo push is to be attempted and the
	 *         data is to be dropped.
	 *
	 * \see #setOverflowHandler()
	 */
	virtual bool overflow()
	{
		if(m_overflowCallback) {
			return m_overflowCallback();
		} else {
			setLastError(ENOMEM);
			return false;
		}
	}

	using OverflowCallback = bool();

	/*!
	 * \brief Set the handler to be called by #overflow().
	 */
	template <typename F = std::nullptr_t, SFINAE_IS_FUNCTION(F, OverflowCallback, int) = 0>
	void setOverflowHandler(F&& cb = nullptr)
	{
		m_overflowCallback = std::forward<F>(cb);
	}

#    ifndef DOXYGEN
	using base::encode;
#    endif

	virtual void reset() override
	{
		base::reset();
		setLastError(0);
	}

	virtual size_t mtu() const override
	{
		size_t res = base::mtu();
		return res ? std::min(Capacity, res) : Capacity;
	}

	constexpr bool bounded() const noexcept
	{
		return m_fifo.bounded();
	}

	bool empty() const noexcept
	{
		return m_fifo.empty();
	}

	size_t available() const noexcept
	{
		return m_fifo.available();
	}

	constexpr size_t size() const noexcept
	{
		return m_fifo.size();
	}

	bool full() const noexcept
	{
		return m_fifo.full();
	}

	size_t space() const noexcept
	{
		return m_fifo.space();
	}

private:
	Fifo_type m_fifo;
	Callable<OverflowCallback>::type m_overflowCallback;
};

/*!
 * \brief Bidirectional loopback for two protocol stacks with thread-safe FIFOs.
 *
 * The loopback has an \c a and \c b side, which are symmetrical.  Both
 * sides can be used to connect to a #stored::Synchronizer, for example.
 */
template <size_t Capacity, size_t Messages = impl::defaultMessages(Capacity)>
class FifoLoopback {
	STORED_CLASS_NOCOPY(FifoLoopback)
public:
	using FifoLoopback1_type = FifoLoopback1<Capacity, Messages>;

	FifoLoopback()
		: m_a(nullptr, &m_a2b)
		, m_b(nullptr, &m_b2a)
		, m_a2b(&m_b)
		, m_b2a(&m_a)
	{}

	FifoLoopback(ProtocolLayer& a, ProtocolLayer& b)
		: FifoLoopback()
	{
		this->a().setUp(&a);
		a.setDown(&this->a());
		this->b().setUp(&b);
		b.setDown(&this->b());
	}

	~FifoLoopback()
	{
		m_a.setDown(nullptr);
		m_b.setDown(nullptr);
		m_a2b.setUp(nullptr);
		m_b2a.setUp(nullptr);
	}

	/*!
	 * \brief \c a endpoint.
	 *
	 * Use this layer to register in a #stored::Synchronizer, for example,
	 * or to wrap another stack.
	 */
	ProtocolLayer& a()
	{
		return m_a;
	}

	/*!
	 * \brief \c b endpoint.
	 *
	 * Use this layer to register in a #stored::Synchronizer, for example,
	 * or to wrap another stack.
	 */
	ProtocolLayer& b()
	{
		return m_b;
	}

	/*!
	 * \brief The \c a to \c b FIFO.
	 *
	 * Use this FIFO to call \c recv() on at the \c b side of the loopback.
	 */
	FifoLoopback1_type& a2b()
	{
		return m_a2b;
	}

	/*!
	 * \brief The \c b to \c a FIFO.
	 *
	 * Use this FIFO to call \c recv() on at the \c a side of the loopback.
	 */
	FifoLoopback1_type& b2a()
	{
		return m_b2a;
	}

private:
	ProtocolLayer m_a;
	ProtocolLayer m_b;
	FifoLoopback1_type m_a2b;
	FifoLoopback1_type m_b2a;
};

#    ifdef STORED_COMPILER_GCC
#      pragma GCC pop_options
#    endif

} // namespace stored

#  endif // STORED_cplusplus
#endif	 // __cplusplus
#endif	 // LIBSTORED_FIFO_H
