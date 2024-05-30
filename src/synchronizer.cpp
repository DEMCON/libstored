// SPDX-FileCopyrightText: 2020-2024 Jochem Rutgers
//
// SPDX-License-Identifier: MPL-2.0

#include <libstored/synchronizer.h>

#include <algorithm>

namespace stored {


/////////////////////////////
// StoreJournal
//

namespace impl {

/*!
 * \brief StoreJournal::Key operations.
 *
 * Encoding/decoding the key depends on the store size, as it determines the
 * key size.  As the store size is constant, determine the required codec
 * functions only once in the ctor, use them afterwards during normal
 * operation.
 */
// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions,hicpp-special-member-functions)
class KeyCodec {
public:
	typedef StoreJournal::Key type;

	virtual ~KeyCodec() is_default
	virtual size_t size() const noexcept = 0;
	virtual void encode(uint8_t* buf, size_t& offset, type key) const noexcept = 0;
	virtual void encode2(uint8_t*& buf, type a, type b) const noexcept = 0;
	virtual type decode(uint8_t*& buffer, size_t& len, bool& ok) const noexcept = 0;
	virtual void push_back2(void* list, size_t& len, type a, type b) const noexcept = 0;
	virtual type pop_front(void*& list, size_t& len) const noexcept = 0;

protected:
	template <typename T>
	static void encode_(uint8_t* buf, size_t& offset, type key) noexcept
	{
		key = endian_h2s((T)key);
		memcpy(buf, &key, sizeof(T));
		offset += sizeof(T);
	}

	template <typename T>
	static type decode_(uint8_t*& buffer, size_t& len, bool& ok) noexcept
	{
		if(unlikely(len < sizeof(T))) {
			ok = false;
			return 0;
		}

		type key = (type)endian_s2h<T>(buffer);
		len -= sizeof(T);
		buffer += sizeof(T);
		return key;
	}

	template <typename T>
	static void push_back2_(void* list, size_t& len, type a, type b) noexcept
	{
		T* list_ = (T*)list;
		list_[len++] = (T)a;
		list_[len++] = (T)b;
	}

	template <typename T>
	static type pop_front_(void*& list, size_t& len) noexcept
	{
		stored_assert(len > 0);
		T* list_ = (T*)list;
		type key = *list_;
		list_++;
		len--;
		list = (void*)list_;
		return key;
	}
};

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions,hicpp-special-member-functions)
class KeyCodec1 final : public KeyCodec {
public:
	~KeyCodec1() override is_default

	size_t size() const noexcept override
	{
		return 1U;
	}

	void encode(uint8_t* buf, size_t& offset, type key) const noexcept override
	{
		buf[0] = (uint8_t)key;
		offset += 1U;
	}

	void encode2(uint8_t*& buf, type a, type b) const noexcept override
	{
		buf[0] = (uint8_t)a;
		buf[1] = (uint8_t)b;
		buf += 2;
	}

	type decode(uint8_t*& buffer, size_t& len, bool& ok) const noexcept override
	{
		if(unlikely(len == 0)) {
			ok = false;
			return 0;
		}

		type key = (type)*buffer;
		len--;
		buffer++;
		return key;
	}

	void push_back2(void* list, size_t& len, type a, type b) const noexcept override
	{
		push_back2_<uint8_t>(list, len, a, b);
	}

	type pop_front(void*& list, size_t& len) const noexcept override
	{
		return pop_front_<uint8_t>(list, len);
	}
};

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions,hicpp-special-member-functions)
class KeyCodec2 final : public KeyCodec {
public:
	~KeyCodec2() override is_default

	size_t size() const noexcept override
	{
		return 2U;
	}

	void encode(uint8_t* buf, size_t& offset, type key) const noexcept override
	{
		encode_<uint16_t>(buf, offset, key);
	}

	void encode2(uint8_t*& buf, type a, type b) const noexcept override
	{
		size_t offset = 0;
		encode_<uint16_t>(buf, offset, a);
		encode_<uint16_t>(buf + offset, offset, b);
		buf += offset;
	}

	type decode(uint8_t*& buffer, size_t& len, bool& ok) const noexcept override
	{
		return decode_<uint16_t>(buffer, len, ok);
	}

	void push_back2(void* list, size_t& len, type a, type b) const noexcept override
	{
		push_back2_<uint16_t>(list, len, a, b);
	}

	type pop_front(void*& list, size_t& len) const noexcept override
	{
		return pop_front_<uint16_t>(list, len);
	}
};

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions,hicpp-special-member-functions)
class KeyCodec4 final : public KeyCodec {
public:
	~KeyCodec4() override is_default

	size_t size() const noexcept override
	{
		return 4U;
	}

	void encode(uint8_t* buf, size_t& offset, type key) const noexcept override
	{
		encode_<uint32_t>(buf, offset, key);
	}

	void encode2(uint8_t*& buf, type a, type b) const noexcept override
	{
		size_t offset = 0;
		encode_<uint32_t>(buf, offset, a);
		encode_<uint32_t>(buf + offset, offset, b);
		buf += offset;
	}

	type decode(uint8_t*& buffer, size_t& len, bool& ok) const noexcept override
	{
		return decode_<uint32_t>(buffer, len, ok);
	}

