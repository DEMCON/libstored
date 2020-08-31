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
AsciiEscapeLayer::AsciiEscapeLayer(ProtocolLayer* up, ProtocolLayer* down)
	: base(up, down)
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

static char needEscape(char c) {
	if(!((uint8_t)c & (uint8_t)~(uint8_t)AsciiEscapeLayer::EscMask)) {
		switch(c) {
		case '\t':
		case '\n':
			// Don't escape.
			return 0;

		case '\r':
			// Do escape \r, as Windows may inject it automatically.  So, if \r
			// is meant to be sent, escape it, such that the client may remove
			// all (unescaped) \r's automatically.
		default:
			return (char)((uint8_t)c | 0x40u);
		}
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

			uint8_t const esc[2] = { AsciiEscapeLayer::Esc, (uint8_t const)escaped };
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
 */
void TerminalLayer::writeToFd(int fd, void const* buffer, size_t len) {
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
		m_decode.resize(start + len);
		memcpy(&m_decode[start], buffer_, len);

		size_t size = m_decode.size();
		if(m_decode[size - 1] == EndMarker) {
			// Got it.
			base::decode(&m_decode[0], size - 1);
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
	while(len) {
		size_t remaining = mtu - m_encoded;
		size_t chunk = std::min(len, remaining);
		base::encode(buffer_, chunk, chunk == remaining);
		len -= chunk;
		buffer_ += chunk;

		if(chunk == remaining) {
			// Full MTU.
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
	, m_decodeId(1)
	, m_decodeIdStart()
	, m_encodeState(EncodeStateIdle)
	, m_encodeId()
	, m_encodeIdStart()
	, m_maxEncodeBuffer(maxEncodeBuffer)
	, m_encodeBufferSize()
{
}

void ArqLayer::decode(void* buffer, size_t len) {
	if(len == 0)
		return;

	char* buffer_ = static_cast<char*>(buffer);
	uint8_t id = (uint8_t)buffer_[0];
	if(unlikely(id == 0)) {
		// Reset communication state.
		m_decodeState = DecodeStateIdle;
		m_encodeState = EncodeStateIdle;
		m_encodeBuffer.clear();
		m_encodeBufferSize = 0;
		m_decodeId = 0;
		m_encodeId = 0;
		setPurgeableResponse(false);
	}

	switch(m_decodeState) {
	case DecodeStateDecoded:
		stored_assert(m_encodeState == EncodeStateIdle || m_encodeState == EncodeStateUnbufferedIdle);

		if(id == m_decodeId) {
			// This is the next command. The previous one was received, apparently.
			m_decodeState = DecodeStateIdle;
			m_encodeState = EncodeStateIdle;
			m_encodeBuffer.clear();
			m_encodeBufferSize = 0;
			setPurgeableResponse(false);
		} else if(id == m_decodeIdStart) {
			// This seems to be a retransmit of the current command.
			switch(m_encodeState) {
			case EncodeStateUnbufferedIdle:
				// We must reexecute the command, so revert to previous decode start state
				m_decodeState = DecodeStateIdle;
				m_decodeId = m_decodeIdStart;
				break;
			case EncodeStateIdle:
				// Wait for full retransmit of the command, but do not actually decode.
				m_decodeState = DecodeStateRetransmit;
				break;
			default:
				stored_assert(false); // NOLINT(hicpp-static-assert,misc-static-assert)
			}
		} // else: unexpected id; ignore.
		break;
	default:;
	}

	switch(m_decodeState) {
	case DecodeStateRetransmit:
		if(id == m_decodeId) {
			// Got the last part of the command. Retransmit the response buffer.
			for(std::vector<std::string>::const_iterator it = m_encodeBuffer.cbegin(); it != m_encodeBuffer.cend(); ++it)
				encode(it->data(), it->size(), true);
			m_decodeState = DecodeStateDecoded;
		}
		break;
	case DecodeStateIdle:
		if(unlikely(len == 1)) {
			// Ignore empty packets.
			m_decodeId++;
			break;
		}
		m_decodeIdStart = m_decodeId;
		m_encodeIdStart = m_encodeId;
		m_decodeState = DecodeStateDecoding;
		// fall-through
	case DecodeStateDecoding:
		if(likely(id == m_decodeId)) {
			// Properly in sequence.
			base::decode(buffer_ + 1, len - 1);
			m_decodeId++;
			if(m_decodeId == 0)
				m_decodeId = 1;

			// Should not wrap-around during a request.
			stored_assert(m_decodeId != m_decodeIdStart);
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
		stored_assert(m_encodeState == EncodeStateIdle);
		m_encodeState = EncodeStateIdle;
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
		base::encode((void const*)&m_encodeId, 1, false);
		m_encodeId++;
		m_encodeState = EncodeStateUnbufferedEncoding;
		// fall-through
	case EncodeStateUnbufferedEncoding:
		base::encode(buffer, len, last);
		if(last)
			m_encodeState = EncodeStateUnbufferedIdle;
		break;

	case EncodeStateIdle:
		{
			char encodeId = (char)m_encodeId;
#if STORED_cplusplus >= 201103L
			m_encodeBuffer.emplace_back(&encodeId, 1);
#else
			m_encodeBuffer.push_back(std::string(&encodeId, 1));
#endif
		}
		m_encodeId++;
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
		case EncodeStateEncoding:
			base::encode(m_encodeBuffer.back().data(), false);
			m_encodeState = EncodeStateUnbufferedEncoding;
			break;
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

} // namespace

