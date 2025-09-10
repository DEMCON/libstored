#ifndef LIBSTORED_SPM_H
#define LIBSTORED_SPM_H
// SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
//
// SPDX-License-Identifier: MPL-2.0

#ifdef __cplusplus

#	include <libstored/macros.h>
#	include <libstored/util.h>

#	include <new>

namespace stored {

/*!
 * \brief Memory that uses bump-alloc for a very fast short-lived heap.
 *
 * The ScratchPad grows automatically, but it is more efficient to manage
 * the #capacity() on beforehand.  The capacity is determined while using
 * the ScratchPad, which may cause some more overhead at the start of the
 * application.
 *
 * There is no overhead per #alloc(), but padding bytes may be inserted to
 * word-align allocs.  Heap fragmentation is not possible.
 *
 * Alloc is very fast, but dealloc or free is not possible.  Bump-alloc is
 * like a stack; you can #reset() it, or make a #snapshot(), which you can
 * rollback to.
 *
 * \tparam MaxSize the maximum total size to be allocated, which is used to
 *         determine the type of the internal counters.
 */
template <size_t MaxSize = 0xffff>
class ScratchPad {
	STORED_CLASS_NOCOPY(ScratchPad)
public:
	enum {
		/*! \brief Maximum total size of allocated memory. */
		maxSize = MaxSize,
		/*! \brief Size of the header of a chunk. */
		chunkHeader = sizeof(size_t),
		/*! \brief Extra amount to reserve when the chunk is allocated. */
		spare = 8 * sizeof(void*)
	};
	/*! \brief Type of all internally used size counters. */
	typedef typename value_type<MaxSize>::type size_type;

	/*!
	 * \brief Ctor.
	 * \param reserve number of bytes to reserve during construction
	 */
	explicit ScratchPad(size_t reserve = 0)
		: m_buffer()
		, m_size()
		, m_total()
		, m_max()
	{
		this->reserve(reserve);
	}

	/*!
	 * \brief Dtor.
	 */
	~ScratchPad() noexcept
	{
		chunkDeallocate(chunk(), bufferSize());

		for(List<char*>::type::iterator it = m_old.begin(); it != m_old.end(); ++it)
			chunkDeallocate(chunk(*it), bufferSize(*it));
	}

	/*!
	 * \brief Resets the content of the ScratchPad.
	 *
	 * Coalesce chunks when required. It leaves #max() untouched.
	 * To actually free all used memory, call #shrink_to_fit() afterwards.
	 */
	void reset() noexcept
	{
		m_size = 0;
		m_total = 0;

		if(unlikely(!m_old.empty())) {
			// Coalesce chunks.
			for(List<char*>::type::iterator it = m_old.begin(); it != m_old.end(); ++it)
				chunkDeallocate(chunk(*it), bufferSize(*it));

			m_old.clear();
			try {
				reserve(m_max);
			} catch(...) { // NOLINT(bugprone-empty-catch)
				// Leave for now.
			}
		}

		if(m_buffer)
			STORED_MAKE_MEM_NOACCESS(m_buffer, bufferSize());
	}

	/*!
	 * \brief Checks if the ScratchPad is empty.
	 */
	constexpr bool empty() const noexcept
	{
		return m_total == 0;
	}

	/*!
	 * \brief Returns the total amount of allocated memory.
	 * \details This includes padding because of alignment requirements of #alloc().
	 */
	constexpr size_t size() const noexcept
	{
		return (size_t)m_total;
	}

	/*!
	 * \brief Returns the maximum size.
	 * \details To reset this value, use #shrink_to_fit().
	 */
	constexpr size_t max() const noexcept
	{
		return (size_t)m_max;
	}

	/*!
	 * \brief Returns the total capacity currently available within the ScratchPad.
	 */
	constexpr size_t capacity() const noexcept
	{
		return (size_t)m_total - (size_t)m_size + (size_t)bufferSize();
	}