	void push_back2(void* list, size_t& len, type a, type b) const noexcept override
	{
		push_back2_<uint32_t>(list, len, a, b);
	}

	type pop_front(void*& list, size_t& len) const noexcept override
	{
		return pop_front_<uint32_t>(list, len);
	}
};

static KeyCodec1 const keyCodec1;
static KeyCodec2 const keyCodec2;
static KeyCodec4 const keyCodec4;

} // namespace impl

/*!
 * \brief Ctor.
 * \param hash the hash of the store
 * \param buffer the buffer of the store
 * \param size the size of \p buffer
 * \param callback the #stored::Synchronizable::TypedStoreCallback instance
 */
StoreJournal::StoreJournal(
	char const* hash, void* buffer, size_t size, StoreJournal::StoreCallback* callback)
	: m_hash(hash)
	, m_buffer(buffer)
	, m_bufferSize(size)
	, m_keyCodec()
	, m_seq(1)
	, m_seqLower()
	, m_partialSeq()
	, m_callback(callback && callback->doHooks() ? callback : nullptr)
{
	// Size is 32 bit, where size_t might be 64. But I guess that the
	// store is never >4G in size...
	stored_assert(size < std::numeric_limits<Size>::max());

	switch(keySize(size)) {
	default:
		// NOLINTNEXTLINE(hicpp-static-assert,misc-static-assert,cert-dcl03-c)
		stored_assert(false);
		STORED_FALLTHROUGH
	case 0:
	case 1:
		m_keyCodec = &impl::keyCodec1;
		break;
	case 2:
		m_keyCodec = &impl::keyCodec2;
		break;
	case 4:
		m_keyCodec = &impl::keyCodec4;
		break;
	}
}

/*!
 * \brief Compute key size in bytes given a buffer size.
 */
uint8_t StoreJournal::keySize(size_t bufferSize)
{
	uint8_t s = 0;
	while(bufferSize) {
		s++;
		bufferSize >>= 8U;
	}
	return s;
}

/*!
 * \brief Return the hash of the corresponding store.
 */
char const* StoreJournal::hash() const
{
	return m_hash;
}

/*!
 * \brief Returns the current update sequence number.
 *
 * This number indicates in which period objects have changed.
 * Then, a SyncConnection can determine which objects have already and which
 * have not yet been updated remotely.
 */
StoreJournal::Seq StoreJournal::seq() const
{
	return m_seq;
}

/*!
 * \brief Bump #seq(), when required.
 */
StoreJournal::Seq StoreJournal::bumpSeq()
{
	return bumpSeq(false);
}

/*!
 * \brief Bump #seq().
 */
StoreJournal::Seq StoreJournal::bumpSeq(bool force)
{
	if(!force && !m_partialSeq)
		return m_seq;

	m_partialSeq = false;
	m_seq++;

	Seq const safeRange = ShortSeqWindow - SeqLowerMargin;

	if(unlikely(m_seq - m_seqLower > safeRange)) {
		Seq seqLower = m_seq;
		m_seqLower = m_seq - ShortSeqWindow + (Seq)2U * SeqLowerMargin;

		for(size_t i = 0; i < m_changes.size(); i++) {
			ObjectInfo const& o = m_changes[i];
			Seq o_seq = toLong(o.seq);
			if(m_seq - o_seq > safeRange) {
				update(o.key, o.len, m_seqLower, 0, m_changes.size());
				seqLower = m_seqLower;
			} else
				seqLower = std::min(seqLower, o_seq);
		}

		m_seqLower = seqLower;
	}

	return m_seq;
}

/*!
 * \brief Convert to short seq.
 * \param seq the seq to convert, which must be within the current ShortSeqWindow
 */
// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
StoreJournal::ShortSeq StoreJournal::toShort(StoreJournal::Seq seq) const
{
	stored_assert(seq <= m_seq);
	stored_assert(m_seq - seq < ShortSeqWindow);
	return (ShortSeq)seq;
}

/*!
 * \brief Convert from short seq.
 */
StoreJournal::Seq StoreJournal::toLong(StoreJournal::ShortSeq seq) const
{
	ShortSeq short_m_seq = toShort(m_seq);
	if(seq == short_m_seq)
		return m_seq;
	else if(seq < short_m_seq)
		//     <------ShortSeqWindow------->
		//    0....seq....short_m_seq.......|.... . . . ...m_seq
		return m_seq - (short_m_seq - seq);
	else
		//    0....short_m_seq....seq.......|.... . . . ...m_seq
		return m_seq - (ShortSeqWindow + short_m_seq - seq);
}

/*!
 * \brief Record a change.
 * \param key of the object within the store this is the journal of
 * \param len the length of the (changed) data of the object.
 *        This is usually constant, but may change if the object is a string, for example.
 * \param insertIfNew when \c true, record the change, even if it is not in the journal yet
 */
void StoreJournal::changed(StoreJournal::Key key, size_t len, bool insertIfNew)
{
	m_partialSeq = true;

	if(!update(key, len, seq(), 0, m_changes.size()) && insertIfNew) {
#if STORED_cplusplus >= 201103L
		m_changes.emplace_back(key, (Size)len, toShort(seq()));
#else
		m_changes.push_back(ObjectInfo(key, (Size)len, toShort(seq())));
#endif
		regenerate();
	}
}

