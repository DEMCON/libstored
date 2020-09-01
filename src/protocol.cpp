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

	for(; i + 1 < len; i++, decodeOffset++) {
		if(unlikely(p[i] == Esc))
			goto escape;
		else
			p[decodeOffset] = p[i];
	}

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
	return mtu ? mtu / 2 : 0;
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
{}

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
	stored_assert(mtu == 0 || mtu > 4);
	return mtu <= 4 ? 0 : mtu - 4;
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
		size_t start = m_decode.size();
		m_decode.resize(start + len - 1);
		memcpy(&m_decode[start], buffer_, len - 1);

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
	stored_assert(mtu > 1);

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

/*!
 * \brief Ctor.
 */
ArqLayer::ArqLayer(size_t maxEncodeBuffer, ProtocolLayer* up, ProtocolLayer* down)
	: base(up, down)
	, m_decodeState(DecodeStateIdle)
	, m_decodeSeq(nextSeq(ResetSeq))
	, m_decodeSeqStart()
	, m_encodeState(EncodeStateIdle)
	, m_encodeSeq(ResetSeq)
	, m_maxEncodeBuffer(maxEncodeBuffer)
	, m_encodeBufferSize()
{
}

void ArqLayer::decode(void* buffer, size_t len) {
	if(len == 0)
		return;

	char* buffer_ = static_cast<char*>(buffer);
	uint8_t seq = (uint8_t)buffer_[0];
	if(unlikely(seq == ResetSeq)) {
		// Reset communication state.
		m_decodeState = DecodeStateIdle;
		m_encodeState = EncodeStateIdle;
		m_encodeBuffer.clear();
		m_encodeBufferSize = 0;
		m_decodeSeq = ResetSeq;
		m_encodeSeq = ResetSeq;
		setPurgeableResponse(false);
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
		} else if(seq == m_decodeSeqStart) {
			// This seems to be a retransmit of the current command.
			switch(m_encodeState) {
			case EncodeStateUnbufferedIdle:
				// We must reexecute the command. Reset the sequence number, as the content may change.
				setPurgeableResponse(false);
				m_decodeSeq = m_decodeSeqStart;
				m_decodeState = DecodeStateIdle;
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
			for(std::vector<std::string>::const_iterator it = m_encodeBuffer.cbegin(); it != m_encodeBuffer.cend(); ++it)
				base::encode(it->data(), it->size(), true);
			m_decodeState = DecodeStateDecoded;
		}
		break;
	case DecodeStateIdle:
		if(unlikely(len == 1)) {
			// Ignore empty packets.
			m_decodeSeq = nextSeq(m_decodeSeq);
			break;
		}
		m_decodeSeqStart = m_decodeSeq;
		m_decodeState = DecodeStateDecoding;
		// fall-through
	case DecodeStateDecoding:
		if(likely(seq == m_decodeSeq)) {
			// Properly in sequence.
			base::decode(buffer_ + 1, len - 1);
			m_decodeSeq = nextSeq(m_decodeSeq);

			// Should not wrap-around during a request.
			stored_assert(m_decodeSeq != m_decodeSeqStart);
		}
		break;
	default:;
	}
}

void ArqLayer::encode(void const* buffer, size_t len, bool last) {
	if(m_decodeState == DecodeStateDecoding) {
		// This seems to be the first part of the response.
		// So, the request message must have been complete.
		m_decodeState = DecodeStateDecoded;
		stored_assert(m_encodeState == EncodeStateIdle || m_encodeState == EncodeStateUnbufferedIdle);
		m_encodeSeq = ResetSeq;
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

	switch(m_encodeState) {
	case EncodeStateUnbufferedIdle:
		base::encode((void const*)&m_encodeSeq, 1, false);
		m_encodeSeq = nextSeq(m_encodeSeq);
		m_encodeState = EncodeStateUnbufferedEncoding;
		// fall-through
	case EncodeStateUnbufferedEncoding:
		base::encode(buffer, len, last);
		if(last)
			m_encodeState = EncodeStateUnbufferedIdle;
		break;

	case EncodeStateIdle:
		{
			char encodeSeq = (char)m_encodeSeq;
#if STORED_cplusplus >= 201103L
			m_encodeBuffer.emplace_back(&encodeSeq, 1u);
#else
			m_encodeBuffer.push_back(std::string(&encodeSeq, 1u));
#endif
		}
		m_encodeSeq = nextSeq(m_encodeSeq);
		m_encodeState = EncodeStateEncoding;
		m_encodeBufferSize++;
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

void ArqLayer::setPurgeableResponse(bool purgeable) {
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

size_t ArqLayer::mtu() const {
	size_t mtu = base::mtu();
	stored_assert(mtu == 0 || mtu > 1);
	return mtu <= 1 ? 0 : mtu - 1;
}

uint8_t ArqLayer::nextSeq(uint8_t seq) {
	seq = (uint8_t)(seq + 1u);
	return seq == ResetSeq ? (uint8_t)(ResetSeq + 1u) : seq;
}





//////////////////////////////
// CrcLayer
//

// Generated by http://www.sunshine2k.de/coding/javascript/crc/crc_js.html
static const uint8_t crc_table[] = {
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
CrcLayer::CrcLayer(ProtocolLayer* up, ProtocolLayer* down)
	: base(up, down)
	, m_crc(init)
{
}

void CrcLayer::decode(void* buffer, size_t len) {
	if(len < 1)
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

void CrcLayer::encode(void const* buffer, size_t len, bool last) {
	uint8_t const* buffer_ = static_cast<uint8_t const*>(buffer);
	for(size_t i = 0; i < len; i++)
		m_crc = compute(m_crc, buffer_[i]);

	base::encode(buffer, len, false);

	if(last) {
		base::encode(&m_crc, 1, true);
		m_crc = init;
	}
}

uint8_t CrcLayer::compute(uint8_t input, uint8_t crc) {
	// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
	return crc_table[(uint8_t)(input ^ crc)];
}

size_t CrcLayer::mtu() const {
	size_t mtu = base::mtu();
	stored_assert(mtu == 0 || mtu > 1);
	return mtu <= 1 ? 0 : mtu - 1;
}

} // namespace