	/*!
	 * \brief A snapshot of the ScratchPad, which can be rolled back to.
	 *
	 * A Snapshot remains valid, until the ScratchPad is reset, or an
	 * earlier snapshot is rolled back.  An invalid snapshot cannot be
	 * rolled back, and cannot be destructed, as that implies a rollback.
	 * Make sure to reset the snapshot before destruction, if it may have
	 * become invalid.
	 *
	 * Normally, you would let a snapshot go out of scope before doing
	 * anything with older snapshots.
	 *
	 * \see #stored::ScratchPad::snapshot()
	 */
	class Snapshot {
	protected:
		friend class ScratchPad;
		/*!
		 * \brief Ctor.
		 */
		Snapshot(ScratchPad& spm, void* buffer, ScratchPad::size_type size) noexcept
			: m_spm(&spm)
			, m_buffer(buffer)
			, m_size(size)
		{}

	public:
		/*!
		 * \brief Dtor, which implies a rollback.
		 */
		~Snapshot() noexcept
		{
			rollback();
		}

		/*!
		 * \brief Detach from the ScratchPad.
		 * \details Cannot rollback afterwards.
		 */
		void reset() noexcept
		{
			m_spm = nullptr;
		}

		/*!
		 * \brief Perform a rollback of the corresponding ScratchPad.
		 */
		void rollback() noexcept
		{
			if(m_spm)
				m_spm->rollback(m_buffer, m_size);
		}

#	if STORED_cplusplus >= 201103L
		/*!
		 * \brief Move ctor.
		 */
		Snapshot(Snapshot&& s) noexcept
			: m_spm(s.m_spm)
			, m_buffer(s.m_buffer)
			, m_size(s.m_size)
		{
			s.reset();
		}

		/*!
		 * \brief Move-assign.
		 */
		Snapshot& operator=(Snapshot&& s) noexcept
		{
			reset();
			m_spm = s.m_spm;
			m_buffer = s.m_buffer;
			m_size = s.m_size;
			s.reset();
			return *this;
		}
#	endif
		/*!
		 * \brief Move ctor.
		 * \details Even though \p s is \c const, it will be reset anyway by this ctor.
		 */
		Snapshot(Snapshot const& s) noexcept
			: m_spm(s.m_spm)
			, m_buffer(s.m_buffer)
			, m_size(s.m_size)
		{
			s.reset();
		}

	private:
		/*!
		 * \brief Resets and detaches this snapshot from the ScratchPad.
		 */
		void reset() const noexcept
		{
			m_spm = nullptr;
		}

#	if STORED_cplusplus >= 201103L
	public:
		Snapshot& operator=(Snapshot const& s) = delete;
#	else
	private:
		/*!
		 * \brief Move-assign.
		 * \details Even though \p s is \c const, it will be reset anyway by this operator.
		 */
		Snapshot& operator=(Snapshot const& s);
#	endif

	private:
		/*! \brief The ScratchPad this is a snapshot of. */
		mutable ScratchPad* m_spm;
		/*! \brief The snapshot. */
		void* m_buffer;
		/*! \brief The total size of the ScratchPad when taking the snapshot. */
		ScratchPad::size_type m_size;
	};

	friend class Snapshot;

	/*!
	 * \brief Get a snapshot of the ScratchPad.
	 * \see #stored::ScratchPad::Snapshot.
	 */
	Snapshot snapshot() noexcept
	{
		return Snapshot(*this, empty() ? nullptr : &m_buffer[m_size], m_total);
	}

private:
	/*!
	 * \brief Perform a rollback to the given point.
	 * \see #stored::ScratchPad::Snapshot.
	 */
	void rollback(void* snapshot, size_type size) noexcept
	{
		if(!snapshot || !size) {
			reset();
			return;
		}

		// cppcheck-suppress constVariablePointer
		char* snapshot_ = static_cast<char*>(snapshot);

		// Find correct buffer.
		while(!(snapshot_ >= m_buffer && snapshot_ < &m_buffer[bufferSize()])) {
			m_size = 0; // Don't care, will be recovered later on.
			bufferPop();
			stored_assert(m_buffer);
		}

		// Recover pointers within this buffer.
		stored_assert(size <= m_total);
		m_total = size;
		size_t size_ = (size_t)(snapshot_ - m_buffer);
		stored_assert(size_ < bufferSize());
		m_size = (size_type)size_;

		STORED_MAKE_MEM_NOACCESS(&m_buffer[m_size], bufferSize() - m_size);
	}