/*!
 * \brief Update the meta data of the given key.
 * \details This function does a binary search through \c m_changes, limited by [lower,upper[.
 * \return \c true of successful, \c false if key is unknown
 */
bool StoreJournal::update(
	StoreJournal::Key key, size_t len, StoreJournal::Seq seq, size_t lower, size_t upper)
{
	if(lower >= upper)
		return false;

	size_t pivot = (upper - lower) / 2 + lower;
	ObjectInfo& o = m_changes[pivot];

	o.highest = toShort(std::max(toLong(o.highest), seq));

	if(o.key == key) {
		o.seq = toShort(seq);
		o.len = (Size)len;
		return true;
	} else if(key < o.key)
		return update(key, len, seq, lower, pivot);
	else
		return update(key, len, seq, pivot + 1, upper);
}

/*!
 * \brief Regenerate the administration.
 * \details This must be done if elements are added or removed from \c m_changes.
 */
void StoreJournal::regenerate()
{
	std::sort(m_changes.begin(), m_changes.end(), ObjectInfoComparator());
	regenerate(0, m_changes.size());
}

/*!
 * \brief Implementation of #regenerate().
 */
StoreJournal::Seq StoreJournal::regenerate(size_t lower, size_t upper)
{
	if(lower >= upper)
		return 0;

	size_t pivot = (upper - lower) / 2 + lower;
	ObjectInfo& o = m_changes[pivot];

	Seq highest = std::max(
		toLong(o.seq), std::max(regenerate(lower, pivot), regenerate(pivot + 1, upper)));

	o.highest = toShort(highest);
	return highest;
}

/*!
 * \brief Checks if the given object has changed since the given sequence number.
 *
 * The sequence number is the value returned by #encodeUpdates().
 */
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
bool StoreJournal::hasChanged(Key key, Seq since) const
{
	size_t lower = 0;
	size_t upper = m_changes.size();

	while(lower < upper) {
		size_t pivot = (upper - lower) / 2 + lower;

		ObjectInfo const& o = m_changes[pivot];

		if(toLong(o.highest) < since)
			return false;
		if(o.key == key)
			return toLong(o.seq) >= since;

		if(o.key > key)
			upper = pivot;
		else
			lower = pivot + 1;
	}

	return false;
}

/*!
 * \brief Checks if any object has changed since the given sequence number.
 *
 * The sequence number is the value returned by #encodeUpdates().
 */
bool StoreJournal::hasChanged(Seq since) const
{
	if(m_changes.empty())
		return false;

	size_t pivot = m_changes.size() / 2;
	return toLong(m_changes[pivot].highest) >= since;
}

/*!
 * \brief Iterate all changes since the given seq.
 *
 * The callback \p will receive the Key of the object that has changed
 * since the given seq, and the supplied \p arg.
 */
void StoreJournal::iterateChanged(
	StoreJournal::Seq since, IterateChangedCallback* cb, void* arg) const
{
	if(!cb)
		return;

	iterateChanged(since, cb, arg, 0, m_changes.size());
}

/*!
 * \brief Implementation of #iterateChanged()
 */
void StoreJournal::iterateChanged(
	StoreJournal::Seq since, IterateChangedCallback* cb, void* arg, size_t lower,
	size_t upper) const
{
	if(lower >= upper)
		return;

	size_t pivot = (upper - lower) / 2 + lower;
	ObjectInfo const& o = m_changes[pivot];
	if(toLong(o.highest) < since)
		return;

	iterateChanged(since, cb, arg, lower, pivot);
	if(toLong(o.seq) >= since)
		cb(o.key, arg);
	iterateChanged(since, cb, arg, pivot + 1, upper);
}

/*!
 * \brief Remove all elements from the administration older than the given threshold.
 * \param oldest seq of oldest element to remain. If 0, use older than #SeqCleanThreshold before
 *	#seq().
 */
void StoreJournal::clean(StoreJournal::Seq oldest)
{
	if(oldest == 0)
		oldest = seq() - SeqCleanThreshold;

	bool didErase = false;
	for(size_t i = m_changes.size(); i > 0; i--)
		if(toLong(m_changes[i - 1].seq) < oldest) {
			m_changes[i - 1] = m_changes[m_changes.size() - 1];
			m_changes.pop_back();
			didErase = true;
		}

	if(didErase)
		regenerate();
}

/*!
 * \brief Encode the store's hash into a Synchronizer message.
 */
void StoreJournal::encodeHash(ProtocolLayer& p, bool last) const
{
	encodeHash(p, hash(), last);
}

/*!
 * \brief Encode a hash into a Synchronizer message.
 */
void StoreJournal::encodeHash(ProtocolLayer& p, char const* hash, bool last)
{
	size_t len = strlen(hash) + 1;
	stored_assert(len > sizeof(SyncConnection::Id)); // Otherwise SyncConnection::Bye cannot see
							 // the difference.
	p.encode(hash, len, last);
}

