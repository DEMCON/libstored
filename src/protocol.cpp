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

#include <libstored/protocol.h>
#include <libstored/util.h>

#ifndef STORED_OS_BAREMETAL
#  ifdef STORED_COMPILER_MSVC
#    include <io.h>
#  else
#    include <unistd.h>
#  endif
#endif

#if defined(STORED_OS_WINDOWS)
#  define write(fd, buffer, count) _write(fd, buffer, (unsigned int)(count))
#endif

#include <algorithm>
#include <new>

namespace stored {



//////////////////////////////
// ProtocolLayer
//

/*!
 * \brief Destructor.
 *
 * Ties to the layer above and below are nicely removed.
 */
ProtocolLayer::~ProtocolLayer() {
	if(up() && up()->down() == this)
		up()->setDown(down());
	if(down() && down()->up() == this)
		down()->setUp(up());
}




//////////////////////////////
// AsciiEscapeLayer
//

/*!
 * \copydoc stored::ProtocolLayer::ProtocolLayer()
 */
AsciiEscapeLayer::AsciiEscapeLayer(bool all, ProtocolLayer* up, ProtocolLayer* down)
	: base(up, down)
	, m_all(all)
{}

void AsciiEscapeLayer::decode(void* buffer, size_t len) {
	char* p = static_cast<char*>(buffer);

	// Common case: there is no escape character.
	size_t i = 0;
	for(; i + 1 < len; i++)
		if(unlikely(p[i] == Esc))
			goto first_escape;

	// No escape characters.
	base::decode(buffer, len);
	return;

first_escape:
	// Process escape sequences in-place.
	size_t decodeOffset = i;

escape:
	if(p[++i] == Esc)
		p[decodeOffset] = (uint8_t)Esc;
	else
		p[decodeOffset] = (uint8_t)p[i] & (uint8_t)EscMask;
	decodeOffset++;
	i++;

	// The + 1 prevents it from trying to decode an escape character at the end.
	for(; i + 1 < len; i++, decodeOffset++) {
		if(unlikely(p[i] == Esc))
			goto escape;
		else
			p[decodeOffset] = p[i];
	}

	// Always add the last character (if any).
	if(i < len)
		p[decodeOffset++] = p[i];
	base::decode(p, decodeOffset);
}

char AsciiEscapeLayer::needEscape(char c) {
	if(!((uint8_t)c & (uint8_t)~(uint8_t)AsciiEscapeLayer::EscMask)) {
		if(!m_all) {
			// Only escape what conflicts with other protocols.
			switch(c) {
			case '\0':
			case '\x11': // XON
			case '\x13': // XOFF
			case '\x1b': // ESC
			case '\r':
				// Do escape \r, as Windows may inject it automatically.  So, if \r
				// is meant to be sent, escape it, such that the client may remove
				// all (unescaped) \r's automatically.
				break;

			default:
				// Don't escape.
				return 0;
			}
		}
		return (char)((uint8_t)c | 0x40u);
	} else if(c == AsciiEscapeLayer::Esc) {
		return c;
	} else {
		return 0;
	}
}

void AsciiEscapeLayer::encode(void const* buffer, size_t len, bool last) {
	uint8_t const* p = static_cast<uint8_t const*>(buffer);
	uint8_t const* chunk = p;

	for(size_t i = 0; i < len; i++) {
		char escaped = needEscape((char)p[i]);
		if(unlikely(escaped)) {
			// This is a to-be-escaped byte.
			if(chunk < p + i)
				base::encode(chunk, (size_t)(p + i - chunk), false);

			uint8_t const esc[2] = { AsciiEscapeLayer::Esc, (uint8_t)escaped };
			base::encode(esc, sizeof(esc), last && i + 1 == len);
			chunk = p + i + 1;
		}
	}

	if(likely(chunk < p + len) || (len == 0 && last))
		base::encode(chunk, (size_t)(p + len - chunk), last);
}

size_t AsciiEscapeLayer::mtu() const {
	size_t mtu = base::mtu();
	if(mtu == 0u)
		return 0u;
	if(mtu == 1u)
		return 1u;
	return mtu / 2u;
}





//////////////////////////////
// TerminalLayer
//

/*!
 * \copydoc stored::ProtocolLayer::ProtocolLayer(ProtocolLayer*,ProtocolLayer*)
 * \param nonDebugDecodeFd the file descriptor to write data to that are not part of debug messages during decode(). Set to -1 to drop this data.
 * \param encodeFd the file descriptor to write encoded debug messages to. Set to -1 to drop this data.
 */
TerminalLayer::TerminalLayer(int nonDebugDecodeFd, int encodeFd, ProtocolLayer* up, ProtocolLayer* down)
	: base(up, down)
	, m_nonDebugDecodeFd(nonDebugDecodeFd)
	, m_encodeFd(encodeFd)
	, m_decodeState(StateNormal)
	, m_encodeState()
{
}

void TerminalLayer::reset() {
	m_decodeState = StateNormal;
	m_encodeState = false;
	m_buffer.clear();
	base::reset();
}

/*!
 * \copydoc stored::ProtocolLayer::~ProtocolLayer()
 */
TerminalLayer::~TerminalLayer() is_default

void TerminalLayer::decode(void* buffer, size_t len) {
	size_t nonDebugOffset = m_decodeState < StateDebug ? 0 : len;

	for(size_t i = 0; i < len; i++) {
		char c = (static_cast<char*>(buffer))[i];

		switch(m_decodeState) {
		case StateNormal:
			if(unlikely(c == Esc))
				m_decodeState = StateNormalEsc;
			break;
		case StateNormalEsc:
			if(likely(c == EscStart)) {
				nonDebugDecode(static_cast<char*>(buffer) + nonDebugOffset, i - nonDebugOffset - 1); // Also skip the ESC
				m_decodeState = StateDebug;
				nonDebugOffset = len;
			} else
				m_decodeState = StateNormal;
			break;
		case StateDebug:
			if(unlikely(c == Esc))
				m_decodeState = StateDebugEsc;
			else
				m_buffer.push_back(c);
			break;
		case StateDebugEsc:
			if(likely(c == EscEnd)) {
				base::decode(&m_buffer[0], m_buffer.size());
				m_decodeState = StateNormal;
				m_buffer.clear();
				nonDebugOffset = i + 1;
			} else {
				m_decodeState = StateDebug;
				m_buffer.push_back((char)Esc);
				m_buffer.push_back(c);
			}
			break;
		}
	}

	if(nonDebugOffset < len)
		nonDebugDecode(static_cast<char*>(buffer) + nonDebugOffset, len - nonDebugOffset);
}

/*!
 * \brief Receptor of non-debug data during decode().
 *
 * Default implementation writes to the \c nonDebugDecodeFd, as supplied to the constructor.
 */
void TerminalLayer::nonDebugDecode(void* buffer, size_t len) {
	writeToFd(m_nonDebugDecodeFd, buffer, len);
}

#ifndef STORED_OS_BAREMETAL
/*!
 * \brief Helper function to write a buffer to a file descriptor.
 *
 * By default calls #writeToFd_().
 */
void TerminalLayer::writeToFd(int fd, void const* buffer, size_t len) {
	writeToFd_(fd, buffer, len);
}

/*!
 * \brief Write the given buffer to a file descriptor.
 */
void TerminalLayer::writeToFd_(int fd, void const* buffer, size_t len) {
	if(fd < 0)
		return;

	ssize_t res = 0;
	for(size_t i = 0; res >= 0 && i < len; i += (size_t)res)
		res = write(fd, static_cast<char const*>(buffer) + i, (len - i));
}
#endif

void TerminalLayer::encode(void const* buffer, size_t len, bool last) {
	encodeStart();
	writeToFd(m_encodeFd, buffer, len);
	base::encode(buffer, len, false);
	if(last)
		encodeEnd();
}

/*!
 * \brief Emits a start-of-frame sequence if it hasn't done yet.
 *
 * Call #encodeEnd() to finish the current frame.
 */
void TerminalLayer::encodeStart() {
	if(m_encodeState)
		return;

	m_encodeState = true;
	char start[2] = {Esc, EscStart};
	writeToFd(m_encodeFd, start, sizeof(start));
	base::encode((void*)start, sizeof(start), false);
}

/*!
 * \brief Emits an end-of-frame sequence of the frame started using #encodeStart().
 */
void TerminalLayer::encodeEnd() {
	if(!m_encodeState)
		return;

	char end[2] = {Esc, EscEnd};
	writeToFd(m_encodeFd, end, sizeof(end));
	base::encode((void*)end, sizeof(end), true);
	m_encodeState = false;
}

size_t TerminalLayer::mtu() const {
	size_t mtu = base::mtu();
	if(mtu == 0)
		return 0;
	if(mtu <= 4)
		return 1;
	return mtu - 4;
}






//////////////////////////////
// SegmentationLayer
//

/*!
 * \brief Ctor.
 */
SegmentationLayer::SegmentationLayer(size_t mtu, ProtocolLayer* up, ProtocolLayer* down)
	: base(up, down)
	, m_mtu(mtu)
	, m_encoded()
{
}

void SegmentationLayer::reset() {
	m_decode.clear();
	m_encoded = 0;
	base::reset();
}

size_t SegmentationLayer::mtu() const {
	// We segment, so all layers above can use any size they want.
	return 0;
}

/*!
 * \brief Returns the MTU used to split messages into.
 */
size_t SegmentationLayer::lowerMtu() const {
	size_t lower_mtu = base::mtu();
	if(!m_mtu)
		return lower_mtu;
	else if(!lower_mtu)
		return m_mtu;
	else
		return std::min<size_t>(m_mtu, lower_mtu);
}

void SegmentationLayer::decode(void* buffer, size_t len) {
	if(len == 0)
		return;

	char* buffer_ = static_cast<char*>(buffer);
	if(!m_decode.empty() || buffer_[len - 1] != EndMarker) {
		// Save for later packet reassembling.
		if(len > 1) {
			size_t start = m_decode.size();
			m_decode.resize(start + len - 1);
			memcpy(&m_decode[start], buffer_, len - 1);
		}

		if(buffer_[len - 1] == EndMarker) {
			// Got it.
			base::decode(&m_decode[0], m_decode.size());
			m_decode.clear();
		}
	} else {
		// Full packet is in buffer. Forward immediately.
		base::decode(buffer, len - 1);
	}
}

void SegmentationLayer::encode(void const* buffer, size_t len, bool last) {
	char const* buffer_ = static_cast<char const*>(buffer);

	size_t mtu = lowerMtu();
	if(mtu == 0)
		mtu = std::numeric_limits<size_t>::max();
	else if(mtu == 1)
		mtu = 2;

	while(len) {
		size_t remaining = mtu - m_encoded - 1;
		size_t chunk = std::min(len, remaining);

		if(chunk) {
			base::encode(buffer_, chunk, false);
			len -= chunk;
			buffer_ += chunk;
		}

		if(chunk == remaining && len) {
			// Full MTU.
			char cont = ContinueMarker;
			base::encode(&cont, 1, true);
			m_encoded = 0;
		} else {
			// Partial MTU. Record that we already filled some of the packet.
			m_encoded += chunk;
		}
	}

	if(last) {
		// The marker always ends the packet.
		char end = EndMarker;
		base::encode(&end, 1, true);
		m_encoded = 0;
	}
}



//////////////////////////////
// ArqLayer
//

ArqLayer::ArqLayer(size_t maxEncodeBuffer, ProtocolLayer* up, ProtocolLayer* down)
	: base(up, down)
	, m_maxEncodeBuffer(maxEncodeBuffer)
{
}


//////////////////////////////
// DebugArqLayer
//

/*!
 * \brief Ctor.
 */
DebugArqLayer::DebugArqLayer(size_t maxEncodeBuffer, ProtocolLayer* up, ProtocolLayer* down)
	: base(up, down)
	, m_decodeState(DecodeStateIdle)
	, m_decodeSeq(1)
	, m_decodeSeqStart()
	, m_encodeState(EncodeStateIdle)
	, m_encodeSeq(1)
	, m_encodeSeqReset(true)
	, m_maxEncodeBuffer(maxEncodeBuffer)
	, m_encodeBufferSize()
{
}

void DebugArqLayer::reset() {
	m_decodeState = DecodeStateIdle;
	m_decodeSeq = 1;
	m_decodeSeqStart = 0;
	m_encodeState = EncodeStateIdle;
	m_encodeSeq = 1;
	m_encodeSeqReset = true;
	m_encodeBuffer.clear();
	m_encodeBufferSize = 0;
	base::reset();
}

void DebugArqLayer::decode(void* buffer, size_t len) {
	if(len == 0)
		return;

	uint8_t* buffer_ = static_cast<uint8_t*>(buffer);
	uint8_t flags = *buffer_;
	uint32_t seq = decodeSeq(buffer_, len);
	if(unlikely(flags & ResetFlag)) {
		// Reset communication state.
		m_decodeState = DecodeStateIdle;
		m_encodeState = EncodeStateIdle;
		m_encodeBuffer.clear();
		m_encodeBufferSize = 0;
		m_decodeSeq = nextSeq(seq);
		m_encodeSeq = 1;
		setPurgeableResponse(false);
		uint8_t ack = ResetFlag;
		base::encode(&ack, sizeof(ack), true);
		m_encodeSeqReset = false;
	}

	switch(m_decodeState) {
	case DecodeStateDecoded:
		stored_assert(m_encodeState == EncodeStateIdle || m_encodeState == EncodeStateUnbufferedIdle);

		if(seq == m_decodeSeq) {
			// This is the next command. The previous one was received, apparently.
			m_decodeState = DecodeStateIdle;
			m_encodeState = EncodeStateIdle;
			m_encodeBuffer.clear();
			m_encodeBufferSize = 0;
			setPurgeableResponse(false);
		} else
			// fall-through
	case DecodeStateRetransmit:
		if(seq == m_decodeSeqStart) {
			// This seems to be a retransmit of the current command.
			switch(m_encodeState) {
			case EncodeStateUnbufferedIdle:
				// We must reexecute the command. Reset the sequence number, as the content may change.
				setPurgeableResponse(false);
				m_decodeSeq = m_decodeSeqStart;
				m_decodeState = DecodeStateIdle;
				m_encodeSeqReset = true;
				break;
			case EncodeStateIdle:
				// Wait for full retransmit of the command, but do not actually decode.
				m_decodeState = DecodeStateRetransmit;
				break;
			default:
				stored_assert(false); // NOLINT(hicpp-static-assert,misc-static-assert)
			}
		} // else: unexpected seq; ignore.
		break;
	default:;
	}

	switch(m_decodeState) {
	case DecodeStateRetransmit:
		if(nextSeq(seq) == m_decodeSeq) {
			// Got the last part of the command. Retransmit the response buffer.
			for(std::vector<std::string>::const_iterator it = m_encodeBuffer.begin(); it != m_encodeBuffer.end(); ++it)
				base::encode(it->data(), it->size(), true);
			m_decodeState = DecodeStateDecoded;
		}
		break;
	case DecodeStateIdle:
		m_decodeSeqStart = m_decodeSeq;
		m_decodeState = DecodeStateDecoding;
		// fall-through
	case DecodeStateDecoding:
		if(likely(seq == m_decodeSeq)) {
			// Properly in sequence.
			base::decode(buffer_, len);
			m_decodeSeq = nextSeq(m_decodeSeq);

			// Should not wrap-around during a request.
			stored_assert(m_decodeSeq != m_decodeSeqStart);
		}
		break;
	default:;
	}
}

void DebugArqLayer::encode(void const* buffer, size_t len, bool last) {
	if(m_decodeState == DecodeStateDecoding) {
		// This seems to be the first part of the response.
		// So, the request message must have been complete.
		m_decodeState = DecodeStateDecoded;
		stored_assert(m_encodeState == EncodeStateIdle || m_encodeState == EncodeStateUnbufferedIdle);
	}

	switch(m_encodeState) {
	case EncodeStateIdle:
	case EncodeStateEncoding:
		if(unlikely(m_maxEncodeBuffer > 0 && m_encodeBufferSize + len > m_maxEncodeBuffer)) {
			// Overflow.
			setPurgeableResponse();
		}
		break;
	default:;
	}

	uint8_t seq[4];
	size_t seqlen = 0;

	switch(m_encodeState) {
	case EncodeStateUnbufferedIdle:
	case EncodeStateIdle:
		seqlen = encodeSeq(m_encodeSeq, seq);
		stored_assert(seqlen <= sizeof(seq));
		m_encodeSeq = nextSeq(m_encodeSeq);
		// Assume a wrap-around will be noticed, as it contains at least 128 MB of data.
		if(m_encodeSeqReset) {
			seq[0] = (uint8_t)(seq[0] | ResetFlag);
			m_encodeSeqReset = false;
		}
		break;
	default:;
	}

	switch(m_encodeState) {
	case EncodeStateUnbufferedIdle:
		base::encode(seq, seqlen, false);
		m_encodeState = EncodeStateUnbufferedEncoding;
		// fall-through
	case EncodeStateUnbufferedEncoding:
		base::encode(buffer, len, last);
		if(last)
			m_encodeState = EncodeStateUnbufferedIdle;
		break;

	case EncodeStateIdle:
#if STORED_cplusplus >= 201103L
		// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
		m_encodeBuffer.emplace_back(reinterpret_cast<char*>(seq), seqlen);
#else
		// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
		m_encodeBuffer.push_back(std::string(reinterpret_cast<char*>(seq), seqlen));
#endif
		m_encodeState = EncodeStateEncoding;
		m_encodeBufferSize += seqlen;
		// fall-through
	case EncodeStateEncoding:
		stored_assert(!m_encodeBuffer.empty());
		m_encodeBufferSize += len;
		m_encodeBuffer.back().append(static_cast<char const*>(buffer), len);

		if(last) {
			base::encode(m_encodeBuffer.back().data(), m_encodeBuffer.back().size(), true);
			m_encodeState = EncodeStateIdle;
		}
	}
}

void DebugArqLayer::setPurgeableResponse(bool purgeable) {
	bool wasPurgeable = m_encodeState == EncodeStateUnbufferedIdle || m_encodeState == EncodeStateUnbufferedEncoding;

	if(wasPurgeable == purgeable)
		return;

	if(purgeable) {
		// Switch to purgeable.
		base::setPurgeableResponse(true);

		switch(m_encodeState) {
		case EncodeStateEncoding: {
			std::string const& s = m_encodeBuffer.back();
			base::encode(s.data(), s.size(), false);
			m_encodeState = EncodeStateUnbufferedEncoding;
			break; }
		case EncodeStateIdle:
			m_encodeState = EncodeStateUnbufferedIdle;
			break;
		default:
			stored_assert(false); // NOLINT(hicpp-static-assert,misc-static-assert)
		}

		m_encodeBuffer.clear();
		m_encodeBufferSize = 0;
	} else {
		// Switch to precious.
		switch(m_encodeState) {
		case EncodeStateUnbufferedEncoding:
			// First part already sent. Can't switch to precious right now.
			// Wait till start of next command.
			break;
		case EncodeStateUnbufferedIdle:
			base::setPurgeableResponse(false);
			m_encodeState = EncodeStateIdle;
			break;
		default:
			stored_assert(false); // NOLINT(hicpp-static-assert,misc-static-assert)
		}
	}
}

size_t DebugArqLayer::mtu() const {
	size_t mtu = base::mtu();
	if(mtu == 0)
		return 0;
	if(mtu <= 4u)
		return 1u;
	return mtu - 4u;
}

/*!
 * \brief Compute the successive sequence number.
 */
uint32_t DebugArqLayer::nextSeq(uint32_t seq) {
	seq = (uint32_t)((seq + 1u) % 0x8000000);
	return seq ? seq : 1u;
}

/*!
 * \brief Decode the sequence number from a buffer.
 */
uint32_t DebugArqLayer::decodeSeq(uint8_t*& buffer, size_t& len) {
	uint32_t seq = 0;
	uint8_t flag = 0x40;

	while(true) {
		if(!len--)
			return ~0u;
		seq = (seq << 7u) | (*buffer & (flag - 1u));
		if(!(*buffer++ & flag))
			return seq;
		flag = 0x80;
	}
}

/*!
 * \brief Encode a sequence number into a buffer.
 * \return the number of bytes written (maximum of 4)
 */
size_t DebugArqLayer::encodeSeq(uint32_t seq, void* buffer) {
	uint8_t* buffer_ = static_cast<uint8_t*>(buffer);
	if(seq < 0x40u) {
		buffer_[0] = (uint8_t)(seq & 0x3fu);
		return 1;
	} else if(seq < 0x2000) {
		buffer_[0] = (uint8_t)(0x40u | ((seq >> 7u) & 0x3fu));
		buffer_[1] = (uint8_t)(seq & 0x7fu);
		return 2;
	} else if(seq < 0x100000) {
		buffer_[0] = (uint8_t)(0x40u | ((seq >> 14u) & 0x3fu));
		buffer_[1] = (uint8_t)(0x80u | ((seq >> 7u) & 0x7fu));
		buffer_[2] = (uint8_t)(seq & 0x7fu);
		return 3;
	} else if(seq < 0x8000000) {
		buffer_[0] = (uint8_t)(0x40u | ((seq >> 21u) & 0x3fu));
		buffer_[1] = (uint8_t)(0x80u | ((seq >> 14u) & 0x7fu));
		buffer_[2] = (uint8_t)(0x80u | ((seq >> 7u) & 0x7fu));
		buffer_[3] = (uint8_t)(seq & 0x7fu);
		return 4;
	} else {
		return encodeSeq(seq % 0x8000000, buffer);
	}
}





//////////////////////////////
// Crc8Layer
//

// Generated by http://www.sunshine2k.de/coding/javascript/crc/crc_js.html
static const uint8_t crc8_table[] = {
	0x00, 0xA6, 0xEA, 0x4C, 0x72, 0xD4, 0x98, 0x3E, 0xE4, 0x42, 0x0E, 0xA8, 0x96, 0x30, 0x7C, 0xDA,
	0x6E, 0xC8, 0x84, 0x22, 0x1C, 0xBA, 0xF6, 0x50, 0x8A, 0x2C, 0x60, 0xC6, 0xF8, 0x5E, 0x12, 0xB4,
	0xDC, 0x7A, 0x36, 0x90, 0xAE, 0x08, 0x44, 0xE2, 0x38, 0x9E, 0xD2, 0x74, 0x4A, 0xEC, 0xA0, 0x06,
	0xB2, 0x14, 0x58, 0xFE, 0xC0, 0x66, 0x2A, 0x8C, 0x56, 0xF0, 0xBC, 0x1A, 0x24, 0x82, 0xCE, 0x68,
	0x1E, 0xB8, 0xF4, 0x52, 0x6C, 0xCA, 0x86, 0x20, 0xFA, 0x5C, 0x10, 0xB6, 0x88, 0x2E, 0x62, 0xC4,
	0x70, 0xD6, 0x9A, 0x3C, 0x02, 0xA4, 0xE8, 0x4E, 0x94, 0x32, 0x7E, 0xD8, 0xE6, 0x40, 0x0C, 0xAA,
	0xC2, 0x64, 0x28, 0x8E, 0xB0, 0x16, 0x5A, 0xFC, 0x26, 0x80, 0xCC, 0x6A, 0x54, 0xF2, 0xBE, 0x18,
	0xAC, 0x0A, 0x46, 0xE0, 0xDE, 0x78, 0x34, 0x92, 0x48, 0xEE, 0xA2, 0x04, 0x3A, 0x9C, 0xD0, 0x76,
	0x3C, 0x9A, 0xD6, 0x70, 0x4E, 0xE8, 0xA4, 0x02, 0xD8, 0x7E, 0x32, 0x94, 0xAA, 0x0C, 0x40, 0xE6,
	0x52, 0xF4, 0xB8, 0x1E, 0x20, 0x86, 0xCA, 0x6C, 0xB6, 0x10, 0x5C, 0xFA, 0xC4, 0x62, 0x2E, 0x88,
	0xE0, 0x46, 0x0A, 0xAC, 0x92, 0x34, 0x78, 0xDE, 0x04, 0xA2, 0xEE, 0x48, 0x76, 0xD0, 0x9C, 0x3A,
	0x8E, 0x28, 0x64, 0xC2, 0xFC, 0x5A, 0x16, 0xB0, 0x6A, 0xCC, 0x80, 0x26, 0x18, 0xBE, 0xF2, 0x54,
	0x22, 0x84, 0xC8, 0x6E, 0x50, 0xF6, 0xBA, 0x1C, 0xC6, 0x60, 0x2C, 0x8A, 0xB4, 0x12, 0x5E, 0xF8,
	0x4C, 0xEA, 0xA6, 0x00, 0x3E, 0x98, 0xD4, 0x72, 0xA8, 0x0E, 0x42, 0xE4, 0xDA, 0x7C, 0x30, 0x96,
	0xFE, 0x58, 0x14, 0xB2, 0x8C, 0x2A, 0x66, 0xC0, 0x1A, 0xBC, 0xF0, 0x56, 0x68, 0xCE, 0x82, 0x24,
	0x90, 0x36, 0x7A, 0xDC, 0xE2, 0x44, 0x08, 0xAE, 0x74, 0xD2, 0x9E, 0x38, 0x06, 0xA0, 0xEC, 0x4A
};

/*!
 * \brief Ctor.
 */
Crc8Layer::Crc8Layer(ProtocolLayer* up, ProtocolLayer* down)
	: base(up, down)
	, m_crc(init)
{
}

void Crc8Layer::reset() {
	m_crc = init;
	base::reset();
}

void Crc8Layer::decode(void* buffer, size_t len) {
	if(len == 0)
		return;

	uint8_t* buffer_ = static_cast<uint8_t*>(buffer);
	uint8_t crc = init;
	for(size_t i = 0; i < len - 1; i++)
		crc = compute(crc, buffer_[i]);

	if(crc != buffer_[len - 1])
		// Invalid.
		return;

	base::decode(buffer, len - 1);
}

void Crc8Layer::encode(void const* buffer, size_t len, bool last) {
	uint8_t const* buffer_ = static_cast<uint8_t const*>(buffer);
	for(size_t i = 0; i < len; i++)
		m_crc = compute(m_crc, buffer_[i]);

	base::encode(buffer, len, false);

	if(last) {
		base::encode(&m_crc, 1, true);
		m_crc = init;
	}
}

uint8_t Crc8Layer::compute(uint8_t input, uint8_t crc) {
	// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
	return crc8_table[(uint8_t)(input ^ crc)];
}

size_t Crc8Layer::mtu() const {
	size_t mtu = base::mtu();
	if(mtu == 0 || mtu > 256u)
		return 256u;
	if(mtu <= 2u)
		return 1u;
	return mtu - 1u;
}





//////////////////////////////
// Crc16Layer
//

// Generated by http://www.sunshine2k.de/coding/javascript/crc/crc_js.html
static const uint16_t crc16_table[] = {
	0x0000, 0xBAAD, 0xCFF7, 0x755A, 0x2543, 0x9FEE, 0xEAB4, 0x5019, 0x4A86, 0xF02B, 0x8571, 0x3FDC, 0x6FC5, 0xD568, 0xA032, 0x1A9F,
	0x950C, 0x2FA1, 0x5AFB, 0xE056, 0xB04F, 0x0AE2, 0x7FB8, 0xC515, 0xDF8A, 0x6527, 0x107D, 0xAAD0, 0xFAC9, 0x4064, 0x353E, 0x8F93,
	0x90B5, 0x2A18, 0x5F42, 0xE5EF, 0xB5F6, 0x0F5B, 0x7A01, 0xC0AC, 0xDA33, 0x609E, 0x15C4, 0xAF69, 0xFF70, 0x45DD, 0x3087, 0x8A2A,
	0x05B9, 0xBF14, 0xCA4E, 0x70E3, 0x20FA, 0x9A57, 0xEF0D, 0x55A0, 0x4F3F, 0xF592, 0x80C8, 0x3A65, 0x6A7C, 0xD0D1, 0xA58B, 0x1F26,
	0x9BC7, 0x216A, 0x5430, 0xEE9D, 0xBE84, 0x0429, 0x7173, 0xCBDE, 0xD141, 0x6BEC, 0x1EB6, 0xA41B, 0xF402, 0x4EAF, 0x3BF5, 0x8158,
	0x0ECB, 0xB466, 0xC13C, 0x7B91, 0x2B88, 0x9125, 0xE47F, 0x5ED2, 0x444D, 0xFEE0, 0x8BBA, 0x3117, 0x610E, 0xDBA3, 0xAEF9, 0x1454,
	0x0B72, 0xB1DF, 0xC485, 0x7E28, 0x2E31, 0x949C, 0xE1C6, 0x5B6B, 0x41F4, 0xFB59, 0x8E03, 0x34AE, 0x64B7, 0xDE1A, 0xAB40, 0x11ED,
	0x9E7E, 0x24D3, 0x5189, 0xEB24, 0xBB3D, 0x0190, 0x74CA, 0xCE67, 0xD4F8, 0x6E55, 0x1B0F, 0xA1A2, 0xF1BB, 0x4B16, 0x3E4C, 0x84E1,
	0x8D23, 0x378E, 0x42D4, 0xF879, 0xA860, 0x12CD, 0x6797, 0xDD3A, 0xC7A5, 0x7D08, 0x0852, 0xB2FF, 0xE2E6, 0x584B, 0x2D11, 0x97BC,
	0x182F, 0xA282, 0xD7D8, 0x6D75, 0x3D6C, 0x87C1, 0xF29B, 0x4836, 0x52A9, 0xE804, 0x9D5E, 0x27F3, 0x77EA, 0xCD47, 0xB81D, 0x02B0,
	0x1D96, 0xA73B, 0xD261, 0x68CC, 0x38D5, 0x8278, 0xF722, 0x4D8F, 0x5710, 0xEDBD, 0x98E7, 0x224A, 0x7253, 0xC8FE, 0xBDA4, 0x0709,
	0x889A, 0x3237, 0x476D, 0xFDC0, 0xADD9, 0x1774, 0x622E, 0xD883, 0xC21C, 0x78B1, 0x0DEB, 0xB746, 0xE75F, 0x5DF2, 0x28A8, 0x9205,
	0x16E4, 0xAC49, 0xD913, 0x63BE, 0x33A7, 0x890A, 0xFC50, 0x46FD, 0x5C62, 0xE6CF, 0x9395, 0x2938, 0x7921, 0xC38C, 0xB6D6, 0x0C7B,
	0x83E8, 0x3945, 0x4C1F, 0xF6B2, 0xA6AB, 0x1C06, 0x695C, 0xD3F1, 0xC96E, 0x73C3, 0x0699, 0xBC34, 0xEC2D, 0x5680, 0x23DA, 0x9977,
	0x8651, 0x3CFC, 0x49A6, 0xF30B, 0xA312, 0x19BF, 0x6CE5, 0xD648, 0xCCD7, 0x767A, 0x0320, 0xB98D, 0xE994, 0x5339, 0x2663, 0x9CCE,
	0x135D, 0xA9F0, 0xDCAA, 0x6607, 0x361E, 0x8CB3, 0xF9E9, 0x4344, 0x59DB, 0xE376, 0x962C, 0x2C81, 0x7C98, 0xC635, 0xB36F, 0x09C2,
};

/*!
 * \brief Ctor.
 */
Crc16Layer::Crc16Layer(ProtocolLayer* up, ProtocolLayer* down)
	: base(up, down)
	, m_crc(init)
{
}

void Crc16Layer::reset() {
	m_crc = init;
	base::reset();
}

void Crc16Layer::decode(void* buffer, size_t len) {
	if(len < 2)
		return;

	uint8_t* buffer_ = static_cast<uint8_t*>(buffer);
	uint16_t crc = init;
	for(size_t i = 0; i < len - 2; i++)
		crc = compute(buffer_[i], crc);

	if(crc != ((uint16_t)((uint16_t)(buffer_[len - 2] << 8u) | buffer_[len - 1])))
		// Invalid.
		return;

	base::decode(buffer, len - 2);
}

void Crc16Layer::encode(void const* buffer, size_t len, bool last) {
	uint8_t const* buffer_ = static_cast<uint8_t const*>(buffer);
	for(size_t i = 0; i < len; i++)
		m_crc = compute(buffer_[i], m_crc);

	base::encode(buffer, len, false);

	if(last) {
		uint8_t crc[2] = { (uint8_t)(m_crc >> 8u), (uint8_t)m_crc };
		base::encode(crc, sizeof(crc), true);
		m_crc = init;
	}
}

uint16_t Crc16Layer::compute(uint8_t input, uint16_t crc) {
	// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
	return (uint16_t)(crc16_table[input ^ (uint8_t)(crc >> 8u)] ^ (uint16_t)(crc << 8u));
}

size_t Crc16Layer::mtu() const {
	size_t mtu = base::mtu();
	if(mtu == 0 || mtu > 256u)
		return 256u;
	if(mtu <= 3u)
		return 1u;
	return mtu - 2u;
}



//////////////////////////////
// BufferLayer
//

/*!
 * \brief Constructor for a buffer with given size.
 *
 * If \p size is 0, the buffer it not bounded.
 */
BufferLayer::BufferLayer(size_t size, ProtocolLayer* up, ProtocolLayer* down)
	: base(up, down)
	, m_size(size ? size : std::numeric_limits<size_t>::max())
{}

void BufferLayer::reset() {
	m_buffer.clear();
	base::reset();
}

/*!
 * \brief Collects all partial buffers, and passes the full encoded data on when \p last is set.
 */
void BufferLayer::encode(void const* buffer, size_t len, bool last) {
	char const* buffer_ = static_cast<char const*>(buffer);

	size_t remaining = m_size - m_buffer.size();

	while(remaining < len) {
		// Does not fit in our buffer. Forward immediately.
		if(m_buffer.empty()) {
			base::encode(buffer_, remaining, false);
		} else {
			m_buffer.append(buffer_, remaining);
			base::encode(m_buffer.data(), m_buffer.size(), false);
			m_buffer.clear();
		}
		buffer_ += remaining;
		len -= remaining;
		remaining = m_size;
	}

	if(last || len == remaining) {
		if(m_buffer.empty()) {
			// Pass through immediately.
			base::encode(buffer_, len, last);
		} else {
			// Got already something in the buffer. Concat and forward.
			m_buffer.append(buffer_, len);
			base::encode(m_buffer.data(), m_buffer.size(), last);
			m_buffer.clear();
		}
	} else {
		// Save for later.
		m_buffer.append(buffer_, len);
	}
}


//////////////////////////////
// PrintLayer
//

/*!
 * \brief Constructor to print all decoding/encoding messages to the given \c FILE.
 *
 * If \p f is \c nullptr, printing is suppressed.
 * The name is used as prefix of the printed messages.
 *
 * \see #setFile()
 */
PrintLayer::PrintLayer(FILE* f, char const* name, ProtocolLayer* up, ProtocolLayer* down)
	: base(up, down)
	, m_f(f)
	, m_name(name)
{
}

void PrintLayer::decode(void* buffer, size_t len) {
	if(m_f) {
		std::string prefix;
		if(m_name)
			prefix += m_name;
		prefix += " < ";

		std::string s = string_literal(buffer, len, prefix.c_str());
		s += "\n";
		fputs(s.c_str(), m_f);
	}

	base::decode(buffer, len);
}

void PrintLayer::encode(void const* buffer, size_t len, bool last) {
	if(m_f) {
		std::string prefix;
		if(m_name)
			prefix += m_name;

		if(last)
			prefix += " > ";
		else
			prefix += " * ";

		std::string s = string_literal(buffer, len, prefix.c_str());
		s += "\n";
		fputs(s.c_str(), m_f);
	}

	base::encode(buffer, len, last);
}

/*!
 * \brief Set the \c FILE to write to.
 * \param f the \c FILE, set to \c nullptr to disable output
 */
void PrintLayer::setFile(FILE* f) {
	m_f = f;
}



//////////////////////////////
// Loopback
//

/*!
 * \brief Constructor.
 */
impl::Loopback1::Loopback1(ProtocolLayer& from, ProtocolLayer& to)
	: m_to(to)
	, m_buffer()
	, m_capacity()
	, m_len()
{
	wrap(from);
}

/*!
 * \brief Destructor.
 */
impl::Loopback1::~Loopback1() {
	// NOLINTNEXTLINE(cppcoreguidelines-owning-memory, cppcoreguidelines-no-malloc)
	free(m_buffer);
}

void impl::Loopback1::reset() {
	m_len = 0;
	base::reset();
}

/*!
 * \brief Collect partial data, and passes into the \c decode() of \c to when it has the full message.
 */
void impl::Loopback1::encode(void const* buffer, size_t len, bool last) {
	if(likely(len > 0)) {
		if(unlikely(m_len + len > m_capacity)) {
			size_t capacity = m_len + len + ExtraAlloc;
			// NOLINTNEXTLINE(cppcoreguidelines-owning-memory, cppcoreguidelines-no-malloc)
			void* p = realloc(m_buffer, capacity);
			if(!p)
				throw std::bad_alloc();
			m_buffer = static_cast<char*>(p);
			m_capacity = capacity;
		}

		memcpy(m_buffer + m_len, buffer, len);
		m_len += len;
	}

	if(last) {
		m_to.decode(m_buffer, m_len);
		m_len = 0;
	}
}

/*!
 * \brief Constructs a bidirectional loopback of stacks \p a and \p b.
 */
Loopback::Loopback(ProtocolLayer& a, ProtocolLayer& b)
	: m_a2b(a, b)
	, m_b2a(b, a)
{
}



} // namespace