	/*!
	 * \brief Allocate a new buffer with the given size.
	 * \details The current buffer is moved to the #m_old list.
	 */
	void bufferPush(size_t size)
	{
		stored_assert(size > 0);

		// May throw std::bad_alloc.
		char* p = allocate<char>(size + chunkHeader);

		if(m_buffer) {
			try {
				// May throw std::bad_alloc.
				m_old.push_back(m_buffer);
			} catch(...) {
				deallocate<char>(p, size + chunkHeader);
#	ifdef STORED_cpp_exceptions
				// cppcheck-suppress rethrowNoCurrentException
				throw;
#	else
				std::terminate();
#	endif
			}
		}

		m_buffer = buffer(p);
		setBufferSize(size);
		m_size = 0;

		STORED_MAKE_MEM_NOACCESS(m_buffer, size);
	}

	/*!
	 * \brief Discard the current buffer and get the next one from the #m_old list.
	 */
	void bufferPop() noexcept
	{
		stored_assert(m_buffer || m_old.empty());

		if(m_buffer) {
			chunkDeallocate(chunk(), bufferSize());
			m_buffer = nullptr;
		}

		if(!m_old.empty()) {
			m_buffer = m_old.back();
			m_old.pop_back();
		}

		stored_assert(m_size <= m_total);
		m_total = (size_type)(m_total - m_size);

		if(Config::Debug) {
			// We don't know m_size. Assume that this is part of a rollback(), which
			// will set it properly. Set it at worst-case now, such that the assert
			// above may trigger.
			size_t b = bufferSize();
			stored_assert(b <= maxSize);
			m_size = (size_type)b;
		}
	}

	/*!
	 * \brief Replace current buffer by a bigger one.
	 * \details Contents of the current buffer may be lost.
	 */
	void bufferGrow(size_t size)
	{
		stored_assert(size > bufferSize());

		// Standard allocators don't have realloc. So, deallocate first,
		// and then allocate a new one.
		chunkDeallocate(chunk(), bufferSize());
		m_buffer = buffer(allocate<char>(size + chunkHeader));
		setBufferSize(size);

		STORED_MAKE_MEM_NOACCESS(&m_buffer[m_size], size - m_size);
	}

	/*!
	 * \brief Deallocate a previously allocated chunk.
	 */
	void chunkDeallocate(void* chunk, size_t len)
	{
		if(!chunk)
			return;

		STORED_MAKE_MEM_UNDEFINED(chunk, len + chunkHeader);
		deallocate<char>(static_cast<char*>(chunk), len + chunkHeader);
	}

	/*!
	 * \brief Returns the size of the given buffer.
	 */
	static constexpr size_t bufferSize(char* buffer) noexcept
	{
		return likely(buffer) ? *(size_t*)(chunk(buffer)) : 0;
	}

	/*!
	 * \brief Returns the size of the current buffer.
	 */
	size_t bufferSize() const noexcept
	{
		return bufferSize(m_buffer);
	}

	/*!
	 * \brief Saves the malloc()ed size of the current buffer.
	 */
	void setBufferSize(size_t size) noexcept
	{
		char* buf = m_buffer;
		stored_assert(buf);
		stored_assert(size > 0);
		*(size_t*)(chunk(buf)) = size;
	}

	/*!
	 * \brief Returns the chunk from the given buffer.
	 * \details The chunk is the actual piece of memory on the heap, which is the buffer with a
	 * header.
	 */
	// cppcheck-suppress constParameterPointer
	static constexpr void* chunk(char* buffer) noexcept
	{
		return buffer ? buffer - chunkHeader : nullptr;
	}

	/*!
	 * \brief Returns the chunk from the current buffer.
	 * \details The chunk is the actual piece of memory on the heap, which is the buffer with a
	 * header.
	 */
	constexpr void* chunk() const noexcept
	{
		return m_buffer ? chunk(m_buffer) : nullptr;
	}

	/*!
	 * \brief Returns the buffer within the given chunk.
	 */
	static constexpr char* buffer(void* chunk) noexcept
	{
		return static_cast<char*>(chunk) + chunkHeader;
	}

