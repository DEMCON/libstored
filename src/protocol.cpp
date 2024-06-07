// SPDX-FileCopyrightText: 2020-2024 Jochem Rutgers
//
// SPDX-License-Identifier: MPL-2.0

#include <libstored/poller.h>
#include <libstored/protocol.h>
#include <libstored/util.h>

#ifdef STORED_OS_WINDOWS
#  include <fcntl.h>
#  include <io.h>
#elif !defined(STORED_OS_BAREMETAL)
#  include <fcntl.h>
#  include <unistd.h>
#endif

#if defined(STORED_OS_POSIX)
#  include <csignal>
#  include <sys/stat.h>
#  include <sys/types.h>
#  include <termios.h>
#endif

#ifdef STORED_HAVE_ZTH
#  include <zth>
#  define delay_ms(ms) zth::mnap(ms)
#elif defined(STORED_OS_WINDOWS)
#  define delay_ms(ms) Sleep(ms)
#else
#  define delay_ms(ms) usleep((ms) * 1000L)
#endif

#if defined(STORED_OS_WINDOWS)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
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
ProtocolLayer::~ProtocolLayer()
{
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
 * \param all when \c true, convert all control characters, instead of only
 *	those that conflict with other protocols
 */
AsciiEscapeLayer::AsciiEscapeLayer(bool all, ProtocolLayer* up, ProtocolLayer* down)
	: base(up, down)
	, m_all(all)
{}

void AsciiEscapeLayer::decode(void* buffer, size_t len)
{
	char* p = static_cast<char*>(buffer);

	// Common case: there is no escape character.
	size_t i = 0;
	for(; i + 1 < len; i++)
		if(unlikely(p[i] == Esc || p[i] == '\r'))
			goto first_escape;

	// No escape characters.
	base::decode(buffer, len);
	return;

first_escape:
	// Process escape sequences in-place.
	size_t decodeOffset = i;

	if(p[i] == '\r') {
		// Just drop.
	} else {
escape:
		if(p[++i] == Esc)
			p[decodeOffset] = (char)Esc;
		else
			p[decodeOffset] = (char)((uint8_t)p[i] & (uint8_t)EscMask);
		decodeOffset++;
	}
	i++;

	// The + 1 prevents it from trying to decode an escape character at the end.
	for(; i + 1 < len; i++, decodeOffset++) {
		if(unlikely(p[i] == Esc))
			goto escape;
		else if(unlikely(p[i] == '\r'))
			decodeOffset--;
		else
			p[decodeOffset] = p[i];
	}

	// Always add the last character (if any).
	if(i < len)
		p[decodeOffset++] = p[i];
	base::decode(p, decodeOffset);
}

char AsciiEscapeLayer::needEscape(char c) const
{
	if(!((uint8_t)c & (uint8_t) ~(uint8_t)AsciiEscapeLayer::EscMask)) {
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
		return (char)((uint8_t)c | 0x40U);
	} else if(c == AsciiEscapeLayer::Esc) {
		return c;
	} else {
		return 0;
	}
}

void AsciiEscapeLayer::encode(void const* buffer, size_t len, bool last)
{
	uint8_t const* p = static_cast<uint8_t const*>(buffer);
	uint8_t const* chunk = p;

	for(size_t i = 0; i < len; i++) {
		char escaped = needEscape((char)p[i]);
		if(unlikely(escaped)) {
			// This is a to-be-escaped byte.
			if(chunk < p + i)
				base::encode(chunk, (size_t)(p + i - chunk), false);

			uint8_t const esc[2] = {AsciiEscapeLayer::Esc, (uint8_t)escaped};
			base::encode(esc, sizeof(esc), last && i + 1 == len);
			chunk = p + i + 1;
		}
	}

	if(likely(chunk < p + len) || (len == 0 && last))
		base::encode(chunk, (size_t)(p + len - chunk), last);
}

size_t AsciiEscapeLayer::mtu() const
{
	size_t mtu = base::mtu();
	if(mtu == 0U)
		return 0U;
	if(mtu == 1U)
		return 1U;
	return mtu / 2U;
}



//////////////////////////////
// TerminalLayer
//

#if STORED_cplusplus < 201103L
/*!
 * \copydoc stored::ProtocolLayer::ProtocolLayer(ProtocolLayer*,ProtocolLayer*)
 * \param cb the callback to call with the data that is not part of debug messages during decode().
 *	Set to \c nullptr to drop this data.
 */
TerminalLayer::TerminalLayer(NonDebugDecodeCallback* cb, ProtocolLayer* up, ProtocolLayer* down)
	: base(up, down)
	, m_nonDebugDecodeCallback(cb)
	, m_decodeState(StateNormal)
	, m_encodeState()
{}
#endif

TerminalLayer::TerminalLayer(ProtocolLayer* up, ProtocolLayer* down)
	: base(up, down)
#if STORED_cplusplus < 201103L
	, m_nonDebugDecodeCallback()
#endif
	, m_decodeState(StateNormal)
	, m_encodeState()
{}

void TerminalLayer::reset()
{
	m_decodeState = StateNormal;
	m_encodeState = false;
	m_buffer.clear();
	base::reset();
}

/*!
 * \copydoc stored::ProtocolLayer::~ProtocolLayer()
 */
TerminalLayer::~TerminalLayer() is_default

void TerminalLayer::decode(void* buffer, size_t len)
{
	size_t nonDebugOffset = m_decodeState < StateDebug ? 0 : len;

	for(size_t i = 0; i < len; i++) {
		char c = (static_cast<char*>(buffer))[i];

		switch(m_decodeState) {
		default:
		case StateNormal:
			if(unlikely(c == Esc))
				m_decodeState = StateNormalEsc;
			break;
		case StateNormalEsc:
			if(likely(c == EscStart)) {
				if(i - nonDebugOffset > 1U)
					nonDebugDecode(
						static_cast<char*>(buffer) + nonDebugOffset,
						i - nonDebugOffset - 1); // Also skip the ESC
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
				base::decode(m_buffer.data(), m_buffer.size());
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

void TerminalLayer::nonDebugEncode(void const* buffer, size_t len)
{
	stored_assert(!m_encodeState);
	stored_assert(buffer || len == 0);
	if(len)
		base::encode(buffer, len, true);
}

/*!
 * \brief Receptor of non-debug data during decode().
 *
 * Default implementation writes to the \c nonDebugDecodeFd, as supplied to the constructor.
 */
void TerminalLayer::nonDebugDecode(void* buffer, size_t len)
{
	if(m_nonDebugDecodeCallback)
		m_nonDebugDecodeCallback(buffer, len);
}

void TerminalLayer::encode(void const* buffer, size_t len, bool last)
{
	encodeStart();
	base::encode(buffer, len, false);
	if(last)
		encodeEnd();
}

/*!
 * \brief Emits a start-of-frame sequence if it hasn't done yet.
 *
 * Call #encodeEnd() to finish the current frame.
 */
void TerminalLayer::encodeStart()
{
	if(m_encodeState)
		return;

	m_encodeState = true;
	char start[2] = {Esc, EscStart};
	base::encode((void*)start, sizeof(start), false);
}

/*!
 * \brief Emits an end-of-frame sequence of the frame started using #encodeStart().
 */
void TerminalLayer::encodeEnd()
{
	if(!m_encodeState)
		return;

	m_encodeState = false;
	char end[2] = {Esc, EscEnd};
	base::encode((void*)end, sizeof(end), true);
}

size_t TerminalLayer::mtu() const
{
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
{}

void SegmentationLayer::reset()
{
	m_decode.clear();
	m_encoded = 0;
	base::reset();
}

size_t SegmentationLayer::mtu() const
{
	// We segment, so all layers above can use any size they want.
	return 0;
}

/*!
 * \brief Returns the MTU used to split messages into.
 */
size_t SegmentationLayer::lowerMtu() const
{
	size_t lower_mtu = base::mtu();
	if(!m_mtu)
		return lower_mtu;
	else if(!lower_mtu)
		return m_mtu;
	else
		return std::min<size_t>(m_mtu, lower_mtu);
}

void SegmentationLayer::decode(void* buffer, size_t len)
{
	if(len == 0)
		return;

	char const* buffer_ = static_cast<char*>(buffer);
	if(!m_decode.empty() || buffer_[len - 1] != EndMarker) {
		// Save for later packet reassembling.
		if(len > 1) {
			size_t start = m_decode.size();
			m_decode.resize(start + len - 1);
			memcpy(&m_decode[start], buffer_, len - 1);
		}

		if(buffer_[len - 1] == EndMarker) {
			// Got it.
			base::decode(m_decode.data(), m_decode.size());
			m_decode.clear();
		}
	} else {
		// Full packet is in buffer. Forward immediately.
		base::decode(buffer, len - 1);
	}
}

void SegmentationLayer::encode(void const* buffer, size_t len, bool last)
{
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

/*!
 * \brief Ctor.
 *
 * If \p maxEncodeBuffer is non-zero, it defines the upper limit of the
 * combined length of all queued messages for encoding. If the limit is hit,
 * the #EventEncodeBufferOverflow event is passed to the callback.
 */
ArqLayer::ArqLayer(size_t maxEncodeBuffer, ProtocolLayer* up, ProtocolLayer* down)
	: base(up, down)
#if STORED_cplusplus < 201103L
	, m_cb()
	, m_cbArg()
#endif
	, m_maxEncodeBuffer(maxEncodeBuffer)
	, m_encodeQueueSize()
	, m_encodeState(EncodeStateIdle)
	, m_pauseTransmit()
	, m_didTransmit()
	, m_retransmits()
	, m_sendSeq()
	, m_recvSeq()
{
	// Empty encode with seq 0, which indicates a reset message.
	keepAlive();
}

/*!
 * \brief Dtor.
 */
ArqLayer::~ArqLayer()
{
	for(Deque<String::type*>::type::iterator it = m_encodeQueue.begin();
	    it != m_encodeQueue.end(); ++it)
		cleanup(*it);
	for(Deque<String::type*>::type::iterator it = m_spare.begin(); it != m_spare.end(); ++it)
		cleanup(*it);
}

void ArqLayer::reset()
{
	while(!m_encodeQueue.empty())
		popEncodeQueue();
	stored_assert(m_encodeQueueSize == 0);

	m_encodeState = EncodeStateIdle;
	m_pauseTransmit = false;
	m_didTransmit = false;
	m_retransmits = 0;
	m_sendSeq = 0;
	m_recvSeq = 0;
	base::reset();
	keepAlive();
}

void ArqLayer::decode(void* buffer, size_t len)
{
	uint8_t* buffer_ = static_cast<uint8_t*>(buffer);
	bool reconnect = false;

	// Usually, we expect something we have to ack. Possibly a reset command to ack afterwards.
	// After the response, a transmit() may be called.
	uint8_t resp[2];
	size_t resplen = 0;
	bool do_transmit = false;
	bool do_decode = false;

	while(len > 0) {
		uint8_t hdr = buffer_[0];

		if(hdr & AckFlag) {
			if(waitingForAck()
			   && (hdr & SeqMask) == ((uint8_t)(*m_encodeQueue.front())[0] & SeqMask)) {
				// They got our last transmission.
				popEncodeQueue();
				m_retransmits = 0;

				// Transmit next message, if any.
				do_transmit = true;

				if(unlikely((hdr & SeqMask) == 0)) {
					// This is an ack to our reset message. We are connected
					// now.
					reconnect = true;
					base::connected();
				}
			}

			buffer_++;
			len--;
		} else if(likely((hdr & SeqMask) == m_recvSeq)) {
			// This is a proper next message.
			// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
			resp[resplen++] = (uint8_t)(m_recvSeq | AckFlag);
			m_recvSeq = nextSeq(m_recvSeq);

			do_decode = !(hdr & NopFlag);
			do_transmit = true; // Send out next message.
			buffer_++;
			len--;
		} else if((hdr & SeqMask) == 0) {
			// This is an unexpected reset message. Reset communication.
			event(EventReconnect);

			// Send ack.
			m_recvSeq = nextSeq(0);
			// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
			resp[resplen++] = (uint8_t)AckFlag;

			// Also reset our send seq.
			if(!reconnect
			   && (m_encodeQueueSize == 0
			       || (*m_encodeQueue.front())[0] != (char)NopFlag)) {
				// Inject reset message in queue.
				String::type& s = pushEncodeQueueRaw(false);
				s.push_back((char)NopFlag);
				m_encodeQueueSize++;
				m_sendSeq = nextSeq(0);

				// Reencode existing outbound messages.
				for(Deque<String::type*>::type::iterator it =
					    ++m_encodeQueue.begin();
				    it != m_encodeQueue.end(); ++it) {
					(**it)[0] = (char)((uint8_t)((uint8_t)(**it)[0]
								     & (uint8_t)~SeqMask)
							   | m_sendSeq);
					m_sendSeq = nextSeq(m_sendSeq);
				}
			}

			do_transmit = true;
			buffer_++;
			len--;
		} else if(nextSeq((uint8_t)(hdr & SeqMask)) == m_recvSeq) {
			// This is a retransmit of the previous message.
			// Send ack again.
			// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
			resp[resplen++] = (uint8_t)((uint8_t)(hdr & SeqMask) | AckFlag);

			if((hdr & NopFlag)) {
				buffer_++;
				len--;
			} else {
				// Drop remaining message, without decode.
				len = 0;
			}
		} else {
			// Drop.
			len = 0;
			do_transmit = true;
		}

		if(do_decode) {
			// Rest of message is data to decode.
			break;
		}

		if(resplen == sizeof(resp)) {
			// Buffer full. Unexpected amount of responses. Drop and wait for
			// retransmit.
			break;
		}
	}

	if(do_decode) {
		// Do decode first, as recursive calls to decode/encode may corrupt our buffer.

		stored_assert(!m_pauseTransmit);
		m_pauseTransmit = true;

		resetDidTransmit();
		// Decode and queue encodes only.
		base::decode(buffer_, len);
		if(didTransmit())
			do_transmit = true;

		// We do not expect recursion here that influence this flag.
		stored_assert(m_pauseTransmit);
		m_pauseTransmit = false;
	}

	if(resplen) {
		// First encode the responses...
		base::encode(resp, resplen, !do_transmit);
		m_didTransmit = true;
	}

	if(do_transmit) {
		// ...then a message.
		if(!transmit() && resplen) {
			base::encode(nullptr, 0, true);
			m_didTransmit = true;
		}
	}
}

/*!
 * \brief Checks if this layer is waiting for an ack.
 */
bool ArqLayer::waitingForAck() const
{
	if(m_encodeQueue.empty())
		return false;

	if(m_encodeQueue.size() == 1 && m_encodeState != EncodeStateIdle)
		return false;

	stored_assert(m_encodeQueue.front() && !m_encodeQueue.front()->empty());
	return true;
}

void ArqLayer::encode(void const* buffer, size_t len, bool last)
{
	if(m_maxEncodeBuffer > 0 && m_maxEncodeBuffer < m_encodeQueueSize + len + 1U /* seq */)
		event(EventEncodeBufferOverflow);

	switch(m_encodeState) {
	default:
	case EncodeStateIdle:
		pushEncodeQueue(buffer, len);

		if(!last)
			m_encodeState = EncodeStateEncoding;

		break;

	case EncodeStateEncoding:
		stored_assert(!m_encodeQueue.empty());
		m_encodeQueueSize += len;
		m_encodeQueue.back()->append(static_cast<char const*>(buffer), len);

		if(last)
			m_encodeState = EncodeStateIdle;
		break;
	}

	transmit();
}

bool ArqLayer::flush()
{
	bool res = !transmit();
	return base::flush() && res;
}

/*!
 * \brief (Re)transmits the first message in the queue.
 * \return \c true if something has been sent, \c false if the queue is empty
 */
bool ArqLayer::transmit()
{
	if(m_encodeQueue.empty())
		// Nothing to send.
		return false;

	if(m_encodeQueue.size() == 1 && m_encodeState == EncodeStateEncoding)
		// Still assembling first message, but there is nothing to flush (yet).
		return false;

	// Update administration first, in case base::encode() has some recursive
	// call back to decode()/encode().
	m_didTransmit = true;

	if(m_pauseTransmit)
		// Only queue for now.
		return false;

	if(m_retransmits < std::numeric_limits<decltype(m_retransmits)>::max())
		m_retransmits++;
	if(m_retransmits >= RetransmitCallbackThreshold)
		event(EventRetransmit);

	// (Re)transmit first message.
	stored_assert(waitingForAck());
	base::encode(m_encodeQueue.front()->data(), m_encodeQueue.front()->size(), true);
	return true;
}

/*!
 * \brief Forward the given event to the registered callback, if any.
 */
void ArqLayer::event(ArqLayer::Event e)
{
	if(m_cb) {
#if STORED_cplusplus < 201103L
		m_cb(*this, e, m_cbArg);
#else
		m_cb(*this, e);
#endif
	} else {
		switch(e) {
		default:
		case EventNone:
		case EventReconnect:
		case EventRetransmit:
			// By default, ignore.
			break;
		case EventEncodeBufferOverflow:
			// Cannot handle this.
			abort();
		}
	}
}

/*!
 * \brief Compute the next sequence number.
 */
uint8_t ArqLayer::nextSeq(uint8_t seq)
{
	seq = (uint8_t)((seq + 1U) & SeqMask);
	// cppcheck-suppress[knownConditionTrueFalse,unmatchedSuppression]
	return seq ? seq : 1U;
}

size_t ArqLayer::mtu() const
{
	size_t mtu = base::mtu();
	if(mtu == 0)
		return 0;
	if(mtu <= 2U)
		return 1U;
	return mtu - 2U;
}

/*!
 * \brief Checks if a full message has been transmitted.
 *
 * This function can be used to determine if this layer transmitted anything.
 * For example, when a response is decoded, this function can be used to check
 * if anything has sent back, or the message was dropped. Or, when #flush() is
 * called, this flag can be used to check if anything was actually flushed.
 *
 * To use the function, first call #resetDidTransmit(), then execute the code
 * you want to check, and then check #didTransmit().
 *
 * \see #resetDidTransmit()
 */
bool ArqLayer::didTransmit() const
{
	return m_didTransmit;
}

/*!
 * \brief Reset the flag for #didTransmit().
 * \see #didTransmit()
 */
void ArqLayer::resetDidTransmit()
{
	m_didTransmit = false;
}

/*!
 * \brief Returns the number of consecutive retransmits of the same message.
 *
 * Use this function to determine whether the connection is still alive.  It is
 * application-defined what the threshold is of too many retransmits.
 */
size_t ArqLayer::retransmits() const
{
	return m_retransmits ? m_retransmits - 1U : 0U;
}

/*!
 * \brief Send a keep-alive packet to check the connection.
 *
 * It actually retransmits the message that is currently processed (waiting for
 * an ack), or sends a dummy message in case the encode queue is empty. Either
 * way, #retransmits() and the #EventRetransmit can be used afterwards to
 * determine the quality of the link.
 */
void ArqLayer::keepAlive()
{
	if(m_encodeQueue.empty()) {
		// Send empty message. This will trigger (re)transmits, so a broken
		// connection will be detected.
		pushEncodeQueueRaw().push_back((char)(m_sendSeq | NopFlag));
		m_encodeQueueSize++;
		m_sendSeq = nextSeq(m_sendSeq);
	}

	transmit();
}

/*!
 * \brief Drop front of encode queue.
 */
void ArqLayer::popEncodeQueue()
{
	stored_assert(!m_encodeQueue.empty());
	m_encodeQueueSize -= m_encodeQueue.front()->size();
	m_encodeQueue.front()->clear();

#if STORED_cplusplus >= 201103L
	m_spare.emplace_back(m_encodeQueue.front());
#else
	m_spare.push_back(m_encodeQueue.front());
#endif

	m_encodeQueue.pop_front();
}

/*!
 * \brief Push the given buffer into the encode queue.
 */
void ArqLayer::pushEncodeQueue(void const* buffer, size_t len, bool back)
{
	String::type& s = pushEncodeQueueRaw(back);
	s.push_back((char)m_sendSeq);
	s.append(static_cast<char const*>(buffer), len);
	m_sendSeq = nextSeq(m_sendSeq);
	m_encodeQueueSize += len + 1U;
}

/*!
 * \brief Adds an entry in the encode queue, but do not populate the contents.
 *
 * The returned buffer can be used to put the message in. This should include
 * the sequence number as the first byte.
 */
String::type& ArqLayer::pushEncodeQueueRaw(bool back)
{
	String::type* s = nullptr;

	if(m_spare.empty()) {
		// NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
		s = new(allocate<String::type>()) String::type;
	} else {
		s = m_spare.back();
		m_spare.pop_back();
	}

	stored_assert(s);

	if(back) {
		m_encodeQueue.
#if STORED_cplusplus >= 201103L
			emplace_back(s);
#else
			push_back(s);
#endif
	} else {
		m_encodeQueue.
#if STORED_cplusplus >= 201103L
			emplace_front(s);
#else
			push_front(s);
#endif
	}

	stored_assert(s->empty());
	return *s;
}

/*!
 * \brief Free all unused memory.
 */
void ArqLayer::shrink_to_fit()
{
	for(Deque<String::type*>::type::iterator it = m_encodeQueue.begin();
	    it != m_encodeQueue.end(); ++it)
#if STORED_cplusplus >= 201103L
		(*it)->shrink_to_fit();
#else
		(*it)->reserve((*it)->size());
#endif

	for(Deque<String::type*>::type::iterator it = m_spare.begin(); it != m_spare.end(); ++it)
		cleanup(*it);

	m_spare.clear();
#if STORED_cplusplus >= 201103L
	m_spare.shrink_to_fit();
#endif
}

void ArqLayer::connected()
{
	// Don't propagate the connected event.  A reconnection is handled by this layer itself, via
	// retransmits or resets.
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
{}

void DebugArqLayer::reset()
{
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

void DebugArqLayer::decode(void* buffer, size_t len)
{
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
		stored_assert(
			m_encodeState == EncodeStateIdle
			|| m_encodeState == EncodeStateUnbufferedIdle);

		if(seq == m_decodeSeq) {
			// This is the next command. The previous one was received, apparently.
			m_decodeState = DecodeStateIdle;
			m_encodeState = EncodeStateIdle;
			m_encodeBuffer.clear();
			m_encodeBufferSize = 0;
			setPurgeableResponse(false);
			break;
		}
		STORED_FALLTHROUGH
	case DecodeStateRetransmit:
		if(seq == m_decodeSeqStart) {
			// This seems to be a retransmit of the current command.
			switch(m_encodeState) {
			case EncodeStateUnbufferedIdle:
				// We must reexecute the command. Reset the sequence number, as the
				// content may change.
				setPurgeableResponse(false);
				m_decodeSeq = m_decodeSeqStart;
				m_decodeState = DecodeStateIdle;
				m_encodeSeqReset = true;
				break;
			case EncodeStateIdle:
				// Wait for full retransmit of the command, but do not actually
				// decode.
				m_decodeState = DecodeStateRetransmit;
				break;
			case EncodeStateEncoding:
			case EncodeStateUnbufferedEncoding:
			default:
				// NOLINTNEXTLINE(hicpp-static-assert,misc-static-assert,cert-dcl03-c)
				stored_assert(false);
			}
		} // else: unexpected seq; ignore.
		break;
	case DecodeStateIdle:
	case DecodeStateDecoding:
	default:;
	}

	switch(m_decodeState) {
	case DecodeStateRetransmit:
		if(nextSeq(seq) == m_decodeSeq) {
			// Got the last part of the command. Retransmit the response buffer.
			for(Vector<String::type>::type::const_iterator it = m_encodeBuffer.begin();
			    it != m_encodeBuffer.end(); ++it)
				base::encode(it->data(), it->size(), true);
			m_decodeState = DecodeStateDecoded;
		}
		break;
	case DecodeStateIdle:
		m_decodeSeqStart = m_decodeSeq;
		m_decodeState = DecodeStateDecoding;
		STORED_FALLTHROUGH
	case DecodeStateDecoding:
		if(likely(seq == m_decodeSeq)) {
			// Properly in sequence.
			base::decode(buffer_, len);
			m_decodeSeq = nextSeq(m_decodeSeq);

			// Should not wrap-around during a request.
			stored_assert(m_decodeSeq != m_decodeSeqStart);
		}
		break;
	case DecodeStateDecoded:
	default:;
	}
}

void DebugArqLayer::encode(void const* buffer, size_t len, bool last)
{
	if(m_decodeState == DecodeStateDecoding) {
		// This seems to be the first part of the response.
		// So, the request message must have been complete.
		m_decodeState = DecodeStateDecoded;
		stored_assert(
			m_encodeState == EncodeStateIdle
			|| m_encodeState == EncodeStateUnbufferedIdle);
	}

	switch(m_encodeState) {
	case EncodeStateIdle:
	case EncodeStateEncoding:
		if(unlikely(
			   m_maxEncodeBuffer > 0 && m_encodeBufferSize + len > m_maxEncodeBuffer)) {
			// Overflow.
			setPurgeableResponse();
		}
		break;
	case EncodeStateUnbufferedIdle:
	case EncodeStateUnbufferedEncoding:
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
	case EncodeStateEncoding:
	case EncodeStateUnbufferedEncoding:
	default:;
	}

	switch(m_encodeState) {
	default:
	case EncodeStateUnbufferedIdle:
		base::encode(seq, seqlen, false);
		m_encodeState = EncodeStateUnbufferedEncoding;
		STORED_FALLTHROUGH
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
		m_encodeBuffer.push_back(String::type(reinterpret_cast<char*>(seq), seqlen));
#endif
		m_encodeState = EncodeStateEncoding;
		m_encodeBufferSize += seqlen;
		STORED_FALLTHROUGH
	case EncodeStateEncoding:
		stored_assert(!m_encodeBuffer.empty());
		m_encodeBufferSize += len;
		m_encodeBuffer.back().append(static_cast<char const*>(buffer), len);

		if(last) {
			base::encode(
				m_encodeBuffer.back().data(), m_encodeBuffer.back().size(), true);
			m_encodeState = EncodeStateIdle;
		}
	}
}

void DebugArqLayer::setPurgeableResponse(bool purgeable)
{
	bool wasPurgeable = m_encodeState == EncodeStateUnbufferedIdle
			    || m_encodeState == EncodeStateUnbufferedEncoding;

	if(wasPurgeable == purgeable)
		return;

	if(purgeable) {
		// Switch to purgeable.
		base::setPurgeableResponse(true);

		switch(m_encodeState) {
		case EncodeStateEncoding: {
			String::type const& s = m_encodeBuffer.back();
			base::encode(s.data(), s.size(), false);
			m_encodeState = EncodeStateUnbufferedEncoding;
			break;
		}
		case EncodeStateIdle:
			m_encodeState = EncodeStateUnbufferedIdle;
			break;
		case EncodeStateUnbufferedIdle:
		case EncodeStateUnbufferedEncoding:
		default:
			// NOLINTNEXTLINE(hicpp-static-assert,misc-static-assert,cert-dcl03-c)
			stored_assert(false);
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
		case EncodeStateIdle:
		case EncodeStateEncoding:
		default:
			// NOLINTNEXTLINE(hicpp-static-assert,misc-static-assert,cert-dcl03-c)
			stored_assert(false);
		}
	}
}

size_t DebugArqLayer::mtu() const
{
	size_t mtu = base::mtu();
	if(mtu == 0)
		return 0;
	if(mtu <= 4U)
		return 1U;
	return mtu - 4U;
}

/*!
 * \brief Compute the successive sequence number.
 */
uint32_t DebugArqLayer::nextSeq(uint32_t seq)
{
	seq = (uint32_t)((seq + 1U) % 0x8000000);
	// cppcheck-suppress[knownConditionTrueFalse,unmatchedSuppression]
	return seq ? seq : 1U;
}

/*!
 * \brief Decode the sequence number from a buffer.
 */
uint32_t DebugArqLayer::decodeSeq(uint8_t*& buffer, size_t& len)
{
	uint32_t seq = 0;
	uint8_t flag = 0x40;

	while(true) {
		if(!len--)
			return ~0U;
		seq = (seq << 7U) | (*buffer & (flag - 1U));
		if(!(*buffer++ & flag))
			return seq;
		flag = 0x80;
	}
}

/*!
 * \brief Encode a sequence number into a buffer.
 * \return the number of bytes written (maximum of 4)
 */
size_t DebugArqLayer::encodeSeq(uint32_t seq, void* buffer)
{
	uint8_t* buffer_ = static_cast<uint8_t*>(buffer);
	if(seq < 0x40U) {
		buffer_[0] = (uint8_t)(seq & 0x3fU);
		return 1;
	} else if(seq < 0x2000) {
		buffer_[0] = (uint8_t)(0x40U | ((seq >> 7U) & 0x3fU));
		buffer_[1] = (uint8_t)(seq & 0x7fU);
		return 2;
	} else if(seq < 0x100000) {
		buffer_[0] = (uint8_t)(0x40U | ((seq >> 14U) & 0x3fU));
		buffer_[1] = (uint8_t)(0x80U | ((seq >> 7U) & 0x7fU));
		buffer_[2] = (uint8_t)(seq & 0x7fU);
		return 3;
	} else if(seq < 0x8000000) {
		buffer_[0] = (uint8_t)(0x40U | ((seq >> 21U) & 0x3fU));
		buffer_[1] = (uint8_t)(0x80U | ((seq >> 14U) & 0x7fU));
		buffer_[2] = (uint8_t)(0x80U | ((seq >> 7U) & 0x7fU));
		buffer_[3] = (uint8_t)(seq & 0x7fU);
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
	0x00, 0xA6, 0xEA, 0x4C, 0x72, 0xD4, 0x98, 0x3E, 0xE4, 0x42, 0x0E, 0xA8, 0x96, 0x30, 0x7C,
	0xDA, 0x6E, 0xC8, 0x84, 0x22, 0x1C, 0xBA, 0xF6, 0x50, 0x8A, 0x2C, 0x60, 0xC6, 0xF8, 0x5E,
	0x12, 0xB4, 0xDC, 0x7A, 0x36, 0x90, 0xAE, 0x08, 0x44, 0xE2, 0x38, 0x9E, 0xD2, 0x74, 0x4A,
	0xEC, 0xA0, 0x06, 0xB2, 0x14, 0x58, 0xFE, 0xC0, 0x66, 0x2A, 0x8C, 0x56, 0xF0, 0xBC, 0x1A,
	0x24, 0x82, 0xCE, 0x68, 0x1E, 0xB8, 0xF4, 0x52, 0x6C, 0xCA, 0x86, 0x20, 0xFA, 0x5C, 0x10,
	0xB6, 0x88, 0x2E, 0x62, 0xC4, 0x70, 0xD6, 0x9A, 0x3C, 0x02, 0xA4, 0xE8, 0x4E, 0x94, 0x32,
	0x7E, 0xD8, 0xE6, 0x40, 0x0C, 0xAA, 0xC2, 0x64, 0x28, 0x8E, 0xB0, 0x16, 0x5A, 0xFC, 0x26,
	0x80, 0xCC, 0x6A, 0x54, 0xF2, 0xBE, 0x18, 0xAC, 0x0A, 0x46, 0xE0, 0xDE, 0x78, 0x34, 0x92,
	0x48, 0xEE, 0xA2, 0x04, 0x3A, 0x9C, 0xD0, 0x76, 0x3C, 0x9A, 0xD6, 0x70, 0x4E, 0xE8, 0xA4,
	0x02, 0xD8, 0x7E, 0x32, 0x94, 0xAA, 0x0C, 0x40, 0xE6, 0x52, 0xF4, 0xB8, 0x1E, 0x20, 0x86,
	0xCA, 0x6C, 0xB6, 0x10, 0x5C, 0xFA, 0xC4, 0x62, 0x2E, 0x88, 0xE0, 0x46, 0x0A, 0xAC, 0x92,
	0x34, 0x78, 0xDE, 0x04, 0xA2, 0xEE, 0x48, 0x76, 0xD0, 0x9C, 0x3A, 0x8E, 0x28, 0x64, 0xC2,
	0xFC, 0x5A, 0x16, 0xB0, 0x6A, 0xCC, 0x80, 0x26, 0x18, 0xBE, 0xF2, 0x54, 0x22, 0x84, 0xC8,
	0x6E, 0x50, 0xF6, 0xBA, 0x1C, 0xC6, 0x60, 0x2C, 0x8A, 0xB4, 0x12, 0x5E, 0xF8, 0x4C, 0xEA,
	0xA6, 0x00, 0x3E, 0x98, 0xD4, 0x72, 0xA8, 0x0E, 0x42, 0xE4, 0xDA, 0x7C, 0x30, 0x96, 0xFE,
	0x58, 0x14, 0xB2, 0x8C, 0x2A, 0x66, 0xC0, 0x1A, 0xBC, 0xF0, 0x56, 0x68, 0xCE, 0x82, 0x24,
	0x90, 0x36, 0x7A, 0xDC, 0xE2, 0x44, 0x08, 0xAE, 0x74, 0xD2, 0x9E, 0x38, 0x06, 0xA0, 0xEC,
	0x4A};

/*!
 * \brief Ctor.
 */
Crc8Layer::Crc8Layer(ProtocolLayer* up, ProtocolLayer* down)
	: base(up, down)
	, m_crc(init)
{}

void Crc8Layer::reset()
{
	m_crc = init;
	base::reset();
}

void Crc8Layer::decode(void* buffer, size_t len)
{
	if(len == 0)
		return;

	// cppcheck-suppress[constVariablePointer,unmatchedSuppression]
	uint8_t* buffer_ = static_cast<uint8_t*>(buffer);
	uint8_t crc = init;
	for(size_t i = 0; i < len - 1; i++)
		crc = compute(crc, buffer_[i]);

	if(crc != buffer_[len - 1])
		// Invalid.
		return;

	base::decode(buffer, len - 1);
}

void Crc8Layer::encode(void const* buffer, size_t len, bool last)
{
	uint8_t const* buffer_ = static_cast<uint8_t const*>(buffer);
	for(size_t i = 0; i < len; i++)
		m_crc = compute(m_crc, buffer_[i]);

	base::encode(buffer, len, false);

	if(last) {
		base::encode(&m_crc, 1, true);
		m_crc = init;
	}
}

uint8_t Crc8Layer::compute(uint8_t input, uint8_t crc)
{
	// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
	return crc8_table[(uint8_t)(input ^ crc)];
}

size_t Crc8Layer::mtu() const
{
	size_t mtu = base::mtu();
	if(mtu == 0 || mtu > 256U)
		return 256U;
	if(mtu <= 2U)
		return 1U;
	return mtu - 1U;
}



//////////////////////////////
// Crc16Layer
//

// Generated by http://www.sunshine2k.de/coding/javascript/crc/crc_js.html
static const uint16_t crc16_table[] = {
	0x0000, 0xBAAD, 0xCFF7, 0x755A, 0x2543, 0x9FEE, 0xEAB4, 0x5019, 0x4A86, 0xF02B, 0x8571,
	0x3FDC, 0x6FC5, 0xD568, 0xA032, 0x1A9F, 0x950C, 0x2FA1, 0x5AFB, 0xE056, 0xB04F, 0x0AE2,
	0x7FB8, 0xC515, 0xDF8A, 0x6527, 0x107D, 0xAAD0, 0xFAC9, 0x4064, 0x353E, 0x8F93, 0x90B5,
	0x2A18, 0x5F42, 0xE5EF, 0xB5F6, 0x0F5B, 0x7A01, 0xC0AC, 0xDA33, 0x609E, 0x15C4, 0xAF69,
	0xFF70, 0x45DD, 0x3087, 0x8A2A, 0x05B9, 0xBF14, 0xCA4E, 0x70E3, 0x20FA, 0x9A57, 0xEF0D,
	0x55A0, 0x4F3F, 0xF592, 0x80C8, 0x3A65, 0x6A7C, 0xD0D1, 0xA58B, 0x1F26, 0x9BC7, 0x216A,
	0x5430, 0xEE9D, 0xBE84, 0x0429, 0x7173, 0xCBDE, 0xD141, 0x6BEC, 0x1EB6, 0xA41B, 0xF402,
	0x4EAF, 0x3BF5, 0x8158, 0x0ECB, 0xB466, 0xC13C, 0x7B91, 0x2B88, 0x9125, 0xE47F, 0x5ED2,
	0x444D, 0xFEE0, 0x8BBA, 0x3117, 0x610E, 0xDBA3, 0xAEF9, 0x1454, 0x0B72, 0xB1DF, 0xC485,
	0x7E28, 0x2E31, 0x949C, 0xE1C6, 0x5B6B, 0x41F4, 0xFB59, 0x8E03, 0x34AE, 0x64B7, 0xDE1A,
	0xAB40, 0x11ED, 0x9E7E, 0x24D3, 0x5189, 0xEB24, 0xBB3D, 0x0190, 0x74CA, 0xCE67, 0xD4F8,
	0x6E55, 0x1B0F, 0xA1A2, 0xF1BB, 0x4B16, 0x3E4C, 0x84E1, 0x8D23, 0x378E, 0x42D4, 0xF879,
	0xA860, 0x12CD, 0x6797, 0xDD3A, 0xC7A5, 0x7D08, 0x0852, 0xB2FF, 0xE2E6, 0x584B, 0x2D11,
	0x97BC, 0x182F, 0xA282, 0xD7D8, 0x6D75, 0x3D6C, 0x87C1, 0xF29B, 0x4836, 0x52A9, 0xE804,
	0x9D5E, 0x27F3, 0x77EA, 0xCD47, 0xB81D, 0x02B0, 0x1D96, 0xA73B, 0xD261, 0x68CC, 0x38D5,
	0x8278, 0xF722, 0x4D8F, 0x5710, 0xEDBD, 0x98E7, 0x224A, 0x7253, 0xC8FE, 0xBDA4, 0x0709,
	0x889A, 0x3237, 0x476D, 0xFDC0, 0xADD9, 0x1774, 0x622E, 0xD883, 0xC21C, 0x78B1, 0x0DEB,
	0xB746, 0xE75F, 0x5DF2, 0x28A8, 0x9205, 0x16E4, 0xAC49, 0xD913, 0x63BE, 0x33A7, 0x890A,
	0xFC50, 0x46FD, 0x5C62, 0xE6CF, 0x9395, 0x2938, 0x7921, 0xC38C, 0xB6D6, 0x0C7B, 0x83E8,
	0x3945, 0x4C1F, 0xF6B2, 0xA6AB, 0x1C06, 0x695C, 0xD3F1, 0xC96E, 0x73C3, 0x0699, 0xBC34,
	0xEC2D, 0x5680, 0x23DA, 0x9977, 0x8651, 0x3CFC, 0x49A6, 0xF30B, 0xA312, 0x19BF, 0x6CE5,
	0xD648, 0xCCD7, 0x767A, 0x0320, 0xB98D, 0xE994, 0x5339, 0x2663, 0x9CCE, 0x135D, 0xA9F0,
	0xDCAA, 0x6607, 0x361E, 0x8CB3, 0xF9E9, 0x4344, 0x59DB, 0xE376, 0x962C, 0x2C81, 0x7C98,
	0xC635, 0xB36F, 0x09C2,
};

/*!
 * \brief Ctor.
 */
Crc16Layer::Crc16Layer(ProtocolLayer* up, ProtocolLayer* down)
	: base(up, down)
	, m_crc(init)
{}

void Crc16Layer::reset()
{
	m_crc = init;
	base::reset();
}

void Crc16Layer::decode(void* buffer, size_t len)
{
	if(len < 2)
		return;

	// cppcheck-suppress[constVariablePointer,unmatchedSuppression]
	uint8_t* buffer_ = static_cast<uint8_t*>(buffer);
	uint16_t crc = init;
	for(size_t i = 0; i < len - 2; i++)
		crc = compute(buffer_[i], crc);

	if(crc != ((uint16_t)((uint16_t)(buffer_[len - 2] << 8U) | buffer_[len - 1])))
		// Invalid.
		return;

	base::decode(buffer, len - 2);
}

void Crc16Layer::encode(void const* buffer, size_t len, bool last)
{
	uint8_t const* buffer_ = static_cast<uint8_t const*>(buffer);
	for(size_t i = 0; i < len; i++)
		m_crc = compute(buffer_[i], m_crc);

	base::encode(buffer, len, false);

	if(last) {
		uint8_t crc[2] = {(uint8_t)(m_crc >> 8U), (uint8_t)m_crc};
		base::encode(crc, sizeof(crc), true);
		m_crc = init;
	}
}

uint16_t Crc16Layer::compute(uint8_t input, uint16_t crc)
{
	// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
	return (uint16_t)(crc16_table[input ^ (uint8_t)(crc >> 8U)] ^ (uint16_t)(crc << 8U));
}

size_t Crc16Layer::mtu() const
{
	size_t mtu = base::mtu();
	if(mtu == 0 || mtu > 256U)
		return 256U;
	if(mtu <= 3U)
		return 1U;
	return mtu - 2U;
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

void BufferLayer::reset()
{
	m_buffer.clear();
	base::reset();
}

/*!
 * \brief Collects all partial buffers, and passes the full encoded data on when \p last is set.
 */
void BufferLayer::encode(void const* buffer, size_t len, bool last)
{
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
	, m_enable(true)
{}

void PrintLayer::decode(void* buffer, size_t len)
{
	if(m_f && enabled()) {
		String::type prefix;
		if(m_name)
			prefix += m_name;
		prefix += " < ";

		String::type s = string_literal(buffer, len, prefix.c_str());
		s += "\n";
		(void)fputs(s.c_str(), m_f);
	}

	base::decode(buffer, len);
}

void PrintLayer::encode(void const* buffer, size_t len, bool last)
{
	if(m_f && enabled()) {
		String::type prefix;
		if(m_name)
			prefix += m_name;

		if(last)
			prefix += " > ";
		else
			prefix += " * ";

		String::type s = string_literal(buffer, len, prefix.c_str());
		s += "\n";
		(void)fputs(s.c_str(), m_f);
	}

	base::encode(buffer, len, last);
}

/*!
 * \brief Set the \c FILE to write to.
 * \param f the \c FILE, set to \c nullptr to disable output
 */
void PrintLayer::setFile(FILE* f)
{
	m_f = f;
}

/*!
 * \brief Return the \c FILE that is written to.
 */
FILE* PrintLayer::file() const
{
	return m_f;
}

/*!
 * \brief Enable printing all messages.
 */
void PrintLayer::enable(bool enable)
{
	m_enable = enable;
}

/*!
 * \brief Disable printing all messages.
 *
 * This is equivalent to calling \c enable(false).
 */
void PrintLayer::disable()
{
	enable(false);
}

/*!
 * \brief Returns if printing is currently enabled.
 */
bool PrintLayer::enabled() const
{
	return m_enable;
}



//////////////////////////////
// Loopback
//

/*!
 * \brief Constructor.
 */
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
impl::Loopback1::Loopback1(ProtocolLayer& from, ProtocolLayer& to)
	: m_to(&to)
	, m_buffer()
	, m_capacity()
	, m_len()
{
	wrap(from);
}

/*!
 * \brief Destructor.
 */
impl::Loopback1::~Loopback1()
{
	// NOLINTNEXTLINE(cppcoreguidelines-owning-memory, cppcoreguidelines-no-malloc)
	free(m_buffer);
}

void impl::Loopback1::reset()
{
	m_len = 0;
	base::reset();
}

void impl::Loopback1::reserve(size_t capacity)
{
	if(likely(capacity <= m_capacity))
		return;

	// NOLINTNEXTLINE(cppcoreguidelines-owning-memory, cppcoreguidelines-no-malloc)
	void* p = realloc(m_buffer, capacity);
	if(unlikely(!p)) {
#ifdef STORED_cpp_exceptions
		throw std::bad_alloc();
#else
		std::terminate();
#endif
	}

	m_buffer = static_cast<char*>(p);
	m_capacity = capacity;
}

/*!
 * \brief Collect partial data, and passes into the \c decode() of \c to when it has the full
 * message.
 */
void impl::Loopback1::encode(void const* buffer, size_t len, bool last)
{
	if(likely(len > 0)) {
		if(unlikely(m_len + len > m_capacity))
			reserve(m_len + len + ExtraAlloc);

		memcpy(m_buffer + m_len, buffer, len);
		m_len += len;
	}

	if(last) {
		size_t l = m_len;
		m_len = 0;
		m_to->decode(m_buffer, l);
	}
}

/*!
 * \brief Constructs a bidirectional loopback of stacks \p a and \p b.
 */
Loopback::Loopback(ProtocolLayer& a, ProtocolLayer& b)
	: m_a2b(a, b)
	, m_b2a(b, a)
{}

/*!
 * \brief Reserve heap memory to assemble partial messages.
 *
 * The capacity is allocated twice; one for both directions.
 */
void Loopback::reserve(size_t capacity)
{
	m_a2b.reserve(capacity);
	m_b2a.reserve(capacity);
}



//////////////////////////////
// PolledLayer
//

/*!
 * \brief Dtor.
 */
PolledLayer::~PolledLayer()
{
	// We would like to close(), but at this point, the subclass was
	// already destructed. So, make sure to close the handles
	// in your subclass dtor.
	// close();

	cleanup(m_poller);
}

/*!
 * \brief Return a poller.
 *
 * Pollers are reused between calls, but only allocated the first time is is needed.
 */
Poller& PolledLayer::poller()
{
	if(!m_poller)
		m_poller = new Poller(); // NOLINT(cppcoreguidelines-owning-memory)

	return *m_poller;
}



//////////////////////////////
// PolledFileLayer
//

/*!
 * \brief Dtor.
 */
PolledFileLayer::~PolledFileLayer() is_default

/*!
 * \brief Block on a file descriptor.
 *
 * Blocking is done using a #stored::Poller.
 *
 * \param fd the file descriptor to block on
 * \param forReading when \c true, it blocks till stored::Poller::PollIn, otherwise PollOut
 * \param timeout_us if zero, this function does not block. -1 blocks indefinitely.
 * \param suspend if \c true, do a suspend of the thread while blocking, otherwise allow fiber
 * switching (when using Zth) \return 0 on success, otherwise an \c errno
 */
int PolledFileLayer::block(
	PolledFileLayer::fd_type fd, bool forReading, long timeout_us, bool suspend)
{
	STORED_UNUSED(suspend)

	setLastError(0);

	Poller& poller = this->poller();

	Pollable::Events events = forReading ? Pollable::PollIn : Pollable::PollOut;

	int err = 0;
#ifdef STORED_OS_WINDOWS
	PollableHandle pollbl(fd, events);
#else
	PollableFd pollbl(fd, events);
#endif
	int res = poller.add(pollbl);

	if(res) {
		err = res;
		goto done;
	}

	while(true) {
		Poller::Result const& pres = poller.poll((int)(timeout_us / 1000L));

		if(pres.empty()) {
			if(timeout_us <= 0 && errno == EINTR)
				// Interrupted. Retry.
				continue;

			err = errno;
			if(!err || timeout_us < 0)
				// Should not happen.
				err = EINVAL;
			break;
		} else if((pres[0]->events
			   & (Pollable::Events)(Pollable::PollErr | Pollable::PollHup))
				  .any()) {
			// Something is wrong with the pipe/socket.
			err = EIO;
			break;
		} else if((pres[0]->events & events).any()) {
			// Got it.
			break;
		}
	}

	res = poller.remove(pollbl);

	if(res && !err)
		err = res;

done:
	switch(err) {
	case EAGAIN:
#if EAGAIN != EWOULDBLOCK
	case EWOULDBLOCK:
#endif
	case EINTR:
	case 0:
		break;
	default:
		close();
	}

	return setLastError(err);
}



#ifdef STORED_OS_WINDOWS
//////////////////////////////
// PolledSocketLayer
//

/*!
 * \brief Dtor.
 */
PolledSocketLayer::~PolledSocketLayer() is_default
#endif



//////////////////////////////
// FileLayer
//

#ifdef STORED_HAVE_STDIO

#  ifndef STORED_OS_WINDOWS

/*!
 * \brief Generic ctor for subclasses.
 *
 * Use this ctor if and only if inheriting this class, otherwise use another one.
 * Call #init() afterwards to register the actual file descriptors.
 *
 * It sets #lastError() appropriately.
 */
FileLayer::FileLayer(ProtocolLayer* up, ProtocolLayer* down)
	: base(up, down)
	, m_fd_r(-1)
	, m_fd_w(-1)
{
	setLastError(EBADF);
}

/*!
 * \brief Ctor for two already opened file descriptors.
 *
 * If \p fd_w is -1, \p fd_r is used to write to as well.
 *
 * Do not use this ctor when inheriting this class.
 *
 * It sets #lastError() appropriately.
 */
FileLayer::FileLayer(int fd_r, int fd_w, size_t bufferSize, ProtocolLayer* up, ProtocolLayer* down)
	: base(up, down)
	, m_fd_r(-1)
	, m_fd_w(-1)
{
#    ifdef STORED_OS_POSIX
	// NOLINTNEXTLINE(hicpp-signed-bitwise)
	fcntl(fd_r, F_SETFL, fcntl(fd_r, F_GETFL) | O_NONBLOCK);
	// NOLINTNEXTLINE(hicpp-signed-bitwise)
	fcntl(fd_w, F_SETFL, fcntl(fd_w, F_GETFL) | O_NONBLOCK);
#    endif

	init(fd_r, fd_w == -1 ? fd_r : fd_w, bufferSize);
}

/*!
 * \brief Ctor for two files to be opened.
 *
 * If \p name_w is \c nullptr, \p name_r is used to write to as well.  Files
 * are created when required. If the file exists, writing will append data to
 * the file (it is not truncated).
 *
 * Do not use this ctor when inheriting this class.
 *
 * It sets #lastError() appropriately.
 */
FileLayer::FileLayer(
	char const* name_r, char const* name_w, size_t bufferSize, ProtocolLayer* up,
	ProtocolLayer* down)
	: base(up, down)
	, m_fd_r(-1)
	, m_fd_w(-1)
{
	int r = -1;
	int w = -1;

	if(!name_w || strcmp(name_r, name_w) == 0) {
		// NOLINTNEXTLINE(hicpp-signed-bitwise)
		w = r = open(name_r, O_RDWR | O_APPEND | O_CREAT | O_NONBLOCK, 0666);
		if(w == -1)
			goto error;
	} else {
		// NOLINTNEXTLINE(hicpp-signed-bitwise,bugprone-branch-clone)
		r = open(name_r, O_RDONLY | O_CREAT | O_NONBLOCK, 0666);
		if(r == -1)
			goto error;

		// NOLINTNEXTLINE(hicpp-signed-bitwise)
		w = open(name_w, O_WRONLY | O_APPEND | O_CREAT | O_NONBLOCK, 0666);
		if(w == -1)
			goto error;
	}

	init(r, w, bufferSize);
	return;

error:
	if(r != -1)
		::close(r);
	if(w != -1)
		::close(w);
	setLastError(errno ? errno : EBADF);
}

/*!
 * \brief Initialize the given file descriptors for use in this layer.
 *
 * Does not return, but sets #lastError() appropriately.
 */
// cppcheck-suppress passedByValue
void FileLayer::init(FileLayer::fd_type fd_r, FileLayer::fd_type fd_w, size_t bufferSize)
{
	stored_assert(m_fd_r == -1);
	stored_assert(m_fd_w == -1);

	m_fd_r = fd_r;
	m_fd_w = fd_w;

	if(m_fd_r == -1 || m_fd_w == -1) {
		setLastError(EBADF);
	} else {
		m_bufferRead.resize(bufferSize);
		setLastError(0);
	}
}

/*!
 * \brief Dtor.
 *
 * It calls #close_(), not #close() as the latter is virtual.
 */
FileLayer::~FileLayer()
{
	close_();
}

/*!
 * \brief Virtual wrapper around #close_().
 */
void FileLayer::close()
{
	close_();
}

/*!
 * \brief Close the file descriptors.
 */
void FileLayer::close_()
{
	if(m_fd_r != -1)
		::close(m_fd_r);

	if(m_fd_w != -1 && m_fd_r != m_fd_w)
		::close(m_fd_w);

	m_fd_r = -1;
	m_fd_w = -1;

	base::close();
}

/*!
 * \brief Check if the file descriptors are open.
 */
bool FileLayer::isOpen() const
{
	return m_fd_r != -1;
}

/*!
 * \copydoc stored::PolledFileLayer::encode(void const*, size_t, bool)
 * \details Sets #lastError() appropriately.
 */
void FileLayer::encode(void const* buffer, size_t len, bool last)
{
	if(m_fd_w == -1) {
		setLastError(EBADF);
done:
		base::encode(buffer, len, last);
		return;
	}

	char const* buf = static_cast<char const*>(buffer);
	size_t buflen = len;

	if(m_fd_w == STDOUT_FILENO)
		// Do not reorder stdout's FILE buffer and our write()s. Flush first.
		(void)fflush(stdout);

	while(buflen > 0) {
		ssize_t written = write(m_fd_w, buf, buflen);

		switch(written) {
		case -1:
			switch(errno) {
#    if EAGAIN != EWOULDBLOCK
			case EWOULDBLOCK:
#    endif
			case EAGAIN: {
				if(this->block(m_fd_w, false, -1, m_fd_w == STDOUT_FILENO))
					return;

				// We should be able to write more now. Retry.
				continue;
			}
			default:
				goto error;
			}
			break;
		case 0:
			errno = EIO;
			goto error;
		default:
			stored_assert(buflen >= (size_t)written);
			buflen -= (size_t)written;
			buf += written;
		}
	}

	setLastError(0);
	goto done;

error:
	close();
	setLastError(errno ? errno : EIO);
	goto done;
}

/*!
 * \brief Returns the file descriptor to be used by a #stored::Poller in order to call #recv().
 */
FileLayer::fd_type FileLayer::fd() const
{
	return m_fd_r;
}

/*!
 * \brief Returns the read file descriptor.
 */
FileLayer::fd_type FileLayer::fd_r() const
{
	return m_fd_r;
}

/*!
 * \brief Returns the write file descriptor.
 */
FileLayer::fd_type FileLayer::fd_w() const
{
	return m_fd_w;
}

/*!
 * \brief Try to receive data from the file descriptor and forward it for decoding.
 * \param timeout_us if zero, this function does not block. -1 blocks indefinitely.
 * \return 0 on success, otherwise an errno
 */
int FileLayer::recv(long timeout_us)
{
	if(fd_r() == -1)
		return setLastError(EBADF);

again:
	ssize_t cnt = read(fd_r(), m_bufferRead.data(), m_bufferRead.size());

	if(cnt == -1) {
		switch(errno) {
		case EAGAIN:
#    if EAGAIN != EWOULDBLOCK
		case EWOULDBLOCK:
#    endif
			if(timeout_us == 0)
				return setLastError(errno);

			if(block(fd_r(), true, timeout_us))
				return lastError();

			goto again;
		default:
			close();
			return setLastError(errno);
		}
	} else if(cnt == 0) {
		// EOF
		// Don't close(). The file may grow, or the FIFO may get filled later on.
		return setLastError(EAGAIN);
	} else {
		decode(m_bufferRead.data(), (size_t)cnt);
		return setLastError(0);
	}
}

#  else // STORED_OS_WINDOWS

/*!
 * \brief Checks if the given handle is valid.
 */
static bool isValidHandle(HANDLE h)
{
	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-cstyle-cast)
	return h != INVALID_HANDLE_VALUE;
}

/*!
 * \brief Ctor for two already opened files.
 *
 * If \p fd_w is -1, \p fd_r is used to write to as well.
 *
 * Do not use this ctor when inheriting this class.
 *
 * It sets #lastError() appropriately.
 */
FileLayer::FileLayer(int fd_r, int fd_w, size_t bufferSize, ProtocolLayer* up, ProtocolLayer* down)
	: base(up, down)
	, m_fd_r(INVALID_HANDLE_VALUE) // NOLINT(cppcoreguidelines-pro-type-cstyle-cast)
	, m_fd_w(INVALID_HANDLE_VALUE) // NOLINT(cppcoreguidelines-pro-type-cstyle-cast)
	, m_overlappedRead()
	, m_overlappedWrite()
	, m_this(this)
	, m_writeLen()
{
	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-cstyle-cast)
	HANDLE h_r = (HANDLE)_get_osfhandle(fd_r);
	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-cstyle-cast)
	HANDLE h_w = fd_w == -1 ? h_r : (HANDLE)_get_osfhandle(fd_w);
	init(h_r, h_w, bufferSize);
}

/*!
 * \brief Ctor for two files to be opened.
 *
 * If \p name_w is \c nullptr, \p name_r is used to write to as well.  Files
 * are created when required. If the file exists, writing will append data to
 * the file (it is not truncated).
 *
 * Do not use this ctor when inheriting this class.
 *
 * It sets #lastError() appropriately.
 */
FileLayer::FileLayer(
	char const* name_r, char const* name_w, size_t bufferSize, ProtocolLayer* up,
	ProtocolLayer* down)
	: base(up, down)
	, m_fd_r(INVALID_HANDLE_VALUE) // NOLINT(cppcoreguidelines-pro-type-cstyle-cast)
	, m_fd_w(INVALID_HANDLE_VALUE) // NOLINT(cppcoreguidelines-pro-type-cstyle-cast)
	, m_overlappedRead()
	, m_overlappedWrite()
	, m_this(this)
	, m_writeLen()
{
	setLastError(EBADF);

	if(!name_r) {
		setLastError(EINVAL);
		return;
	}

	bool isCOM_r = ::strncmp(name_r, "\\\\.\\COM", 7) == 0;
	bool isCOM_w = name_w && ::strncmp(name_w, "\\\\.\\COM", 7) == 0;

	if(!name_w || strcmp(name_r, name_w) == 0) {
		HANDLE h = INVALID_HANDLE_VALUE; // NOLINT(cppcoreguidelines-pro-type-cstyle-cast)
		if(!isValidHandle(
			   (h = CreateFile(
				    name_r,
				    GENERIC_READ | GENERIC_WRITE,	// NOLINT
				    FILE_SHARE_READ | FILE_SHARE_WRITE, // NOLINT
				    NULL, (DWORD)(isCOM_r ? OPEN_EXISTING : OPEN_ALWAYS),
				    FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, // NOLINT
				    NULL)))) {
			setLastError(EINVAL);
			return;
		}

		init(h, h, bufferSize);
	} else {
		HANDLE h_r = INVALID_HANDLE_VALUE; // NOLINT(cppcoreguidelines-pro-type-cstyle-cast)
		if(!isValidHandle(
			   (h_r = CreateFile(
				    name_r, GENERIC_READ,
				    FILE_SHARE_READ | FILE_SHARE_WRITE, // NOLINT
				    NULL, (DWORD)(isCOM_r ? OPEN_EXISTING : OPEN_ALWAYS),
				    FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, // NOLINT
				    NULL)))) {
			setLastError(EINVAL);
			return;
		}

		HANDLE h_w = INVALID_HANDLE_VALUE; // NOLINT(cppcoreguidelines-pro-type-cstyle-cast)
		if(!isValidHandle(
			   (h_w = CreateFile(
				    name_w, GENERIC_WRITE,
				    FILE_SHARE_READ | FILE_SHARE_WRITE, // NOLINT
				    NULL, (DWORD)(isCOM_w ? OPEN_EXISTING : OPEN_ALWAYS),
				    FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, // NOLINT
				    NULL)))) {
			setLastError(EINVAL);
			return;
		}

		init(h_r, h_w, bufferSize);
	}
}

/*!
 * \brief Ctor for to existing HANDLEs.
 *
 * If \p h_w is \c INVALID_HANDLE_VALUE, \p h_r is used for writing too.
 *
 * The HANDLEs must be opened with FILE_FLAG_OVERLAPPED.
 *
 * Do not use this ctor when inheriting this class.
 *
 * It sets #lastError() appropriately.
 */
FileLayer::FileLayer(
	HANDLE h_r, HANDLE h_w, size_t bufferSize, ProtocolLayer* up, ProtocolLayer* down)
	: base(up, down)
	, m_fd_r(INVALID_HANDLE_VALUE) // NOLINT(cppcoreguidelines-pro-type-cstyle-cast)
	, m_fd_w(INVALID_HANDLE_VALUE) // NOLINT(cppcoreguidelines-pro-type-cstyle-cast)
	, m_overlappedRead()
	, m_overlappedWrite()
	, m_this(this)
	, m_writeLen()
{
	init(h_r, !isValidHandle(h_w) ? h_r : h_w, bufferSize);
}

/*!
 * \brief Generic ctor for subclasses.
 *
 * Use this ctor if and only if inheriting this class, otherwise use another one.
 * Call #init() afterwards to register the actual file descriptors.
 *
 * It sets #lastError() appropriately.
 */
FileLayer::FileLayer(ProtocolLayer* up, ProtocolLayer* down)
	: base(up, down)
	, m_fd_r(INVALID_HANDLE_VALUE) // NOLINT(cppcoreguidelines-pro-type-cstyle-cast)
	, m_fd_w(INVALID_HANDLE_VALUE) // NOLINT(cppcoreguidelines-pro-type-cstyle-cast)
	, m_overlappedRead()
	, m_overlappedWrite()
	, m_this(this)
	, m_writeLen()
{
	setLastError(EBADF);
}

/*!
 * \brief Initialize the given file descriptors for use in this layer.
 *
 * Does not return, but sets #lastError() appropriately.
 */
void FileLayer::init(FileLayer::fd_type fd_r, FileLayer::fd_type fd_w, size_t bufferSize)
{
	stored_assert(!isValidHandle(m_fd_r));
	stored_assert(!isValidHandle(m_fd_w));

	if(!isValidHandle(fd_r)) {
		setLastError(EBADF);
		goto error;
	}

	if(!isValidHandle(fd_w)) {
		setLastError(EBADF);
		goto error;
	}

	m_fd_r = fd_r;
	m_fd_w = fd_w;

	setLastError(0);

	m_bufferRead.resize(bufferSize);

	m_overlappedRead.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if(!m_overlappedRead.hEvent) {
		// NOLINTNEXTLINE(cppcoreguidelines-pro-type-cstyle-cast)
		m_overlappedRead.hEvent = INVALID_HANDLE_VALUE;
		setLastError(ENOMEM);
		goto error;
	}

	m_overlappedWrite.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if(!m_overlappedWrite.hEvent) {
		// NOLINTNEXTLINE(cppcoreguidelines-pro-type-cstyle-cast)
		m_overlappedWrite.hEvent = INVALID_HANDLE_VALUE;
		setLastError(ENOMEM);
		goto error;
	}

	// If init() is called by a ctor, which was invoked by a subclass,
	// FileLayer::startRead() is called, not the one of the subclass.
	// Therefore, a subclass should use the FileRead(up,down) ctor, and
	// call init() afterwards.
	//
	// NOLINTNEXTLINE(clang-analyzer-optin.cplusplus.VirtualCall)
	startRead();

	// EAGAIN is what you may expect by startRead().
	if(lastError() == EAGAIN)
		setLastError(0);

	return;

error:
	close_();

	if(!lastError())
		setLastError(EIO);
}

/*!
 * \brier Dtor.
 *
 * It calls #close_(), instead of #close(), as the latter is virtual.
 */
FileLayer::~FileLayer()
{
	close_();
}

/*!
 * \brief Virtual wrapper for #close_().
 */
void FileLayer::close()
{
	close_();
}

/*!
 * \brief Close the file handles.
 */
void FileLayer::close_()
{
	if(m_fd_r != m_fd_w && isValidHandle(m_fd_w)) {
		FlushFileBuffers(m_fd_w);
		CancelIo(m_fd_w);
		CloseHandle(m_fd_w);
	}
	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-cstyle-cast)
	m_fd_w = INVALID_HANDLE_VALUE;

	if(isValidHandle(m_fd_r)) {
		FlushFileBuffers(m_fd_r);
		CancelIo(m_fd_r);
		CloseHandle(m_fd_r);
		// NOLINTNEXTLINE(cppcoreguidelines-pro-type-cstyle-cast)
		m_fd_r = INVALID_HANDLE_VALUE;
	}

	if(m_overlappedRead.hEvent) {
		CloseHandle(m_overlappedRead.hEvent);
		m_overlappedRead.hEvent = NULL;
	}

	if(m_overlappedWrite.hEvent) {
		CloseHandle(m_overlappedWrite.hEvent);
		m_overlappedWrite.hEvent = NULL;
	}

	base::close();
}

/*!
 * \brief Checks if the files are still open.
 */
bool FileLayer::isOpen() const
{
	return isValidHandle(m_fd_r);
}

/*!
 * \brief Try to finish a previously started overlapped write.
 * \return 0 on success, otherwise an \c errno
 */
int FileLayer::finishWrite(bool block)
{
	size_t offset = 0;

wait_prev_write:
	if(!HasOverlappedIoCompleted(&m_overlappedWrite)) {
		if(block)
			if(this->block(overlappedWrite().hEvent, false))
				return lastError();

		DWORD written = 0;
		if(!GetOverlappedResult(fd_w(), &m_overlappedWrite, &written, FALSE)) {
			switch(GetLastError()) {
			case ERROR_IO_INCOMPLETE:
			case ERROR_IO_PENDING:
				stored_assert(!block);
				return 0;
			default:
				goto error;
			}
		}

		while(written < m_writeLen) {
			// Restart for the remaining data.
			offset += written;
			resetOverlappedWrite();
			m_writeLen -= written;
			if(block) {
				if(WriteFile(
					   fd_w(), &m_bufferWrite[offset], (DWORD)m_writeLen,
					   &written, &m_overlappedWrite)) {
					// Completed immediately.
				} else {
					switch(GetLastError()) {
					case ERROR_IO_INCOMPLETE:
					case ERROR_IO_PENDING:
						goto wait_prev_write;
					default:
						goto error;
					}
				}
			} else {
				if(WriteFileEx(
					   fd_w(), &m_bufferWrite[offset], (DWORD)m_writeLen,
					   &m_overlappedWrite,
					   // NOLINTNEXTLINE(cppcoreguidelines-pro-type-cstyle-cast)
					   (LPOVERLAPPED_COMPLETION_ROUTINE)(void*)&writeCompletionRoutine))
					return 0;
				else
					goto error;
			}
		}
	}

	SetEvent(m_overlappedWrite.hEvent);
	return 0;

error:
	close();
	return setLastError(EIO);
}

/*!
 * \brief Completion routine when an overlapped write has finished.
 *
 * If the write was incomplete (which is unlikely, but not guaranteed),
 * it will issue a new overlapped write to continue with the rest of the write buffer.
 */
void FileLayer::writeCompletionRoutine(
	// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
	DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped)
{
	STORED_UNUSED(dwNumberOfBytesTransfered)

	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-cstyle-cast)
	FileLayer* that = *(FileLayer**)((uintptr_t)lpOverlapped + sizeof(*lpOverlapped));
	stored_assert(that);

	if(dwErrorCode != 0) {
		that->close();
		that->setLastError(EIO);
	} else {
		// Make sure all data has been transferred.
		that->finishWrite(false);
	}
}

/*!
 * \copydoc stored::PolledFileLayer::encode(void const*, size_t, bool)
 * \details Sets #lastError() appropriately.
 */
void FileLayer::encode(void const* buffer, size_t len, bool last)
{
	if(!isValidHandle(fd_w())) {
		setLastError(EBADF);
done:
		// Done. It might be the case that the write already finished, but
		// not all data has been written. Let the next invocation of encode() handle that.
		base::encode(buffer, len, last);
		return;
	}

	finishWrite(true);

	// Issue a new write.
	stored_assert(len < (size_t)std::numeric_limits<DWORD>::max());
	m_bufferWrite.resize(len);
	memcpy(m_bufferWrite.data(), buffer, len);
	setLastError(0);
	resetOverlappedWrite();
	m_overlappedWrite.Offset = m_overlappedWrite.OffsetHigh = 0xffffffff;
	if(WriteFileEx(
		   fd_w(), m_bufferWrite.data(), (DWORD)len, &m_overlappedWrite,
		   // NOLINTNEXTLINE(cppcoreguidelines-pro-type-cstyle-cast)
		   (LPOVERLAPPED_COMPLETION_ROUTINE)(void*)&writeCompletionRoutine)) {
		// Already finished.
	} else {
		switch(GetLastError()) {
		case ERROR_IO_INCOMPLETE:
		case ERROR_IO_PENDING:
			break;
		default:
			goto error;
		}
	}

	goto done;

error:
	close();
	setLastError(EIO);
	goto done;
}

/*!
 * \brief Returns the file descriptor to be used by a #stored::Poller in order to call #recv().
 */
FileLayer::fd_type FileLayer::fd() const
{
	return m_overlappedRead.hEvent;
}

/*!
 * \brief The read handle.
 */
FileLayer::fd_type FileLayer::fd_r() const
{
	return m_fd_r;
}

/*!
 * \brief The write handle.
 */
FileLayer::fd_type FileLayer::fd_w() const
{
	return m_fd_w;
}

/*!
 * \brief Initiate an overlapped read.
 */
int FileLayer::startRead()
{
	bool didDecode = false;

again:
	if(!isValidHandle(m_fd_r))
		return setLastError(EBADF);

	size_t readable = available();
	if(readable == 0) {
		close();
		return setLastError(EIO);
	}

	if(readable > m_bufferRead.size())
		readable = m_bufferRead.size();

	if(readable > std::numeric_limits<DWORD>::max())
		readable = std::numeric_limits<DWORD>::max();

	DWORD bytesRead = 0;

	// Read everything there is to read.
	// This should be finished quickly.
	resetOverlappedRead();
	if(ReadFile(m_fd_r, m_bufferRead.data(), (DWORD)readable, &bytesRead, &m_overlappedRead)) {
		// Finished immediately.
		decode(m_bufferRead.data(), (size_t)bytesRead);
		didDecode = true;
		goto again;
	} else {
		switch(GetLastError()) {
		case ERROR_IO_INCOMPLETE:
		case ERROR_IO_PENDING:
			// The read is pending.
			return setLastError(didDecode ? 0 : EAGAIN);
		default:
			close();
			return setLastError(EIO);
		}
	}
}

/*!
 * \brief Returns the number of bytes available to read.
 *
 * This value is used for the next overlapped read.
 */
size_t FileLayer::available()
{
	if(!isValidHandle(m_fd_r))
		return 0;

	switch(GetFileType(m_fd_r)) {
	case FILE_TYPE_DISK: {
		LONG posHigh = 0L;
		DWORD res = SetFilePointer(m_fd_r, 0L, &posHigh, FILE_CURRENT);
		if(res == INVALID_SET_FILE_POINTER)
			return 0;

		uint64_t pos = (uint64_t)res | ((uint64_t)posHigh << 32U);

		DWORD sizeHigh = 0;
		res = GetFileSize(m_fd_r, &sizeHigh);
		if(res == INVALID_FILE_SIZE)
			return 0;

		uint64_t size = (uint64_t)res | ((uint64_t)sizeHigh << 32U);

		stored_assert(size >= pos);

		uint64_t avail = size - pos;
		return (size_t)std::min<uint64_t>(avail, std::numeric_limits<size_t>::max());
	}
	case FILE_TYPE_PIPE: {
		DWORD readable = 0;
		if(!PeekNamedPipe(m_fd_r, NULL, 0, NULL, &readable, NULL))
			return 0;

		if(readable == 0)
			// There is nothing to read. Issue a read of 1 byte, which will
			// overlap/block.
			return 1;

		return (size_t)readable;
	}
	default:
		// Read byte-by-byte.
		return 1;
	}
}

/*!
 * \brief Try to receive data from the file descriptor and forward it for decoding.
 * \param timeout_us if zero, this function does not block. -1 blocks indefinitely.
 * \return 0 on success, otherwise an errno
 */
int FileLayer::recv(long timeout_us)
{
	bool didDecode = false;

	setLastError(0);

	if(timeout_us)
		if(block(overlappedRead().hEvent, true, timeout_us))
			return lastError();

again:
	DWORD read = 0;
	if(GetOverlappedResult(m_fd_r, &m_overlappedRead, &read, FALSE)) {
		// Finished the previous read.
		decode(m_bufferRead.data(), read);
		// Go issue the next read.
		return startRead() == EAGAIN ? setLastError(0) : lastError();
	} else {
		switch(GetLastError()) {
		case ERROR_IO_INCOMPLETE:
		case ERROR_IO_PENDING:
			// Still waiting.
			if(didDecode)
				// But we did something already.
				return setLastError(0);

			if(timeout_us) {
				// Go block till we are readable.
				if(block(overlappedRead().hEvent, true, timeout_us))
					return lastError();

				goto again;
			}

			// Non blocking.
			return setLastError(EAGAIN);
		default:
			close();
			return setLastError(EIO);
		}
	}
}

/*!
 * \brief Returns the OVERLAPPED struct for read operations.
 */
OVERLAPPED& FileLayer::overlappedRead()
{
	return m_overlappedRead;
}

/*!
 * \brief Returns the OVERLAPPED struct for read operations.
 */
OVERLAPPED& FileLayer::overlappedWrite()
{
	return m_overlappedWrite;
}

/*!
 * \brief Reset the OVERLAPPED struct for a new read operation.
 *
 * Make sure the previous read has completed before.
 */
void FileLayer::resetOverlappedRead()
{
	HANDLE hEvent = m_overlappedRead.hEvent;
	memset(&m_overlappedRead, 0, sizeof(m_overlappedRead));
	m_overlappedRead.hEvent = hEvent;
	ResetEvent(hEvent);
}

/*!
 * \brief Reset the OVERLAPPED struct for a new write operation.
 *
 * Make sure the previous write has completed before.
 */
void FileLayer::resetOverlappedWrite()
{
	HANDLE hEvent = m_overlappedWrite.hEvent;
	memset(&m_overlappedWrite, 0, sizeof(m_overlappedWrite));
	m_overlappedWrite.hEvent = hEvent;
	ResetEvent(hEvent);
}

#  endif // STORED_OS_WINDOWS



//////////////////////////////
// NamedPipeLayer
//

#  if defined(STORED_OS_WINDOWS) || defined(DOXYGEN)
/*!
 * \brief Ctor for the server part of a named pipe.
 *
 * The given name is prefixed with \c \\\\.\\pipe\\.
 */
NamedPipeLayer::NamedPipeLayer(
	char const* name, DWORD openMode, ProtocolLayer* up, ProtocolLayer* down)
	: base(up, down)
	, m_state(StateInit)
	, m_openMode(openMode)
{
	stored_assert(name);
	stored_assert(openMode == Duplex || openMode == Inbound || openMode == Outbound);

	m_name = "\\\\.\\pipe\\";
	m_name += name;

	HANDLE h = CreateNamedPipe(
		m_name.c_str(),
		openMode | FILE_FLAG_OVERLAPPED,		 // NOLINT(hicpp-signed-bitwise)
		PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, // NOLINT(hicpp-signed-bitwise)
		1, BufferSize, BufferSize, 0, NULL);

	if(!isValidHandle(h)) {
		close_();
		setLastError(EAGAIN);
		return;
	}

	init(h, h);
}

/*!
 * \brief Dtor.
 */
NamedPipeLayer::~NamedPipeLayer()
{
	close_();
}

/*!
 * \brief Close the pipe.
 */
void NamedPipeLayer::close_()
{
	if(isValidHandle(handle())) {
		FlushFileBuffers(handle());
		CancelIo(handle());
		DisconnectNamedPipe(handle());
	}

	base::close();
}

/*!
 * \brief Resets the connection to accept a new incoming one.
 */
void NamedPipeLayer::close()
{
	reopen();
}

/*!
 * \brief Resets the connection to accept a new incoming one.
 */
void NamedPipeLayer::reopen()
{
	if(isValidHandle(handle()) && m_state > StateConnecting) {
		// Don't really close, just reconnect.
		FlushFileBuffers(handle());
		CancelIo(handle());
		DisconnectNamedPipe(handle());
		reset();
		m_state = StateInit;
		setLastError(EIO);
		// Reconnect.
		recv();
	}
}

int NamedPipeLayer::recv(long timeout_us)
{
	bool didDecode = false;
again:
	switch(m_state) {
	case StateInit:
		resetOverlappedRead();
		if(ConnectNamedPipe(handle(), &overlappedRead())) {
			// Connected immediately.
			m_state = StateConnected;
			connected();
			didDecode = true;
			goto again;
		} else {
			switch(GetLastError()) {
			case ERROR_IO_INCOMPLETE:
			case ERROR_IO_PENDING:
				m_state = StateConnecting;
				if(timeout_us)
					goto again;
				return setLastError(EAGAIN);
			case ERROR_PIPE_CONNECTED:
				// The client arrived early.
				m_state = StateConnected;
				didDecode = true;
				goto again;
			default:
				close();
				// Give up.
				m_state = StateError;
				return setLastError(EIO);
			}
		}
		break;
	case StateConnecting: {
		DWORD dummy = 0;
		if(!didDecode && timeout_us)
			block(overlappedRead().hEvent, true, timeout_us);

		if(GetOverlappedResult(fd_r(), &overlappedRead(), &dummy, FALSE)) {
			m_state = StateConnected;
			connected();
			if(m_openMode != Outbound && startRead()) {
				if(lastError() == EAGAIN) {
					if(!didDecode && timeout_us < 0)
						// Wait for more data.
						goto again;
					else
						return setLastError(didDecode ? 0 : EAGAIN);
				} else
					// Some other error. Done.
					return lastError();
			}
			didDecode = true;
			goto again;
		} else {
			switch(GetLastError()) {
			case ERROR_IO_INCOMPLETE:
			case ERROR_IO_PENDING:
				return setLastError(didDecode ? 0 : EAGAIN);
			default:
				m_state = StateError;
				return setLastError(EIO);
			}
		}
	}
	case StateConnected:
		if(m_openMode == Outbound)
			return setLastError(0); // NOLINT(bugprone-branch-clone)
		else if(base::recv(didDecode ? 0 : timeout_us) == EAGAIN && didDecode)
			return setLastError(0);
		else
			return lastError();

	case StateError:
		return setLastError(EINVAL);

	default:
		stored_assert(false);
		return EINVAL;
	}
}

int NamedPipeLayer::startRead()
{
	switch(m_state) {
	case StateInit:
		return recv(false);
	case StateConnected:
		if(m_openMode == Outbound)
			return setLastError(0);
		else
			return base::startRead();
	case StateError:
		return setLastError(EINVAL);
	case StateConnecting:
	default:
		return setLastError(EAGAIN);
	}
}

void NamedPipeLayer::encode(void const* buffer, size_t len, bool last)
{
	switch(m_state) {
	case StateConnected:
		base::encode(buffer, len, last);
		break;
	case StateInit:
	case StateConnecting:
	case StateError:
	default:
		// Don't actually write, as we are not connected.
		// However, do pass downstream.
		// NOLINTNEXTLINE(bugprone-parent-virtual-call)
		base::base::encode(buffer, len, last);
		setLastError(EAGAIN);
	}
}

/*!
 * \brief Returns the full name of the pipe.
 *
 * This name can be used to open it elsewhere as a normal file.
 */
String::type const& NamedPipeLayer::name() const
{
	return m_name;
}

/*!
 * \brief Returns the pipe handle.
 */
HANDLE NamedPipeLayer::handle() const
{
	return fd_r();
}

/*!
 * \brief Checks if the pipe is connected.
 */
bool NamedPipeLayer::isConnected() const
{
	return m_state == StateConnected;
}

#  elif defined(STORED_OS_POSIX)
/*!
 * \brief Ctor.
 *
 * The \c name is the path to the FIFO file, which is created when it does not exist yet.
 */
NamedPipeLayer::NamedPipeLayer(
	char const* name, Access openMode, ProtocolLayer* up, ProtocolLayer* down)
	: base(up, down)
	, m_name(name)
	, m_openMode(openMode)
{
	if(mkfifo(name, 0666)) {
		int e = errno;
		// NOLINTNEXTLINE(hicpp-multiway-paths-covered)
		switch(e) {
		case EEXIST:
			// That's fine.
			break;
		default:
			setLastError(e ? e : EBADF);
			return;
		}
	}

	if(openMode == Inbound) {
		int fd_r = open(name, O_RDONLY | O_NONBLOCK);
		int fd_w = open("/dev/null", O_WRONLY);
		init(fd_r, fd_w);
	} else {
#    ifdef STORED_OS_OSX
		typedef void (*sighandler_t)(int);
#    endif
		sighandler_t oldh = signal(SIGPIPE, SIG_IGN);

		if(oldh == SIG_ERR) {
			setLastError(errno ? errno : EINVAL);
			return;
		} else if(oldh != SIG_IGN && oldh != SIG_DFL) {
			// Oops, revert the handler and accept SIGPIPEs.
			if(signal(SIGPIPE, oldh) == SIG_ERR) {
				// Really oops... Now we broke something.
#    ifdef STORED_cpp_exceptions
				throw std::runtime_error("Cannot restore SIGPIPE handler");
#    else
				std::terminate();
#    endif
			}
		}

		// Postpone open() till first encode().
		setLastError(0);
	}
}

NamedPipeLayer::~NamedPipeLayer()
{
	close_();
}

/*!
 * \brief Return the path of the FIFO file.
 */
String::type const& NamedPipeLayer::name() const
{
	return m_name;
}

void NamedPipeLayer::encode(void const* buffer, size_t len, bool last)
{
	if(!isConnected())
		reopen();

	base::encode(buffer, len, last);
}

/*!
 * \brief Checks if the pipe is connected.
 */
bool NamedPipeLayer::isConnected() const
{
	// Unknown. Assume it is.
	return isOpen();
}

/*!
 * \brief Resets the connection to accept a new incoming one.
 */
void NamedPipeLayer::reopen()
{
	if(m_openMode == Inbound)
		// Nothing to do.
		return;

	// Try opening now. For the write (outbound) end to open properly, the read (inbound) end
	// should have been opened already.

	close();

	int fd_r = open("/dev/null", O_RDONLY);
	int fd_w =
		open(name().c_str(), O_WRONLY | O_NONBLOCK | O_APPEND); // May exit with ENXIO when
									// no read is open yet.

	if(fd_w == -1) {
		int e = errno;
		::close(fd_r);
		setLastError(e ? e : EBADF);
		return;
	}

	init(fd_r, fd_w);
	connected();
}
#  endif // STORED_OS_POSIX



//////////////////////////////
// DoublePipeLayer
//

#  if defined(STORED_OS_WINDOWS) || defined(STORED_OS_POSIX) || defined(DOXYGEN)
DoublePipeLayer::DoublePipeLayer(
	char const* name_r, char const* name_w, ProtocolLayer* up, ProtocolLayer* down)
	: base(up, down)
	, m_r(name_r, NamedPipeLayer::Inbound, this, nullptr)
	, m_w(name_w, NamedPipeLayer::Outbound)
{
	if(m_r.lastError())
		setLastError(m_r.lastError());
	else
		setLastError(m_w.lastError());
}

DoublePipeLayer::~DoublePipeLayer() is_default

void DoublePipeLayer::encode(void const* buffer, size_t len, bool last)
{
	m_w.encode(buffer, len, last);
	setLastError(m_w.lastError());
	base::encode(buffer, len, last);
}

bool DoublePipeLayer::isOpen() const
{
	return m_r.isOpen() || m_w.isOpen();
}

int DoublePipeLayer::recv(long timeout_us)
{
	m_w.recv();
	return setLastError(m_r.recv(timeout_us));
}

DoublePipeLayer::fd_type DoublePipeLayer::fd() const
{
	return m_r.fd();
}

/*!
 * \brief Checks if both pipes are connected.
 */
bool DoublePipeLayer::isConnected() const
{
	return m_r.isConnected() && m_w.isConnected();
}

void DoublePipeLayer::close()
{
	reopen();
}

void DoublePipeLayer::reopen()
{
	m_r.reopen();
	m_w.reopen();
}

void DoublePipeLayer::reset()
{
	m_r.reset();
	m_w.reset();
	base::reset();
}
#  endif // STORED_OS_WINDOWS || STORED_OS_POSIX



//////////////////////////////
// XsimLayer
//

#  if defined(STORED_OS_WINDOWS) || defined(STORED_OS_POSIX) || defined(DOXYGEN)
/*!
 * \brief Ctor.
 *
 * The \p pipe_prefix species the name of the pipe in Windows, which will be prepended with
 * <tt>\\\\.\\pipe\\</tt> . In other POSIX-like systems, the name is used as the filename of the
 * (to be created) FIFO.
 */
XsimLayer::XsimLayer(char const* pipe_prefix, ProtocolLayer* up, ProtocolLayer* down)
	: base((String::type(pipe_prefix) += "_from_xsim").c_str(),
	       (String::type(pipe_prefix) += "_to_xsim").c_str(), up, down)
	, m_callback(*this)
	, m_req((String::type(pipe_prefix) += "_req_xsim").c_str(), NamedPipeLayer::Inbound)
	, m_inFlight()
{
	m_req.wrap(m_callback);

	if(!lastError() && m_req.lastError())
		setLastError(m_req.lastError());
}

XsimLayer::~XsimLayer() is_default

void XsimLayer::encode(void const* buffer, size_t len, bool last)
{
	m_inFlight += len;
	base::encode(buffer, len, last);

	if(lastError()) {
		// Reset, such that we send a keepAlive afterwards.
		m_inFlight = 0;
	}
}

NamedPipeLayer& XsimLayer::req()
{
	return m_req;
}

void XsimLayer::reset()
{
	m_inFlight = 0;
	m_req.reset();
	base::reset();
	keepAlive();
}

void XsimLayer::keepAlive()
{
	// Make sure that there is always one byte to read, otherwise xsim may
	// hang.  You would expect a < 1 in the line below, but that still makes
	// xsim hang.  Unsure why.
	if(m_inFlight <= 1) {
		char buf = KeepAlive;
		encode(&buf, 1, true);
	}
}

void XsimLayer::decoded(size_t len)
{
	if(len > m_inFlight)
		m_inFlight = 0;
	else
		m_inFlight -= len;

	keepAlive();
}

int XsimLayer::recv(long timeout_us)
{
	int resReq = m_req.recv();
	int resBase = base::recv(timeout_us);

	switch(resBase) {
	case EAGAIN:
	case EINTR:
	case 0:
		switch(resReq) {
		case EAGAIN:
		case EINTR:
		case 0:
			return resBase;
		case EIO:
			// Try to recover all channels.
			reopen();
			STORED_FALLTHROUGH
		default:
			return setLastError(resReq);
		}
	case EIO:
		// Try to recover all channels.
		reopen();
		STORED_FALLTHROUGH
	default:
		return resBase;
	}
}

void XsimLayer::reopen()
{
	m_inFlight = 0;
	m_req.reopen();
	base::reopen();
	keepAlive();
}
#  endif // STORED_OS_WINDOWS || STORED_OS_POSIX



//////////////////////////////
// StdioLayer
//

#  ifdef STORED_OS_WINDOWS
/*!
 * \brief Ctor.
 */
StdioLayer::StdioLayer(size_t bufferSize, ProtocolLayer* up, ProtocolLayer* down)
	: base(up, down)
	, m_fd_r(GetStdHandle(STD_INPUT_HANDLE))
	, m_fd_w(GetStdHandle(STD_OUTPUT_HANDLE))
	, m_pipe_r()
	, m_pipe_w()
{
	DWORD mode = 0;
	if(GetConsoleMode(m_fd_r, &mode)) {
		// NOLINTNEXTLINE(hicpp-signed-bitwise)
		SetConsoleMode(m_fd_r, mode & ~(DWORD)(ENABLE_MOUSE_INPUT | ENABLE_WINDOW_INPUT));
		FlushConsoleInputBuffer(m_fd_r);
	} else {
		// stdin is probably redirected, in which case it is a named pipe.
		m_pipe_r = true;
	}

	if(GetConsoleMode(m_fd_w, &mode)) {
		// NOLINTNEXTLINE(hicpp-signed-bitwise)
		SetConsoleMode(m_fd_w, mode | 4 /*ENABLE_VIRTUAL_TERMINAL_PROCESSING*/);
	} else {
		// stdout is probably redirected, in which case it is a named pipe.
		m_pipe_w = true;
	}

	m_bufferRead.resize(bufferSize);
}

/*!
 * \brief Dtor.
 */
StdioLayer::~StdioLayer()
{
	close_();
}

/*!
 * \brief Checks if the stdin is actually a pipe instead of an interactive console.
 */
bool StdioLayer::isPipeIn() const
{
	return m_pipe_r;
}

/*!
 * \brief Checks if the stdout is actually a pipe instead of an interactive console.
 */
bool StdioLayer::isPipeOut() const
{
	return m_pipe_w;
}

int StdioLayer::block(fd_type fd, bool forReading, long timeout_us, bool suspend)
{
	STORED_UNUSED(fd)
	STORED_UNUSED(forReading)
	STORED_UNUSED(timeout_us)
	STORED_UNUSED(suspend)
	stored_assert(false);
	return setLastError(EINVAL);
}

bool StdioLayer::isOpen() const
{
	return isValidHandle(m_fd_r);
}

void StdioLayer::close()
{
	close_();
}

void StdioLayer::close_()
{
	if(isValidHandle(m_fd_r) && isPipeIn()) {
		CloseHandle(m_fd_r);
		// NOLINTNEXTLINE(cppcoreguidelines-pro-type-cstyle-cast)
		m_fd_r = INVALID_HANDLE_VALUE;
	}

	base::close();
}

StdioLayer::fd_type StdioLayer::fd_r() const
{
	return m_fd_r;
}

StdioLayer::fd_type StdioLayer::fd_w() const
{
	return m_fd_w;
}

// Guess what, Overlapped I/O is not supported on the console...
int StdioLayer::recv(long timeout_us)
{
	if(!isValidHandle(fd_r()))
		return setLastError(EBADF);

	bool didDecode = false;
	DWORD cnt = 0;
again:
	if(isPipeIn()) {
		if(!PeekNamedPipe(fd_r(), NULL, 0, NULL, &cnt, NULL))
			goto error;
	} else if(!GetNumberOfConsoleInputEvents(fd_r(), &cnt)) {
		goto error;
	}

	if(cnt == 0) {
		if(didDecode)
			return setLastError(0);
		if(timeout_us >= 0)
			return EAGAIN;

		cnt = 1;
	}

	if((size_t)cnt > m_bufferRead.size())
		cnt = (DWORD)m_bufferRead.size();

	if(ReadFile(fd_r(), m_bufferRead.data(), cnt, &cnt, NULL)) {
		decode(m_bufferRead.data(), (size_t)cnt);
		didDecode = true;
		goto again;
	}

error:
	close();
	return setLastError(EIO);
}

void StdioLayer::encode(void const* buffer, size_t len, bool last)
{
	if(!isValidHandle(fd_w())) {
		setLastError(EBADF);
done:
		base::encode(buffer, len, last);
		return;
	}

	char const* buf = static_cast<char const*>(buffer);
	size_t rem = len;
	while(rem > 0) {
		DWORD written = 0;
		if(!WriteFile(fd_w(), buf, (DWORD)rem, &written, NULL)) {
			close();
			setLastError(EIO);
			goto done;
		}

		buf += written;
		rem -= written;
	}

	goto done;
}

StdioLayer::fd_type StdioLayer::fd() const
{
	return fd_r();
}

#  else // !STORED_OS_WINDOWS

/*!
 * \brief Ctor.
 */
StdioLayer::StdioLayer(size_t bufferSize, ProtocolLayer* up, ProtocolLayer* down)
	: base(STDIN_FILENO, STDOUT_FILENO, bufferSize, up, down)
{}

#  endif // !STORED_OS_WINDOWS



//////////////////////////////
// SerialLayer
//

#  ifdef STORED_OS_WINDOWS
SerialLayer::SerialLayer(
	char const* name, unsigned long baud, bool rtscts, bool xonxoff, ProtocolLayer* up,
	ProtocolLayer* down)
	: base(up, down)
{
	HANDLE h = INVALID_HANDLE_VALUE; // NOLINT(cppcoreguidelines-pro-type-cstyle-cast)
	if(!isValidHandle(
		   (h = CreateFile(
			    name,
			    GENERIC_READ | GENERIC_WRITE,	// NOLINT
			    FILE_SHARE_READ | FILE_SHARE_WRITE, // NOLINT
			    NULL, OPEN_EXISTING,
			    FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, // NOLINT
			    NULL)))) {
		setLastError(EINVAL);
		return;
	}

	init(h, h, BufferSize);

	if(lastError())
		return;

	DCB dcb;
	memset(&dcb, 0, sizeof(DCB));
	dcb.DCBlength = sizeof(dcb);

	if(!GetCommState(h, &dcb)) {
		setLastError(EIO);
		return;
	}

	dcb.BaudRate = (DWORD)baud;
	dcb.ByteSize = 8;
	dcb.Parity = NOPARITY;
	dcb.StopBits = ONESTOPBIT;
	dcb.fOutxCtsFlow = rtscts;
	dcb.fOutxDsrFlow = FALSE;
	dcb.fDtrControl = DTR_CONTROL_DISABLE;
	dcb.fNull = FALSE;
	dcb.fRtsControl = (DWORD)(rtscts ? RTS_CONTROL_HANDSHAKE : RTS_CONTROL_DISABLE);
	dcb.fAbortOnError = FALSE;
	dcb.fOutX = xonxoff;
	dcb.fInX = xonxoff;
	dcb.XonChar = '\x11';
	dcb.XoffChar = '\x13';

	if(!SetCommState(h, &dcb)) {
		setLastError(EIO);
		return;
	}

	resetAutoBaud();
	// Ignore auto baud errors, as it is an optional feature.
	setLastError(0);
}

int SerialLayer::resetAutoBaud()
{
	if(!isOpen())
		return setLastError(EAGAIN);

	setLastError(0);

	HANDLE h = fd_r();
	int res = 0;
	if(!SetCommBreak(h))
		res = EIO;

	// Kind of arbitrary sleep. And Windows does not really guarantee how much
	// we will sleep actually. Could be improved.
	delay_ms(125);

	if(!ClearCommBreak(h))
		res = EIO;

	// Some sleep is required here. It seems that when sending data immediately
	// after ClearCommBreak(), it gets lost somehow (at least in the setup I
	// tested).
	delay_ms(125);

	encode("\x11", 1);

	if(res)
		setLastError(res);

	return lastError();
}

#  elif defined(STORED_OS_POSIX)
static speed_t baud_to_speed_t(unsigned long baud)
{
	switch(baud) {
#    ifdef B50
	case 50UL:
		return B50;
#    endif
#    ifdef B75
	case 75UL:
		return B75;
#    endif
#    ifdef B110
	case 110UL:
		return B110;
#    endif
#    ifdef B134
	case 134UL:
		return B134;
#    endif
#    ifdef B150
	case 150UL:
		return B150;
#    endif
#    ifdef B200
	case 200UL:
		return B200;
#    endif
#    ifdef B300
	case 300UL:
		return B300;
#    endif
#    ifdef B600
	case 600UL:
		return B600;
#    endif
#    ifdef B1200
	case 1200UL:
		return B1200;
#    endif
#    ifdef B1800
	case 1800UL:
		return B1800;
#    endif
#    ifdef B2400
	case 2400UL:
		return B2400;
#    endif
#    ifdef B4800
	case 4800UL:
		return B4800;
#    endif
#    ifdef B9600
	case 9600UL:
		return B9600;
#    endif
#    ifdef B19200
	case 19200UL:
		return B19200;
#    endif
#    ifdef B38400
	case 38400UL:
		return B38400;
#    endif
#    ifdef B57600
	case 57600UL:
		return B57600;
#    endif
#    ifdef B115200
	case 115200UL:
		return B115200;
#    endif
#    ifdef B230400
	case 230400UL:
		return B230400;
#    endif
#    ifdef B460800
	case 460800UL:
		return B460800;
#    endif
#    ifdef B500000
	case 500000UL:
		return B500000;
#    endif
#    ifdef B576000
	case 576000UL:
		return B576000;
#    endif
#    ifdef B921600
	case 921600UL:
		return B921600;
#    endif
#    ifdef B1000000
	case 1000000UL:
		return B1000000;
#    endif
#    ifdef B1152000
	case 1152000UL:
		return B1152000;
#    endif
#    ifdef B1500000
	case 1500000UL:
		return B1500000;
#    endif
#    ifdef B2000000
	case 2000000UL:
		return B2000000;
#    endif
#    ifdef B2500000
	case 2500000UL:
		return B2500000;
#    endif
#    ifdef B3000000
	case 3000000UL:
		return B3000000;
#    endif
#    ifdef B3500000
	case 3500000UL:
		return B3500000;
#    endif
#    ifdef B4000000
	case 4000000UL:
		return B4000000;
#    endif
	default:
		return B0;
	}
}

SerialLayer::SerialLayer(
	char const* name, unsigned long baud, bool rtscts, bool xonxoff, ProtocolLayer* up,
	ProtocolLayer* down)
	: base(up, down)
{
	int fd = -1;

	// NOLINTNEXTLINE(hicpp-signed-bitwise)
	fd = open(name, O_RDWR | O_APPEND | O_CREAT | O_NONBLOCK, 0666);
	if(fd == -1) {
		setLastError(errno ? errno : EBADF);
		return;
	}

	init(fd, fd, BufferSize);

	if(lastError() || !isatty(fd))
		return;

	struct termios config = {};
	if(tcgetattr(fd, &config) < 0) {
		setLastError(errno);
		return;
	}

	config.c_iflag &=
		// NOLINTNEXTLINE(hicpp-signed-bitwise)
		(tcflag_t) ~(BRKINT | ICRNL | INLCR | PARMRK | INPCK | ISTRIP | IXON | IXOFF);
	if(xonxoff) {
		// NOLINTNEXTLINE(hicpp-signed-bitwise)
		config.c_iflag |= (tcflag_t)(IXON | IXOFF);
	}
	config.c_oflag = 0;
	// NOLINTNEXTLINE(hicpp-signed-bitwise)
	config.c_lflag &= (tcflag_t) ~(ECHO | ECHONL | ICANON | IEXTEN | ISIG);
	// NOLINTNEXTLINE(hicpp-signed-bitwise)
	config.c_cflag &= (tcflag_t) ~(CSIZE | PARENB | CSTOPB);
	// NOLINTNEXTLINE(hicpp-signed-bitwise)
	config.c_cflag |= CS8;
	if(rtscts) {
#    ifdef CNEW_RTSCTS
		config.c_cflag |= CNEW_RTSCTS;
#    elif defined(CRTSCTS)
		config.c_cflag |= CRTSCTS;
#    else
		setLastError(EINVAL);
		return;
#    endif
	}
	config.c_cc[VMIN] = 0;
	config.c_cc[VTIME] = 0;

	speed_t speed = baud_to_speed_t(baud);
	if(speed == B0) {
		setLastError(EINVAL);
		return;
	}

	if(cfsetispeed(&config, speed) < 0 || cfsetospeed(&config, speed) < 0) {
		setLastError(errno);
		return;
	}

	if(tcsetattr(fd, TCSANOW, &config) < 0) {
		setLastError(errno);
		return;
	}

	resetAutoBaud();
	setLastError(0);
}

int SerialLayer::resetAutoBaud()
{
	if(!isOpen())
		return setLastError(EAGAIN);

	setLastError(0);

	int fd = fd_r();
	tcdrain(fd);

	int res = 0;
	// NOLINTNEXTLINE(concurrency-mt-unsafe)
	if(tcsendbreak(fd, 0))
		res = errno;

	tcdrain(fd);

	// We should wait for the break, but it is unclear how long it could take.
	delay_ms(500);

	encode("\x11", 1);

	if(res)
		setLastError(res);

	return lastError();
}
#  endif // STORED_OS_POSIX
#endif	 // STORED_HAVE_STDIO

} // namespace stored
