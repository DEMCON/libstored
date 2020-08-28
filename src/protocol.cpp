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

/*!
 * \brief Implementation of both encode functions.
 * \private
 */
template <typename void_type, typename char_type>
static void AsciiEscapeLayer_encode(AsciiEscapeLayer& that, void_type* buffer, size_t len, bool last) {
	char_type* p = static_cast<char_type*>(buffer);
	char_type* chunk = p;
	for(size_t i = 0; i < len; i++) {
		char escaped = needEscape((char)p[i]);
		if(unlikely(escaped)) {
			// This is a to-be-escaped byte.
			if(chunk < p + i)
				that.AsciiEscapeLayer::base::encode(chunk, (size_t)(p + i - chunk), false);

			char_type esc[2] = { AsciiEscapeLayer::Esc, (char_type)escaped };
			that.AsciiEscapeLayer::base::encode(esc, sizeof(esc), last && i + 1 == len);
			chunk = p + i + 1;
		}
	}

	if(likely(chunk < p + len) || (len == 0 && last))
		that.AsciiEscapeLayer::base::encode(chunk, (size_t)(p + len - chunk), last);
}

void AsciiEscapeLayer::encode(void* buffer, size_t len, bool last) {
	AsciiEscapeLayer_encode<void,uint8_t>(*this, buffer, len, last);
}

void AsciiEscapeLayer::encode(void const* buffer, size_t len, bool last) {
	AsciiEscapeLayer_encode<void const,uint8_t const>(*this, buffer, len, last);
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

void TerminalLayer::encode(void* buffer, size_t len, bool last) {
	encodeStart();
	writeToFd(m_encodeFd, buffer, len);
	base::encode(buffer, len, false);
	if(last)
		encodeEnd();
}

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
	stored_assert(mtu > 0);
}

/*!
 * \brief Returns the MTU used to split messages into.
 */
size_t SegmentationLayer::mtu() const {
	return m_mtu;
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

void SegmentationLayer::encode(void* buffer, size_t len, bool last) {
	// For now, assume other layers don't do in-place processing.
	encode((void const*)buffer, len, last);
}

void SegmentationLayer::encode(void const* buffer, size_t len, bool last) {
	char const* buffer_ = static_cast<char const*>(buffer);

	while(len) {
		size_t remaining = mtu() - m_encoded;
		size_t chunk = std::min(len, remaining);
		base::encode(buffer_, chunk, chunk == remaining);
		len -= chunk;
		buffer_ += chunk;

		if(chunk == remaining) {
			// Full MTU.
			m_encoded = 0;
		} else {
			// Partial MTU. Record that we already filled some of the packet.
			m_encoded = chunk;
		}
	}

	if(last) {
		// The marker always ends the packet.
		char end = EndMarker;
		base::encode(&end, 1, true);
		m_encoded = 0;
	}
}

} // namespace