/*!
 * \brief Decode hash from a Synchronizer message.
 * \return the hash, or an empty string upon decode error
 */
char const* StoreJournal::decodeHash(void*& buffer, size_t& len)
{
	char* buffer_ = static_cast<char*>(buffer);
	size_t i = 0;
	for(; i < len && buffer_[i]; i++)
		;

	if(i == len) {
		// \0 not found
		len = 0;
		return "";
	} else {
		i++; // skip \0 too
		len -= i;
		buffer = (void*)(buffer_ + i);
		return buffer_;
	}
}

/*!
 * \brief Encode full store's buffer as a #stored::Synchronizer message.
 * \return the seq number of the change set
 */
StoreJournal::Seq StoreJournal::encodeBuffer(ProtocolLayer& p, bool last)
{
	if(m_callback)
		m_callback->hookEntryRO();

	// There may be some gaps between the variables, which need to be unpoised too...
	STORED_MAKE_MEM_DEFINED(buffer(), bufferSize());

	p.encode(buffer(), bufferSize(), last);

	if(m_callback)
		m_callback->hookExitRO();

	// Also poison the gaps.
	STORED_MAKE_MEM_NOACCESS(buffer(), bufferSize());

	return bumpSeq();
}

/*!
 * \brief Decode and process a full store's buffer from #stored::Synchronizer message.
 * \return the seq number of the applied changes
 */
StoreJournal::Seq StoreJournal::decodeBuffer(void*& buffer, size_t& len)
{
	if(len < bufferSize())
		return 0;

	// Don't use entryX/exitX, as we don't take exclusive ownership of the
	// data, we only update our local copy.
	STORED_MAKE_MEM_DEFINED(this->buffer(), bufferSize());
	memcpy(this->buffer(), buffer, bufferSize());
	STORED_MAKE_MEM_NOACCESS(this->buffer(), bufferSize());

	if(m_callback)
		m_callback->hookChanged();

	len -= bufferSize();
	m_partialSeq = true;
	Seq seq_ = this->seq();
	for(Changes::iterator it = m_changes.begin(); it != m_changes.end(); ++it)
		it->seq = it->highest = toShort(seq_);
	return bumpSeq();
}

/*!
 * \brief Encode all updates of changed objects since the given update sequence number (inclusive).
 * \return The sequence number of the most recent update, to be passed to the next invocation of \c
 *	encodeUpdates().
 */
StoreJournal::Seq StoreJournal::encodeUpdates(uint8_t*& buf, StoreJournal::Seq sinceSeq)
{
	encodeUpdates(buf, sinceSeq, 0, m_changes.size());
	return bumpSeq();
}

/*!
 * \brief Implementation of #encodeUpdates().
 */
void StoreJournal::encodeUpdates(
	uint8_t*& buf, StoreJournal::Seq sinceSeq, size_t lower, size_t upper)
{
	if(lower >= upper)
		return;

	size_t pivot = (upper - lower) / 2 + lower;
	ObjectInfo& o = m_changes[pivot];
	if(toLong(o.highest) < sinceSeq)
		return;

	encodeUpdates(buf, sinceSeq, lower, pivot);
	if(toLong(o.seq) >= sinceSeq)
		encodeUpdate(buf, o);
	encodeUpdates(buf, sinceSeq, pivot + 1, upper);
}

/*!
 * \brief Encode one change.
 */
void StoreJournal::encodeUpdate(uint8_t*& buf, StoreJournal::ObjectInfo& o)
{
	void* o_buf = keyToBuffer(o.key);
	StoreJournal::Size const o_len = o.len;

	if(unlikely(m_callback))
		m_callback->hookEntryRO(Type::Invalid, o_buf, o_len);

	m_keyCodec->encode2(buf, o.key, o_len);

	STORED_MAKE_MEM_DEFINED(o_buf, o_len);

	memcpy(buf, o_buf, o_len);
	buf += o_len;

	if(unlikely(m_callback))
		m_callback->hookExitRO(Type::Invalid, o_buf, o_len);

	STORED_MAKE_MEM_NOACCESS(o_buf, o_len);
}

/*!
 * \brief Decode and apply updates from a #stored::Synchronizer message.
 *
 * \p scratch should be at least be \c len + 3 of size. It may overlap with \p
 * buffer.
 */