	/*!
	 * \brief Returns the current buffer.
	 */
	constexpr char* buffer() const noexcept
	{
		return m_buffer;
	}

public:
	/*!
	 * \brief Reserves memory to save the additional given amount of bytes.
	 */
	void reserve(size_t more)
	{
		size_t new_cap = m_size + more;

		if(likely(new_cap <= bufferSize()))
			return;

		if(m_buffer && m_size == 0) {
			// Grow (realloc) is fine, as nobody is using the current buffer (if any).
			bufferGrow(new_cap);
		} else {
			// Grow (realloc) may move the buffer. So don't do that.
			new_cap = more + spare; // plus some extra reserve space
			bufferPush(new_cap);
		}
	}

	/*!
	 * \brief Releases all unused memory back to the OS, if possible.
	 */
	void shrink_to_fit() noexcept
	{
		if(unlikely(empty())) {
			m_max = 0;
			reset();
			// Also pop the current buffer.
			bufferPop();
			m_size = 0;
		} else {
			// realloc() may still return another chunk of memory, even if it gets
			// smaller. So, we cannot actually shrink, until reset() is called.
			m_max = m_total;
		}
	}

	/*!
	 * \brief Returns the number of chunks of the ScratchPad.
	 *
	 * You would want to have only one chunk, but during the first moments
	 * of running, the ScratchPad has to determine how much memory the
	 * application uses.  During this time, there may exist multiple
	 * chunks.  Call #reset() to optimize memory usage.
	 */
	constexpr size_t chunks() const noexcept
	{
		return m_old.size() + (m_buffer ? 1 : 0);
	}

	/*!
	 * \brief Allocate memory.
	 * \tparam T the type of object to allocate
	 * \param count number of objects, which is allocated as an array of \p T
	 * \param align alignment requirement (maximized to word size)
	 * \return a pointer to the allocated memory, which remains uninitialized and cannot be \c
	 * nullptr
	 */
	template <typename T>
	__attribute__((malloc, returns_nonnull, warn_unused_result)) T*
	alloc(size_t count = 1, size_t align = sizeof(T))
	{
		size_t alloc_size = count * sizeof(T);
		if(unlikely(alloc_size == 0)) {
			if(unlikely(!m_buffer))
				reserve(spare);
			// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
			return reinterpret_cast<T*>(m_buffer);
		}

		align = std::max<size_t>(1, std::min(sizeof(void*), align));
		size_t padding = align - m_size % align;
		if(padding == align)
			padding = 0;

		if(unlikely(m_total + alloc_size + padding < m_total)) {
			// Wrap around -> overflow.
#	ifdef STORED_cpp_exceptions
			throw std::bad_alloc();
#	else
			std::terminate();
#	endif
		}

		size_t bs = bufferSize();
		if(likely(m_size + padding <= bs)) {
			// The padding (which may be 0) still fits in the buffer.
			m_size = (size_type)(m_size + padding);
			// Now reserve the size, which still may add a new chunk.
			if(unlikely(m_size + alloc_size > bs))
				// Reserve all we probably need, if we are reserving anyway.
				reserve(std::max(max() - this->size(), alloc_size));
		} else {
			// Not enough room for the padding, let alone the size.
			// Just create a new buffer, which has always the correct alignment.
			bufferPush(std::max(max() - this->size(), alloc_size + spare));
		}

		char* p = m_buffer + m_size;
		m_size = (size_type)(m_size + alloc_size);

		// Do count the padding, even it was not allocated, as it might be
		// required anyway if the buffers are coalesced.
		m_total = (size_type)(m_total + padding + alloc_size);

		if(unlikely(m_total > m_max))
			m_max = m_total;

		STORED_MAKE_MEM_UNDEFINED(p, alloc_size);

		// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
		return reinterpret_cast<T*>(p);
	}

private:
	/*! \brief Current buffer chunk. If it gets full, it is pushed onto #m_old and a new one is
	 * allocated. */
	char* m_buffer;
	/*! \brief Previous buffer chunks. */
	List<char*>::type m_old;
	/*! \brief Used offset within #m_buffer. */
	size_type m_size;
	/*! \brief Total memory usage of all chunks. */
	size_type m_total;
	/*! \brief Maximum value of #m_total. */
	size_type m_max;
};

} // namespace stored
#endif // __cplusplus
#endif // LIBSTORED_SPM_H
