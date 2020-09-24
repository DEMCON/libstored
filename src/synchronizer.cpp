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

StoreJournal::Id StoreJournal::m_idNext;

StoreJournal::StoreJournal(void* buffer, size_t size)
	: m_buffer(buffer)
	, m_bufferSize(size)
	, m_keySize(keySize(m_bufferSize))
	, m_id(m_idNext++)
	, m_seq()
	, m_seqLower()
	, m_partialSeq()
{
	// Should not wrap-around, as ids may not be unique anymore in that case.
	stored_assert(m_idNext);

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

StoreJournal::Id StoreJournal::id() const {
	return m_id;
}

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
		size_t seqLower = m_seq;
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
	if(!m_partialSeq) {
		m_partialSeq = true;
		m_seq++;
	}

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

bool StoreJournal::hasChanged(Key key, Seq since) const {
	size_t lower = 0;
	size_t upper = m_changes.size();

	while(lower < upper) {
		size_t pivot = (upper - lower) / 2 + lower;

		ObjectInfo const& o = m_changes[pivot];

		if(o.highest < since)
			return false;
		if(o.key == key)
			return o.seq >= since;

		if(o.key > key)
			upper = pivot;
		else
			lower = pivot + 1;
	}

	return false;
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

void StoreJournal::encodeId(ProtocolLayer& p, bool last) {
	p.encode(&m_id, sizeof(Id), last);
}

void StoreJournal::encodeBuffer(ProtocolLayer& p, bool last) {
	p.encode(buffer(), bufferSize(), last);
}

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

bool StoreJournal::decodeUpdates(void* buffer, size_t len) {
	uint8_t* buffer_ = static_cast<uint8_t*>(buffer);
	bool ok = true;

	while(len) {
		Key key = decodeKey(buffer_, len, ok);
		Size size = (Size)decodeKey(buffer_, len, ok);
		void* obj = keyToBuffer(key, size, &ok);
		if(!ok || len < size)
			return false;

		memcpy(obj, buffer_, size);
		buffer_ += size;
		len -= size;
	}

	return true;
}

void StoreJournal::encodeKey(ProtocolLayer& p, StoreJournal::Key key) {
	uint8_t buffer[sizeof(Key)];
	size_t keysize = keySize();

	for(size_t i = 0; i < keysize; i++)
		// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
		buffer[i] = (uint8_t)(key >> ((keysize - i - 1) * 8));

	p.encode(buffer, keysize, false);
}

StoreJournal::Key StoreJournal::decodeKey(uint8_t*& buffer, size_t& len, bool& ok) {
	Key key = 0;
	size_t i = keySize();
	if(i < len) {
		ok = false;
		return 0;
	}
	len -= i;
	for(; i > 0; i--, buffer++)
		key = (key << 8u) | *buffer;
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

} // namespace