StoreJournal::Seq
StoreJournal::decodeUpdates(void*& buffer, size_t& len, bool recordAll, void* scratch)
{
	bool doHook = m_callback && m_callback->doHookChanged();

	// We need to build a list of key/size pairs to call the changed hook
	// on after decoding.  We can do this in the scratch pad memory, which
	// is the received message buffer.  This buffer always contains at
	// least 3 bytes before buffer, so we can always align 32-bit Keys
	// properly.  As per update, the message contains the key, size, and
	// data, the scratch memory will never grow faster than the decoded
	// buffer.

	// Align to Key size. Even indexes are keys, odd indexes are sizes.
	// NOLINTNEXTLINE
	void* changes = (void*)(((uintptr_t)scratch + sizeof(Key) - 1U) & ~(sizeof(Key) - 1U));
	// NOLINTNEXTLINE
	stored_assert(
		// NOLINTNEXTLINE
		(uintptr_t)scratch > (uintptr_t)buffer || (uintptr_t)changes <= (uintptr_t)buffer);
	size_t chcnt = 0;

	uint8_t* buffer_ = static_cast<uint8_t*>(buffer);
	bool ok = true;

	while(len) {
		Key key = decodeKey(buffer_, len, ok);
		Size size = (Size)decodeKey(buffer_, len, ok);
		void* obj = keyToBuffer(key, size, &ok);

		if(unlikely(!ok || len < size)) {
			// Decoding error. Stop processing this message.
			buffer = buffer_;
			ok = false;
			break;
		}

		if(doHook)
			m_keyCodec->push_back2(changes, chcnt, key, (Key)size);

		STORED_MAKE_MEM_DEFINED(obj, size);
		memcpy(obj, buffer_, size);
		STORED_MAKE_MEM_NOACCESS(obj, size);

		buffer_ += size;
		len -= size;
		changed(key, size, recordAll);
	}

	// Don't use entryX/exitX, as we don't take exclusive ownership of the
	// data, we only update our local copy.

	if(doHook)
		while(chcnt) {
			Key key = m_keyCodec->pop_front(changes, chcnt);
			Size size = (Size)m_keyCodec->pop_front(changes, chcnt);
			m_callback->hookChanged(Type::Invalid, keyToBuffer(key), size);
		}

	return ok ? bumpSeq() : 0;
}

/*!
 * \brief Encode a key for a #stored::Synchronizer message.
 */
void StoreJournal::encodeKey(ProtocolLayer& p, StoreJournal::Key key)
{
	size_t keysize = 0;
	uint8_t buf[4] = {};
	m_keyCodec->encode(buf, keysize, key);
	p.encode(buf, keysize, false);
}

/*!
 * \brief Decode a key from a #stored::Synchronizer message.
 */
StoreJournal::Key StoreJournal::decodeKey(uint8_t*& buffer, size_t& len, bool& ok)
{
	return m_keyCodec->decode(buffer, len, ok);
}

/*!
 * \brief Return the buffer of the store.
 */
void* StoreJournal::buffer() const
{
	return m_buffer;
}
/*!
 * \brief Return the size of #buffer().
 */
size_t StoreJournal::bufferSize() const
{
	return m_bufferSize;
}
/*!
 * \brief Return the key size applicable to this store.
 */
size_t StoreJournal::keySize() const
{
	return m_keyCodec->size();
}

/*!
 * \brief Return the buffer of an object corresponding to the given key.
 */
void* StoreJournal::keyToBuffer(StoreJournal::Key key, StoreJournal::Size len, bool* ok) const
{
	if(ok && len && key + len > bufferSize())
		*ok = false;
	return static_cast<char*>(m_buffer) + key;
}

/*!
 * \brief Pre-allocate memory for the given number of variables.
 *
 * As long as the number of changed variables remains below this number,
 * no heap operations are performed during synchronization.
 */
void StoreJournal::reserveHeap(size_t storeVariableCount)
{
	m_changes.reserve(storeVariableCount);
}

StoreJournal::StoreCallback::~StoreCallback() is_default



/////////////////////////////
// SyncConnection
//

/*!
 * \brief Ctor.
 * \param synchronizer the Synchronizer that manages this connection
 * \param connection the protocol stack that is to wrap this SyncConnection
 */
SyncConnection::SyncConnection(Synchronizer& synchronizer, ProtocolLayer& connection)
	: m_synchronizer(&synchronizer)
	, m_idInNext(1)
{
	connection.wrap(*this);
}

/*!
 * \brief Dtor.
 */
SyncConnection::~SyncConnection()
{
	bye();
}

void SyncConnection::reset()
{
	encodeCmd(Bye);
	flush();
	dropNonSources();
	base::reset();
	helloAgain();
}

/*!
 * \brief Returns the Synchronizer that manages this connection.
 */
Synchronizer& SyncConnection::synchronizer() const
{
	return *m_synchronizer;
}

/*!
 * \brief Encode a command byte.
 */
void SyncConnection::encodeCmd(char cmd, bool last)
{
	encode(&cmd, 1, last);
}

/*!
 * \brief Encode a command byte into a buffer.
 */
void SyncConnection::encodeCmd(char cmd, uint8_t*& buf)
{
	*buf++ = (uint8_t)cmd;
}

/*!
 * \brief Returns if the given store is synchronized over this connection.
 */
bool SyncConnection::isSynchronizing(StoreJournal& store) const
{
	return m_store.find(&store) != m_store.end();
}

/*!
 * \brief Use this connection as a source of the given store.
 */
void SyncConnection::source(StoreJournal& store)
{
	StoreMap::iterator it = m_store.find(&store);
	if(it != m_store.end()) {
		// Already registered, but the direction should not have changed...
		stored_assert(it->second.source);
		return;
	}

	StoreInfo& si = m_store[&store];
	si.source = true;

	encodeCmd(Hello);
	store.encodeHash(*this, false);
	Id id = nextId();
	m_idIn[id] = &store;
	encodeId(id, true);
}

/*!
 * \brief Determine a unique ID for others to send store updates to us.
 */
