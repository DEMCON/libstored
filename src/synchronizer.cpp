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

#include <libstored/synchronizer.h>

#include <algorithm>

namespace stored {


/////////////////////////////
// StoreJournal
//

StoreJournal::StoreJournal(char const* hash, void* buffer, size_t size)
	: m_hash(hash)
	, m_buffer(buffer)
	, m_bufferSize(size)
	, m_keySize(keySize(m_bufferSize))
	, m_seq(1)
	, m_seqLower()
	, m_partialSeq()
{
	// Size is 32 bit, where size_t might be 64. But I guess that the
	// store is never >4G in size...
	stored_assert(size < std::numeric_limits<Size>::max());
}

uint8_t StoreJournal::keySize(size_t bufferSize) {
	uint8_t s = 0;
	while(bufferSize) {
		s++;
		bufferSize >>= 8;
	}
	return s;
}

char const* StoreJournal::hash() const {
	return m_hash;
}

/*!
 * \brief Returns the current update sequence number.
 *
 * This number indicates in which period objects have changed.
 * Then, a SyncConnection can determine which objects have already and which
 * have not yet been updated remotely.
 */
StoreJournal::Seq StoreJournal::seq() const {
	return m_seq;
}

StoreJournal::Seq StoreJournal::bumpSeq() {
	if(!m_partialSeq)
		return m_seq;

	m_partialSeq = false;
	m_seq++;

	Seq const saveRange = (1u << (sizeof(ShortSeq) * 8u)) - SeqLowerMargin;

	if(unlikely(m_seq - m_seqLower > saveRange)) {
		Seq seqLower = m_seq;
		m_seqLower = m_seq - SeqLowerMargin;

		for(size_t i = 0; i < m_changes.size(); i++) {
			ObjectInfo& o = m_changes[i];
			Seq o_seq = toLong(o.seq);
			if(m_seq - o_seq > saveRange) {
				update(o.key, o.len, m_seqLower, 0, m_changes.size());
				seqLower = m_seqLower;
			} else
				seqLower = std::min(seqLower, o_seq);
		}

		m_seqLower = seqLower;
	}

	return m_seq;
}

StoreJournal::ShortSeq StoreJournal::toShort(StoreJournal::Seq seq) const {
	return (ShortSeq)seq;
}

StoreJournal::Seq StoreJournal::toLong(StoreJournal::ShortSeq seq) const {
	ShortSeq sseq = toShort(m_seq);
	if(sseq == seq)
		return m_seq;
	else if(sseq > seq)
		return m_seq - sseq + seq;
	else
		return m_seq - (1u << (sizeof(ShortSeq) * 8u)) - seq + sseq;
}

void StoreJournal::changed(StoreJournal::Key key, size_t len) {
	m_partialSeq = true;

	if(!update(key, len, seq(), 0, m_changes.size())) {
		m_changes.
#if STORED_cplusplus >= 201103L
			emplace_back
#else
			push_back
#endif
				(ObjectInfo(key, (Size)len, toShort(seq())));
		regenerate();
	}
}

bool StoreJournal::update(StoreJournal::Key key, size_t len, StoreJournal::Seq seq, size_t lower, size_t upper) {
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

void StoreJournal::regenerate() {
	std::sort(m_changes.begin(), m_changes.end(), ObjectInfoComparator());
	regenerate(0, m_changes.size());
}

StoreJournal::Seq StoreJournal::regenerate(size_t lower, size_t upper) {
	if(lower >= upper)
		return 0;

	size_t pivot = (upper - lower) / 2 + lower;
	ObjectInfo& o = m_changes[pivot];

	Seq highest =
		std::max(toLong(o.seq),
		std::max(regenerate(lower, pivot), regenerate(pivot + 1, upper)));

	o.highest = toShort(highest);
	return highest;
}

/*!
 * \brief Checks if the given object has changed since the given sequence number.
 *
 * The sequence number is the value returned by #encodeUpdates().
 */
bool StoreJournal::hasChanged(Key key, Seq since) const {
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
bool StoreJournal::hasChanged(Seq since) const {
	if(m_changes.empty())
		return false;

	size_t pivot = m_changes.size() / 2;
	return m_changes[pivot].highest >= since;
}

void StoreJournal::iterateChanged(StoreJournal::Seq since, IterateChangedCallback* cb, void* arg) {
	iterateChanged(since, cb, arg, 0, m_changes.size());
}

void StoreJournal::iterateChanged(StoreJournal::Seq since, IterateChangedCallback* cb, void* arg, size_t lower, size_t upper) {
	if(lower >= upper)
		return;

	size_t pivot = (upper - lower) / 2 + lower;
	ObjectInfo& o = m_changes[pivot];
	if(toLong(o.highest) < since)
		return;

	iterateChanged(since, cb, arg, lower, pivot);
	if(toLong(o.seq) >= since)
		cb(o.key, arg);
	iterateChanged(since, cb, arg, pivot + 1, upper);
}

void StoreJournal::clean(StoreJournal::Seq oldest) {
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

void StoreJournal::encodeHash(ProtocolLayer& p, bool last) {
	encodeHash(p, hash(), last);
}

void StoreJournal::encodeHash(ProtocolLayer& p, char const* hash, bool last) {
	size_t len = strlen(hash) + 1;
	stored_assert(len > sizeof(SyncConnection::Id)); // Otherwise SyncConnection::Bye cannot see the difference.
	p.encode(hash, len, last);
}

char const* StoreJournal::decodeHash(void*& buffer, size_t& len) {
	char* buffer_ = static_cast<char*>(buffer);
	size_t i;
	for(i = 0; i < len && buffer_[i]; i++);

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

StoreJournal::Seq StoreJournal::encodeBuffer(ProtocolLayer& p, bool last) {
	p.encode(buffer(), bufferSize(), last);
	return bumpSeq();
}

StoreJournal::Seq StoreJournal::decodeBuffer(void*& buffer, size_t& len) {
	if(len < bufferSize())
		return 0;

	memcpy(this->buffer(), buffer, bufferSize());
	len -= bufferSize();
	m_partialSeq = true;
	Seq seq = this->seq();
	for(Changes::iterator it = m_changes.begin(); it != m_changes.end(); ++it)
		it->seq = it->highest = toShort(seq);
	return bumpSeq();
}

/*!
 * \brief Encode all updates of changed objects since the given update sequence number (inclusive).
 * \return The sequence number of the most recent update, to be passed to the next invocation of \c encodeUpdates().
 */
StoreJournal::Seq StoreJournal::encodeUpdates(ProtocolLayer& p, StoreJournal::Seq sinceSeq, bool last) {
	encodeUpdates(p, sinceSeq, 0, m_changes.size());
	if(last)
		p.encode();
	return bumpSeq();
}

void StoreJournal::encodeUpdates(ProtocolLayer& p, StoreJournal::Seq sinceSeq, size_t lower, size_t upper) {
	if(lower >= upper)
		return;

	size_t pivot = (upper - lower) / 2 + lower;
	ObjectInfo& o = m_changes[pivot];
	if(toLong(o.highest) < sinceSeq)
		return;

	encodeUpdates(p, sinceSeq, lower, pivot);
	if(toLong(o.seq) >= sinceSeq)
		encodeUpdate(p, o);
	encodeUpdates(p, sinceSeq, pivot + 1, upper);
}

void StoreJournal::encodeUpdate(ProtocolLayer& p, StoreJournal::ObjectInfo& o) {
	encodeKey(p, o.key);
	encodeKey(p, o.len);
	p.encode(keyToBuffer(o.key), o.len, false);
}

StoreJournal::Seq StoreJournal::decodeUpdates(void*& buffer, size_t& len) {
	uint8_t* buffer_ = static_cast<uint8_t*>(buffer);
	bool ok = true;

	while(len) {
		Key key = decodeKey(buffer_, len, ok);
		Size size = (Size)decodeKey(buffer_, len, ok);
		void* obj = keyToBuffer(key, size, &ok);
		if(!ok || len < size) {
			buffer = buffer_;
			return 0;
		}

		memcpy(obj, buffer_, size);
		buffer_ += size;
		len -= size;
		changed(key, size);
	}

	return bumpSeq();
}

void StoreJournal::encodeKey(ProtocolLayer& p, StoreJournal::Key key) {
	size_t keysize = keySize();
	key = endian_h2n(key);
	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
	p.encode(reinterpret_cast<char*>(&key) + sizeof(key) - keysize, keysize, false);
}

StoreJournal::Key StoreJournal::decodeKey(uint8_t*& buffer, size_t& len, bool& ok) {
	Key key = 0;
	size_t i = keySize();
	if(i > len) {
		ok = false;
		return 0;
	}
	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
	memcpy(reinterpret_cast<char*>(&key) + sizeof(key) - i, buffer, i);
	len -= i;
	buffer += i;

	key = endian_n2h(key);
	return key;
}

void* StoreJournal::buffer() const { return m_buffer; }
size_t StoreJournal::bufferSize() const { return m_bufferSize; }
size_t StoreJournal::keySize() const { return m_keySize; }

void* StoreJournal::keyToBuffer(StoreJournal::Key key, StoreJournal::Size len, bool* ok) const {
	if(ok && len && key + len > bufferSize())
		*ok = false;
	return static_cast<char*>(m_buffer) + key;
}



/////////////////////////////
// SyncConnection
//

SyncConnection::SyncConnection(Synchronizer& synchronizer, ProtocolLayer& connection)
	: m_synchronizer(synchronizer)
	, m_idInNext(1)
{
	connection.wrap(*this);
}

SyncConnection::~SyncConnection() {
	bye();
}

Synchronizer& SyncConnection::synchronizer() const {
	return m_synchronizer;
}

void SyncConnection::encodeCmd(char cmd, bool last) {
	encode(&cmd, 1, last);
}

/*!
 * \brief Use this connection as a source of the given store.
 */
void SyncConnection::source(StoreJournal& store) {
	for(IdInMap::iterator it = m_idIn.begin(); it != m_idIn.end(); ++it)
		if(it->second == &store)
			// Already registered.
			return;

	encodeCmd(Hello);
	store.encodeHash(*this, false);
	Id id = nextId();
	m_idIn[id] = &store;
	encodeId(id, true);
}

/*!
 * \brief Determine a unique ID for others to send store updates to us.
 */
SyncConnection::Id SyncConnection::nextId() {
	stored_assert(m_idIn.size() < std::numeric_limits<Id>::max() / 2u);

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
void SyncConnection::drop(StoreJournal& store) {
	bool bye = false;

	Id id = 0;
	for(IdInMap::iterator it = m_idIn.begin(); !id && it != m_idIn.end(); ++it)
		if(it->second == &store)
			id = it->first;

	if(id && m_idIn.erase(id))
		bye = true;
	if(m_idOut.erase(&store))
		bye = true;
	if(m_seq.erase(&store))
		bye = true;

	if(!bye)
		return;

	encodeCmd(Bye);
	store.encodeHash(*this, true);
}

void SyncConnection::bye() {
	encodeCmd(Bye, true);
	m_seq.clear();
	m_idIn.clear();
	m_idOut.clear();
}

void SyncConnection::bye(char const* hash) {
	if(!hash)
		return;

	erase(hash);
	encodeCmd(Bye);
	StoreJournal::encodeHash(*this, hash, true);
}

void SyncConnection::erase(char const* hash) {
	if(!hash)
		return;

	StoreJournal* j = synchronizer().toJournal(hash);
	if(!j)
		return;

	m_seq.erase(j);
	m_idOut.erase(j);

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

void SyncConnection::bye(SyncConnection::Id id) {
	erase(id);
	encodeCmd(Bye);
	encodeId(id);
}

void SyncConnection::erase(SyncConnection::Id id) {
	{
		IdInMap::iterator it = m_idIn.find(id);
		if(it != m_idIn.end()) {
			m_seq.erase(it->second);
			m_idOut.erase(it->second);
			m_idIn.erase(it);
		}
	}

	StoreJournal* j = nullptr;

#if STORED_cplusplus >= 201103L
	for(IdOutMap::iterator it = m_idOut.begin(); it != m_idOut.end();)
		if(it->second == id) {
			j = it->first;
			it = m_idOut.erase(it);
		} else
			++it;
#else
	bool done;
	do {
		done = true;
		for(IdOutMap::iterator it = m_idOut.begin(); it != m_idOut.end(); ++it)
			if(it->second == id) {
				j = it->first;
				m_idOut.erase(it);
				done = false;
				break;
			}
	} while(!done);
#endif

	if(j)
		erase(j->hash());
}

/*!
 * \brief Send out all updates of te given store.
 */
void SyncConnection::process(StoreJournal& store) {
	SeqMap::iterator seq = m_seq.find(&store);
	if(seq == m_seq.end())
		// Unknown store.
		return;
	if(!store.hasChanged(seq->second))
		// No recent changes.
		return;

	IdOutMap::iterator id = m_idOut.find(&store);
	if(id == m_idOut.end())
		// No id related to this store.
		return;

	encodeCmd(Update);
	encodeId(id->second, false);
	// Make sure to set the process seq before the last part of the encode is sent.
	seq->second = store.encodeUpdates(*this, seq->second);
	encode();
}

char SyncConnection::decodeCmd(void*& buffer, size_t& len) {
	if(len == 0)
		return 0;

	len--;
	char* buffer_ = static_cast<char*>(buffer);
	buffer = buffer_ + 1;
	return buffer_[0];
}

void SyncConnection::decode(void* buffer, size_t len) {
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
			bye(hash);
			break;
		}

		encodeCmd(Welcome);
		encodeId(id);

		m_idOut[j] = id;
		id = nextId();
		m_idIn[id] = j;

		encodeId(id, false);
		m_seq[j] = j->encodeBuffer(*this, true);
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

		if(!welcome_id || j == m_idIn.end() ||
			!(seq = j->second->decodeBuffer(buffer, len)))
		{
			bye(id);
			break;
		}

		m_seq[j->second] = seq;
		m_idOut[j->second] = welcome_id;
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
		process(*it->second);

		StoreJournal::Seq seq = 0;
		if(!(seq = it->second->decodeUpdates(buffer, len))) {
			bye(id);
			break;
		}

		m_seq[it->second] = seq;
		break;
	}
	case Bye: {
		/*
		 * Bye
		 *
		 * "I do not need any more updates of the given store (by hash, by id or all)."
		 *
		 * 'b' hash
		 * 'b' id
		 * 'b'
		 */
		if(len == 0) {
			m_seq.clear();
			m_idIn.clear();
			m_idOut.clear();
		} else if(len == sizeof(Id)) {
			Id id = decodeId(buffer, len);
			if(!id)
				break;

			IdInMap::iterator it = m_idIn.find(id);
			if(it == m_idIn.end())
				break;

			erase(id);
		} else {
			char const* hash = StoreJournal::decodeHash(buffer, len);
			erase(hash);
		}
		break;
	}
	default:;
		// Other; ignore.
	}
}

void SyncConnection::encodeId(SyncConnection::Id id, bool last) {
	id = endian_h2s(id);
	encode(&id, sizeof(Id), last);
}

SyncConnection::Id SyncConnection::decodeId(void*& buffer, size_t& len) {
	if(len < sizeof(Id))
		return 0;

	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
	Id id = endian_s2h(*reinterpret_cast<Id*>(buffer));

	len -= sizeof(Id);
	buffer = static_cast<char*>(buffer) + sizeof(Id);
	return id;
}




/////////////////////////////
// Synchronizer
//

Synchronizer::~Synchronizer() {
	for(Connections::iterator it = m_connections.begin(); it != m_connections.end(); ++it)
		// NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
		delete *it;
	m_connections.clear();
}

StoreJournal* Synchronizer::toJournal(char const* hash) const {
	if(!hash)
		return nullptr;
	StoreMap::const_iterator it = m_storeMap.find(hash);
	return it == m_storeMap.end() ? nullptr : it->second;
}

void Synchronizer::connect(ProtocolLayer& connection) {
	// NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
	SyncConnection* c = new SyncConnection(*this, connection);
	m_connections.insert(c);
}

void Synchronizer::disconnect(ProtocolLayer& connection) {
	SyncConnection* c = toConnection(connection);
	if(c) {
		m_connections.erase(c);
		// NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
		delete c;
	}
}

SyncConnection* Synchronizer::toConnection(ProtocolLayer& connection) const {
	ProtocolLayer* c = connection.up();
	if(!c)
		return nullptr;
	if(m_connections.find(c) == m_connections.end())
		return nullptr;

	stored_assert(dynamic_cast<SyncConnection*>(c) != nullptr);
	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
	return static_cast<SyncConnection*>(c);
}

void Synchronizer::process() {
	for(StoreMap::iterator it = m_storeMap.begin(); it != m_storeMap.end(); ++it)
		process(*it->second);
}

void Synchronizer::process(StoreJournal& j) {
	for(Connections::iterator it = m_connections.begin(); it != m_connections.end(); ++it)
		// NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
		static_cast<SyncConnection*>(*it)->process(j);
}

void Synchronizer::process(ProtocolLayer& connection) {
	SyncConnection* c = toConnection(connection);
	if(!c)
		return;

	for(StoreMap::iterator it = m_storeMap.begin(); it != m_storeMap.end(); ++it)
		c->process(*it->second);
}

void Synchronizer::process(ProtocolLayer& connection, StoreJournal& j) {
	SyncConnection* c = toConnection(connection);
	if(c)
		c->process(j);
}

} // namespace