SyncConnection::Id SyncConnection::nextId()
{
	stored_assert(m_idIn.size() < std::numeric_limits<Id>::max() / 2U);

	while(true) {
		Id id = m_idInNext++;

		if(id == 0)
			continue;
		if(m_idIn.find(id) != m_idIn.end())
			continue;

		return id;
	}
}

/*!
 * \brief Deregister a given store.
 */
void SyncConnection::drop(StoreJournal& store)
{
	bool sendBye = false;

	for(IdInMap::iterator it = m_idIn.begin(); it != m_idIn.end(); ++it)
		if(it->second == &store) {
			m_idIn.erase(it);
			sendBye = true;
			break;
		}

	if(m_store.erase(&store))
		sendBye = true;

	if(!sendBye)
		return;

	encodeCmd(Bye);
	store.encodeHash(*this, true);
}

/*!
 * \brief Encode a Bye and drop all stores.
 */
void SyncConnection::bye()
{
	if(m_store.empty())
		return;

	encodeCmd(Bye, true);
	m_store.clear();
	m_idIn.clear();
}

/*!
 * \brief Encode a Bye with a hash and drop the corresponding store.
 */
void SyncConnection::bye(char const* hash)
{
	if(!hash || strlen(hash) <= sizeof(SyncConnection::Id))
		return;

	erase(hash);
	encodeCmd(Bye);
	StoreJournal::encodeHash(*this, hash, true);
}

/*!
 * \brief Erase the store with the given hash from this connection.
 */
void SyncConnection::erase(char const* hash)
{
	if(!hash)
		return;

	StoreJournal* j = synchronizer().toJournal(hash);
	if(!j)
		return;

	m_store.erase(j);

#if STORED_cplusplus >= 201103L
	for(IdInMap::iterator it = m_idIn.begin(); it != m_idIn.end();)
		if(it->second == j)
			it = m_idIn.erase(it);
		else
			++it;
#else
	bool done;
	do {
		done = true;
		for(IdInMap::iterator it = m_idIn.begin(); it != m_idIn.end(); ++it)
			if(it->second == j) {
				m_idIn.erase(it);
				done = false;
				break;
			}
	} while(!done);
#endif
}

/*!
 * \brief Encode a Bye message with ID, and drop it from this connection.
 */
void SyncConnection::bye(SyncConnection::Id id)
{
	eraseIn(id);
	encodeCmd(Bye);
	encodeId(id, true);
}

/*!
 * \brief Erase the store from this connection that uses the given ID to send updates to us.
 */
void SyncConnection::eraseIn(SyncConnection::Id id)
{
	IdInMap::iterator it = m_idIn.find(id);
	if(it == m_idIn.end())
		return;

	m_store.erase(it->second);
	m_idIn.erase(it);
}

/*!
 * \brief Erase the store from this connection that uses the given ID to send updates to the other
 *	party.
 */
void SyncConnection::eraseOut(SyncConnection::Id id)
{
#if STORED_cplusplus >= 201103L
	for(StoreMap::iterator it = m_store.begin(); it != m_store.end();)
		if(it->second.idOut == id) {
			for(IdInMap::iterator itIn = m_idIn.begin(); itIn != m_idIn.end();)
				if(itIn->second == it->first)
					itIn = m_idIn.erase(itIn);
				else
					++itIn;
			it = m_store.erase(it);
		} else
			++it;
#else
again:
	for(StoreMap::iterator it = m_store.begin(); it != m_store.end(); ++it)
		if(it->second.idOut == id) {
againIn:
			for(IdInMap::iterator itIn = m_idIn.begin(); itIn != m_idIn.end(); ++itIn)
				if(itIn->second == it->first) {
					m_idIn.erase(itIn);
					goto againIn;
				}
			m_store.erase(it);
			goto again;
		}
#endif
}

/*!
 * \brief Send out all updates of the given store.
 *
 * The provided \p encodeBuffer must point to a scratch pad memory, large
 * enough for the store's MaxMessageSize.
 */
StoreJournal::Seq SyncConnection::process(StoreJournal& store, void* encodeBuffer)
{
	StoreMap::iterator s = m_store.find(&store);
	if(s == m_store.end())
		// Unknown store.
		return 0;
	if(!store.hasChanged(s->second.seq))
		// No recent changes.
		return 0;

	uint8_t* encodeBuffer_ = static_cast<uint8_t*>(encodeBuffer);
	encodeCmd(Update, encodeBuffer_);
	encodeId(s->second.idOut, encodeBuffer_);
	// Make sure to set the process seq before the last part of the encode is sent.
	StoreJournal::Seq res = s->second.seq = store.encodeUpdates(encodeBuffer_, s->second.seq);
	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-cstyle-cast)
	encode(encodeBuffer, (size_t)((uintptr_t)encodeBuffer_ - (uintptr_t)encodeBuffer));

	return res;
}

/*!
 * \brief Decode the command from the buffer.
 * \return the command or \0 on error
 */
char SyncConnection::decodeCmd(void*& buffer, size_t& len)
{
	if(len == 0)
		return 0;

	len--;
	char* buffer_ = static_cast<char*>(buffer);
	buffer = buffer_ + 1;
	return buffer_[0];
}

void SyncConnection::decode(void* buffer, size_t len)
{
	// We can use the buffer as scratch pad memory, up to where the buffer
	// was decoded.
	void* scratch = buffer;

	switch(decodeCmd(buffer, len)) {
	case Hello: {
		/*
		 * Hello
		 *
		 * 'h' hash id
		 */
		char const* hash = StoreJournal::decodeHash(buffer, len);
		Id id = decodeId(buffer, len);
		if(!hash || !id)
			break;

		StoreJournal* j = synchronizer().toJournal(hash);
		if(!j) {
			// Unknown, drop immediately.
			bye(hash);
			break;
		}

		encodeCmd(Welcome);
		encodeId(id);

		StoreInfo& si = m_store[j];
		si.source = false;
		si.idOut = id;

		for(IdInMap::iterator it = m_idIn.begin(); it != m_idIn.end(); ++it)
			if(it->second == j) {
				// Apparently, we already had a sync to this store. Replace it.
				m_idIn.erase(it);
				break;
			}

		id = nextId();
		m_idIn[id] = j;
		encodeId(id, false);

		si.seq = j->encodeBuffer(*this, true);
		break;
	}
	case Welcome: {
		/*
		 * Welcome
		 *
		 * 'w' hello_id welcome_id buffer
		 */

		Id id = decodeId(buffer, len);
		if(!id)
			break;

		Id welcome_id = decodeId(buffer, len);
		IdInMap::iterator j = m_idIn.find(id);

		StoreJournal::Seq seq = 0;

		if(!welcome_id || j == m_idIn.end()) {
			bye(id);
			break;
		}

		seq = j->second->decodeBuffer(buffer, len);
		if(!seq) {
			bye(id);
			break;
		}

		StoreInfo& si = m_store[j->second];
		if(!si.source) {
			// Wrong direction.
			bye(id);
			break;
		}

		si.seq = seq;
		si.idOut = welcome_id;
		break;
	}
	case Update: {
		/*
		 * Update
		 *
		 * 'u' id updates
		 */
		Id id = decodeId(buffer, len);
		if(!id)
			return;

		IdInMap::iterator it = m_idIn.find(id);
		if(it == m_idIn.end()) {
			bye(id);
			break;
		}

		// Make sure that local changes are flushed out first.
		StoreJournal& j = *it->second;
		process(j, synchronizer().encodeBuffer());

		StoreJournal::Seq seq = 0;
		bool recordAll = synchronizer().isSynchronizing(j, *this);
		seq = it->second->decodeUpdates(buffer, len, recordAll, scratch);
		if(!seq) {
			bye(id);
			break;
		}

		stored_assert(m_store.find(it->second) != m_store.end());
		m_store[it->second].seq = seq;
		break;
	}
	case Bye: {
		/*
		 * Bye
		 *
		 * 'b' hash
		 * 'b' id
		 * 'b'
		 */
		if(len == 0) {
			dropNonSources();
			helloAgain();
		} else if(len == sizeof(Id)) {
			Id id = decodeId(buffer, len);
			if(!id)
				break;

			IdInMap::iterator it = m_idIn.find(id);
			if(it == m_idIn.end())
				break;

			StoreInfo const& si = m_store[it->second];
			if(si.source && si.idOut)
				// Hey, we need it!
				helloAgain(*(it->second));
			else
				eraseOut(id);
		} else {
			char const* hash = StoreJournal::decodeHash(buffer, len);
			StoreJournal* j = synchronizer().toJournal(hash);
			if(!j)
				break;

			StoreMap::iterator it = m_store.find(j);
			if(it == m_store.end())
				break;

			StoreInfo const& si = it->second;
			if(si.source && si.idOut)
				// Hey, we need it!
				helloAgain(*j);
			else
				erase(hash);
		}
		break;
	}
	default:;
		// Other; ignore.
	}
}

/*!
 * \brief Encode the given ID.
 */
void SyncConnection::encodeId(SyncConnection::Id id, bool last)
{
	id = endian_h2s(id);
	encode(&id, sizeof(Id), last);
}

/*!
 * \brief Encode the given ID into a buffer
 */
void SyncConnection::encodeId(SyncConnection::Id id, uint8_t*& buf)
{
	id = endian_h2s(id);
	memcpy(buf, &id, sizeof(Id));
	buf += sizeof(Id);
}

/*!
 * \brief Decode an ID from the buffer.
 * \return the ID, or 0 when there decode failed
 */
SyncConnection::Id SyncConnection::decodeId(void*& buffer, size_t& len)
{
	if(len < sizeof(Id))
		return 0;

	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
	Id id = endian_s2h<Id>(buffer);

	len -= sizeof(Id);
	buffer = static_cast<char*>(buffer) + sizeof(Id);
	return id;
}

/*!
 * \brief Drop all stores from this connection that are not sources.
 */
void SyncConnection::dropNonSources()
{
#if STORED_cplusplus < 201103L
again:
#endif
	for(IdInMap::iterator it = m_idIn.begin(); it != m_idIn.end();) {
		StoreMap::iterator sit = m_store.find(it->second);
		stored_assert(sit != m_store.end());

		if(sit->second.source) {
			// Skip.
			++it;
		} else {
			m_store.erase(sit);
#if STORED_cplusplus < 201103L
			m_idIn.erase(it);
			goto again;
#else
			it = m_idIn.erase(it);
#endif
		}
	}
}

/*!
 * \brief Send a Hello again for all sources.
 */
void SyncConnection::helloAgain()
{
	for(StoreMap::iterator it = m_store.begin(); it != m_store.end(); ++it)
		if(it->second.source)
			helloAgain(*it->first);
}

/*!
 * \brief Send a Hello again for the given source.
 */
void SyncConnection::helloAgain(StoreJournal& store)
{
	StoreMap::iterator it = m_store.find(&store);
	if(it == m_store.end())
		return;

	StoreInfo& si = it->second;
	si.idOut = 0;

	Id id = 0;
	for(IdInMap::iterator itId = m_idIn.begin(); itId != m_idIn.end(); ++itId)
		if(itId->second == &store) {
			id = itId->first;
			break;
		}

	stored_assert(id);

	encodeCmd(Hello);
	store.encodeHash(*this, false);
	encodeId(id, true);
}



/////////////////////////////
// Synchronizer
//

/*!
 * \brief Dtor.
 */
Synchronizer::~Synchronizer()
{
	for(Connections::iterator it = m_connections.begin(); it != m_connections.end(); ++it)
		// NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
		delete *it;
	m_connections.clear();
}

/*!
 * \brief Find a registered store given an hash.
 * \return the store, or \c nullptr when not found
 */
StoreJournal* Synchronizer::toJournal(char const* hash) const
{
	if(!hash)
		return nullptr;
	StoreMap::const_iterator it = m_storeMap.find(hash);
	return it == m_storeMap.end() ? nullptr : it->second;
}

/*!
 * \brief Connect the given connection to this Synchronizer.
 *
 * A #stored::SyncConnection is instantiated on top of the given protocol stack.
 * This SyncConnection is the OSI Application layer of the synchronization prococol.
 *
 * \return the #stored::SyncConnection, which is valid until #disconnect() is called
 */
SyncConnection const& Synchronizer::connect(ProtocolLayer& connection)
{
	// NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
	SyncConnection* c = new SyncConnection(*this, connection);
	m_connections.insert(c);
	return *c;
}

/*!
 * \brief Disconnect the given connection from this Synchronizer.
 *
 * Provide the connection as given to #connect() before.
 */
void Synchronizer::disconnect(ProtocolLayer& connection)
{
	SyncConnection* c = toConnection(connection);
	if(c) {
		m_connections.erase(c);
		// NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
		delete c;
	}
}

/*!
 * \brief Find the SyncConnection instance from the given connection.
 *
 * Provide the connection as given to #connect() before.
 *
 * \return the SyncConnection, or \c nullptr when not found
 */
SyncConnection* Synchronizer::toConnection(ProtocolLayer& connection) const
{
	ProtocolLayer* c = connection.up();
	if(!c)
		return nullptr;
	if(m_connections.find(c) == m_connections.end())
		return nullptr;

#ifdef STORED_cpp_rtti
	stored_assert(dynamic_cast<SyncConnection*>(c) != nullptr);
#endif
	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
	return static_cast<SyncConnection*>(c);
}

/*!
 * \brief Process updates for all connections and all stores.
 */
void Synchronizer::process()
{
	for(StoreMap::iterator it = m_storeMap.begin(); it != m_storeMap.end(); ++it)
		process(*it->second);
}

/*!
 * \brief Process updates for the given journal on all connections.
 */
void Synchronizer::process(StoreJournal& j)
{
	for(Connections::iterator it = m_connections.begin(); it != m_connections.end(); ++it)
		// NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
		static_cast<SyncConnection*>(*it)->process(j, encodeBuffer());
}

/*!
 * \brief Process updates for all stores on the given connection.
 */
void Synchronizer::process(ProtocolLayer& connection)
{
	SyncConnection* c = toConnection(connection);
	if(!c)
		return;

	for(StoreMap::iterator it = m_storeMap.begin(); it != m_storeMap.end(); ++it)
		c->process(*it->second, encodeBuffer());
}

/*!
 * \brief Process updates for the given store on the given connection.
 */
StoreJournal::Seq Synchronizer::process(ProtocolLayer& connection, StoreJournal& j)
{
	SyncConnection* c = toConnection(connection);
	if(c)
		return c->process(j, encodeBuffer());

	return 0;
}

bool Synchronizer::isSynchronizing(StoreJournal& j) const
{
	for(Connections::iterator it = m_connections.begin(); it != m_connections.end(); ++it)
		// NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
		if(static_cast<SyncConnection*>(*it)->isSynchronizing(j))
			return true;
	return false;
}

bool Synchronizer::isSynchronizing(StoreJournal& j, SyncConnection& notOverConnection) const
{
	for(Connections::iterator it = m_connections.begin(); it != m_connections.end(); ++it) {
		// NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
		SyncConnection const* c = static_cast<SyncConnection const*>(*it);
		if(c != &notOverConnection && c->isSynchronizing(j))
			return true;
	}
	return false;
}

} // namespace stored
