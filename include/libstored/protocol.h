#ifndef LIBSTORED_PROTOCOL_H
#define LIBSTORED_PROTOCOL_H
// SPDX-FileCopyrightText: 2020-2024 Jochem Rutgers
//
// SPDX-License-Identifier: MPL-2.0

#ifdef __cplusplus

#  include <libstored/macros.h>
#  include <libstored/util.h>

#  include <cerrno>
#  include <deque>
#  include <string>
#  include <vector>

#  if STORED_cplusplus >= 201103L
#    include <functional>
#    include <utility>
#  endif

#  ifdef STORED_COMPILER_MSVC
#    include <io.h>
#    ifndef STDERR_FILENO
#      define STDERR_FILENO _fileno(stderr)
#    endif
#    ifndef STDOUT_FILENO
#      define STDOUT_FILENO _fileno(stdout)
#    endif
#    ifndef STDIN_FILENO
#      define STDIN_FILENO _fileno(stdin)
#    endif
#  elif !defined(STORED_OS_BAREMETAL)
#    include <unistd.h>
#  endif

#  include <cstdio>

namespace stored {

/*!
 * \brief Protocol layer base class.
 *
 * A layer is usually part of the protocol stack. Bytes are decoded and
 * forwarded to the layer above this one, and the layer above sends bytes
 * for encoding down.  Moreover, #decode() is the inverse of #encode().  It
 * is wise to stick to this concept, even though the interface of this
 * class allows more irregular structures, such that decoding and encoding
 * take a different path through the protocol layers.
 *
 * The implementation of this class does nothing except forwarding bytes.
 * Override encode() and decode() in a subclass.
 */
class ProtocolLayer {
	STORED_CLASS_NOCOPY(ProtocolLayer)
public:
	/*!
	 * \brief Constructor.
	 * \param up the layer above, which receives our decoded frames
	 * \param down the layer below, which receives our encoded frames
	 */
	explicit ProtocolLayer(ProtocolLayer* up = nullptr, ProtocolLayer* down = nullptr)
		: m_up(up)
		, m_down(down)
	{}

	virtual ~ProtocolLayer();

	/*!
	 * \brief Change the layer that receives our decoded frames.
	 * \param up the layer, which can be \c nullptr
	 */
	void setUp(ProtocolLayer* up = nullptr)
	{
		m_up = up;
		connected();
	}

	/*!
	 * \brief Change the layer that receives our encoded frames.
	 * \param down the layer, which can be \c nullptr
	 */
	void setDown(ProtocolLayer* down = nullptr)
	{
		m_down = down;
	}

	/*!
	 * \brief Return the lowest layer of the stack.
	 */
	ProtocolLayer& bottom()
	{
		ProtocolLayer* p = this;

		while(p->down())
			p = p->down();

		return *p;
	}

	/*!
	 * \brief Return the lowest layer of the stack.
	 */
	ProtocolLayer const& bottom() const
	{
		ProtocolLayer const* p = this;

		while(p->down())
			p = p->down();

		return *p;
	}

	/*!
	 * \brief Return the highest layer of the stack.
	 */
	ProtocolLayer& top()
	{
		ProtocolLayer* p = this;

		while(p->up())
			p = p->up();

		return *p;
	}

	/*!
	 * \brief Return the highest layer of the stack.
	 */
	ProtocolLayer const& top() const
	{
		ProtocolLayer const* p = this;

		while(p->up())
			p = p->up();

		return *p;
	}

	/*!
	 * \brief Sets the up/down layers of this layer and the given layer,
	 *	such that this layer wraps the given one.
	 *
	 * If the given layer was not the bottom of the stack, this layer
	 * injects itself in between the given layer and its wrapper.
	 *
	 * \return the new bottom layer of the stack
	 */
	ProtocolLayer& wrap(ProtocolLayer& up)
	{
		ProtocolLayer* b = &bottom();
		ProtocolLayer* d = up.down();

		if(d) {
			b->setDown(d);
			d->setUp(b);
			b = &d->bottom();
		}

		up.setDown(this);
		setUp(&up);
		return *b;
	}

	/*!
	 * \brief Sets the up/down layers of this layer and the given layer,
	 *	such that this layer is stacked on (or wrapped by) the given one.
	 *
	 * If the given layer was not the top of the stack, this layer injects
	 * itself between the given layer and its stacked one.
	 *
	 * \return the new top layer of the stack.
	 */
	ProtocolLayer& stack(ProtocolLayer& down)
	{
		ProtocolLayer* u = down.up();

		setDown(&down);
		down.setUp(this);

		ProtocolLayer* t = &top();

		if(u) {
			u->setDown(t);
			t->setUp(u);
			t = &u->top();
		}

		return *t;
	}

	/*!
	 * \brief Returns the layer above this one.
	 * \return the layer, or \c nullptr if there is none.
	 */
	ProtocolLayer* up() const
	{
		return m_up;
	}

	/*!
	 * \brief Returns the layer below this one.
	 * \return the layer, or \c nullptr if there is none.
	 */
	ProtocolLayer* down() const
	{
		return m_down;
	}

	/*!
	 * \brief Decode a frame and forward the decoded frame to the upper layer.
	 *
	 * The given buffer may be decoded in-place.
	 */
	virtual void decode(void* buffer, size_t len)
	{
		if(up())
			up()->decode(buffer, len);
	}

	/*!
	 * \brief Encodes the last part of the current frame.
	 */
	void encode()
	{
		encode(static_cast<void const*>(nullptr), 0, true);
	}

	/*!
	 * \brief Encode a (partial) frame and forward it to the lower layer.
	 *
	 * The given buffer will not be modified.
	 * A new buffer is allocated when required.
	 */
	virtual void encode(void const* buffer, size_t len, bool last = true)
	{
		if(down())
			down()->encode(buffer, len, last);
	}

	/*!
	 * \brief Flags the current response as purgeable.
	 *
	 * This may influence how a response is handled.  Especially, in case
	 * of retransmits of lost packets, one may decide to either reexecute
	 * the command, or to save the first response and resend it when the
	 * command was retransmitted. In that sense, a precious response
	 * (default) means that every layer should handle the data with case,
	 * as it cannot be recovered once it is lost. When the response is
	 * flagged purgeeble, the response may be thrown away after the first
	 * try to transmit it to the client.
	 *
	 * By default, all responses are precious.
	 */
	virtual void setPurgeableResponse(bool purgeable = true)
	{
		if(down())
			down()->setPurgeableResponse(purgeable);
	}

	/*!
	 * \brief Returns the maximum amount of data to be put in one message that is encoded.
	 *
	 * If there is a MTU applicable to the physical transport (like a CAN bus),
	 * override this method to reflect that value. Layers on top will decrease the MTU
	 * when there protocol adds headers, for example.
	 *
	 * \return the number of bytes, or 0 for infinity
	 */
	virtual size_t mtu() const
	{
		return down() ? down()->mtu() : 0;
	}

	/*!
	 * \brief Flushes all buffered message out of the stack (top-down), if possible.
	 *
	 * Any buffered, held back, queued messages are tried to be sent
	 * immediately.  A flush is always safe; it never destroys data in the
	 * stack, it only tries to force it out.
	 *
	 * \return \c true if successful and the stack is empty, or \c false if message are still
	 * blocked
	 */
	virtual bool flush()
	{
		return down() ? down()->flush() : true;
	}

	/*!
	 * \brief Reset the stack (top-down), and drop all messages.
	 */
	virtual void reset()
	{
		if(down())
			down()->reset();
	}

	/*!
	 * \brief (Re)connected notification (bottom-up).
	 */
	virtual void connected()
	{
		if(up())
			up()->connected();
	}

private:
	/*! \brief The layer above this one. */
	ProtocolLayer* m_up;
	/*! \brief The layer below this one. */
	ProtocolLayer* m_down;
};

/*!
 * \brief Escape ASCII control characters.
 *
 * This is required to encapsulate messages within #stored::TerminalLayer, for example.
 */
class AsciiEscapeLayer : public ProtocolLayer {
	STORED_CLASS_NOCOPY(AsciiEscapeLayer)
public:
	typedef ProtocolLayer base;

	static char const Esc = '\x7f';	    // DEL
	static char const EscMask = '\x1f'; // data bits of the next char

	explicit AsciiEscapeLayer(
		bool all = false, ProtocolLayer* up = nullptr, ProtocolLayer* down = nullptr);

	/*!
	 * \copydoc stored::ProtocolLayer::~ProtocolLayer()
	 */
	virtual ~AsciiEscapeLayer() override is_default

	virtual void decode(void* buffer, size_t len) override;
	virtual void encode(void const* buffer, size_t len, bool last = true) override;
#  ifndef DOXYGEN
	using base::encode;
#  endif
	virtual size_t mtu() const override;

protected:
	char needEscape(char c) const;

private:
	bool m_all;
};

/*!
 * \brief Extracts and injects Embedded Debugger messages in a stream of data, such as a terminal.
 *
 * The frame's boundaries are marked with APC and ST C1 control characters.
 */
class TerminalLayer : public ProtocolLayer {
	STORED_CLASS_NOCOPY(TerminalLayer)
public:
	typedef ProtocolLayer base;

	static char const Esc = '\x1b';	  // ESC
	static char const EscStart = '_'; // APC
	static char const EscEnd = '\\';  // ST
	enum { MaxBuffer = 1024 };

#  if STORED_cplusplus >= 201103L
	using NonDebugDecodeCallback = void(void* buf, size_t len);

	template <typename F, SFINAE_IS_FUNCTION(F, NonDebugDecodeCallback, int) = 0>
	// NOLINTNEXTLINE(misc-forwarding-reference-overload,bugprone-forwarding-reference-overload)
	explicit TerminalLayer(F&& cb, ProtocolLayer* up = nullptr, ProtocolLayer* down = nullptr)
		: TerminalLayer(up, down)
	{
		// cppcheck-suppress useInitializationList
		m_nonDebugDecodeCallback = // NOLINT(cppcoreguidelines-prefer-member-initializer)
			std::forward<F>(cb);
	}
#  else
	typedef void(NonDebugDecodeCallback)(void* buf, size_t len);

	explicit TerminalLayer(
		NonDebugDecodeCallback* cb, ProtocolLayer* up = nullptr,
		ProtocolLayer* down = nullptr);
#  endif
	explicit TerminalLayer(ProtocolLayer* up = nullptr, ProtocolLayer* down = nullptr);

	virtual ~TerminalLayer() override;

	virtual void decode(void* buffer, size_t len) override;
	virtual void encode(void const* buffer, size_t len, bool last = true) override;
#  ifndef DOXYGEN
	using base::encode;
#  endif
	virtual size_t mtu() const override;
	virtual void reset() override;

	virtual void nonDebugEncode(void const* buffer, size_t len);

protected:
	virtual void nonDebugDecode(void* buffer, size_t len);
	void encodeStart();
	void encodeEnd();

private:
	/*! \brief The callback to write non-debug decoded data to. */
	Callable<NonDebugDecodeCallback>::type m_nonDebugDecodeCallback;

	/*! \brief States of frame extraction. */
	enum State { StateNormal, StateNormalEsc, StateDebug, StateDebugEsc };
	/*! \brief State of frame extraction. */
	State m_decodeState;
	/*! \brief Buffer of to-be-decoded data. */
	Vector<char>::type m_buffer;

	/*! \brief State of frame injection. */
	bool m_encodeState;
};

/*!
 * \brief A layer that performs segmentation of the messages.
 *
 * Messages to be encoded are split with a maximum chunk size (MTU). At the end of each chunk,
 * either #ContinueMarker or the #EndMarker is inserted, depending on whether this was the last
 * chunk.  Incoming messages are reassembled until the #EndMarker is encountered.
 *
 * This layer assumes a lossless channel; all messages are received in order. If that is not the
 * case for your transport, wrap this layer in the #stored::DebugArqLayer or #stored::ArqLayer.
 */
class SegmentationLayer : public ProtocolLayer {
	STORED_CLASS_NOCOPY(SegmentationLayer)
public:
	typedef ProtocolLayer base;

	// Pick markers outside of ASCII control, as it might imply escaping.
	static char const ContinueMarker = 'C';
	static char const EndMarker = 'E';

	explicit SegmentationLayer(
		size_t mtu = 0, ProtocolLayer* up = nullptr, ProtocolLayer* down = nullptr);
	/*! \brief Dtor. */
	virtual ~SegmentationLayer() override is_default

	virtual void decode(void* buffer, size_t len) override;
	virtual void encode(void const* buffer, size_t len, bool last = true) override;
#  ifndef DOXYGEN
	using base::encode;
#  endif

	size_t mtu() const final;
	size_t lowerMtu() const;
	virtual void reset() override;

private:
	size_t m_mtu;
	Vector<char>::type m_decode;
	size_t m_encoded;
};

/*!
 * \brief A general purpose layer that performs Automatic Repeat Request operations on messages.
 *
 * This layer does not assume a specific message pattern. For #stored::Debugger, use
 * #stored::DebugArqLayer.
 *
 * Every message sent has to be acknowledged. There is no window; after sending a message, an ack
 * must be received before continuing.  The queue of messages is by default unlimited, but can be
 * set via the constructor.  If the limit is hit, the event callback is invoked.
 *
 * This layer prepends the message with a sequence number byte.  The MSb indicates if it is an ack,
 * the 6 LSb are the sequence number.  Sequence 0 is special; it resets the connection. It should
 * not be used during normal operation, so the next sequence number after 63 is 1.  Messages that do
 * not have a payload (so, no decode() has to be invoked upon receive), should set bit 6. This also
 * applies to the reset message. Bit 6 is implied for an ack.
 *
 * Retransmits are triggered every time a message is queued for encoding, or when #flush() is
 * called. There is no timeout specified.
 *
 * One may decide to use a #stored::SegmentationLayer higher in the protocol stack to reduce the
 * amount of data to retransmit when a message is lost (only one segment is retransmitted, not the
 * full message), but this may add the overhead of the sequence number and round-trip time per
 * segment. If the #stored::SegmentationLayer is used below the ArqLayer, normal-case behavior (no
 * packet loss) is most efficient, but the penalty of a retransmit may be higher. It is up to the
 * infrastructure and application requirements what is best.
 *
 * The layer has no notion of time, or time out for retransmits and acks.  The application must call
 * #flush() (for the whole stack), or #keepAlive() at a regular interval. Every invocation of either
 * function will do a retransmit of the head of the encode queue. If called to often, retransmits
 * may be done before the other party had a chance to respond. If called not often enough,
 * retransmits may take long and communication may be slowed down. Either way, it is functionally
 * correct. Determine for your application what is wise to do.
 *
 * A reset connection or peer can be recovered from by sending the reset message. The other peer
 * will also reset the communication. To prevent recursive resets, the ack and the reset response
 * must be in the same message; this way the peer knows that the reset was processed properly.
 * Successive resets are not required. A typical flow between peer A and B will be:
 *
 * \verbatim
 * A -> B: reset (0x40)
 * B -> A: ack (0x80), reset (0x40)
 * A -> B: ack (0x80)
 * \endverbatim
 *
 * Queued messages are retransmitted after the reset, although they may be duplicated when an ack is
 * lost during the reset. Messages are never completely lost.
 */
class ArqLayer : public ProtocolLayer {
	STORED_CLASS_NOCOPY(ArqLayer)
public:
	typedef ProtocolLayer base;

	static uint8_t const NopFlag =
		0x40U; //!< \brief Flag to indicate that the payload should be ignored.
	static uint8_t const AckFlag = 0x80U; //!< \brief Ack flag.
	static uint8_t const SeqMask = 0x3fU; //!< \brief Mask for sequence number.

	enum {
		/*! \brief Number of successive retransmits before the event is emitted. */
		RetransmitCallbackThreshold = 10,
	};

	explicit ArqLayer(
		size_t maxEncodeBuffer = 0, ProtocolLayer* up = nullptr,
		ProtocolLayer* down = nullptr);
	virtual ~ArqLayer() override;

	virtual void decode(void* buffer, size_t len) override;
	virtual void encode(void const* buffer, size_t len, bool last = true) override;
#  ifndef DOXYGEN
	using base::encode;
#  endif

	virtual size_t mtu() const override;
	virtual bool flush() override;
	virtual void reset() override;
	virtual void connected() override;
	void keepAlive();

	enum Event {
		/*!
		 * \brief No event.
		 */
		EventNone,

		/*!
		 * \brief An unexpected reset message has been received.
		 *
		 * The reset message remains unanswered, until #reset() is called.
		 * The callback function should probably reinitialize the whole stack.
		 */
		EventReconnect,

		/*!
		 * \brief The maximum buffer capactiy has passed.
		 *
		 * The callback may reset the stack to prevent excessive memory usage.
		 * Memory allocation will just continue. If no callback function is set (the
		 * default), \c abort() is called when this event happens.
		 */
		EventEncodeBufferOverflow,

		/*!
		 * \brief #RetransmitCallbackThreshold has been reached on the current message.
		 *
		 * This is an indicator that the connection has been lost.
		 */
		EventRetransmit,
	};

#  if STORED_cplusplus < 201103L
	/*!
	 * \brief Callback type for #setEventCallback(EventCallbackArg*,void*).
	 */
	typedef void(EventCallbackArg)(ArqLayer&, Event, void*);

	/*!
	 * \brief Set event callback.
	 */
	void setEventCallback(EventCallbackArg* cb = nullptr, void* arg = nullptr)
	{
		m_cb = cb;
		m_cbArg = arg;
	}
#  else
	/*!
	 * \brief Callback type for #setEventCallback(EventCallbackArg*,void*).
	 */
	using EventCallbackArg = void(ArqLayer&, Event, void*);

	/*!
	 * \brief Set event callback.
	 */
	void setEventCallback(EventCallbackArg* cb = nullptr, void* arg = nullptr)
	{
		if(cb)
			setEventCallback([arg, cb](ArqLayer& l, Event e) { cb(l, e, arg); });
		else
			m_cb = nullptr;
	}

	/*!
	 * \brief Callback type for #setEventCallback(F&&).
	 */
	using EventCallback = void(ArqLayer&, Event);

	/*!
	 * \brief Set event callback.
	 */
	template <typename F>
	SFINAE_IS_FUNCTION(F, EventCallback, void)
	setEventCallback(F&& cb)
	{
		m_cb = std::forward<F>(cb);
	}
#  endif

	bool didTransmit() const;
	void resetDidTransmit();
	size_t retransmits() const;
	bool waitingForAck() const;

	void shrink_to_fit();

protected:
	virtual void event(Event e);
	bool transmit();

	enum EncodeState { EncodeStateIdle, EncodeStateEncoding };

	static uint8_t nextSeq(uint8_t seq);

	void popEncodeQueue();
	void pushEncodeQueue(void const* buffer, size_t len, bool back = true);
	String::type& pushEncodeQueueRaw(bool back = true);

private:
#  if STORED_cplusplus < 201103L
	EventCallbackArg* m_cb;
	void* m_cbArg;
#  else
	Callable<EventCallback>::type m_cb;
#  endif

	size_t m_maxEncodeBuffer;
	Deque<String::type*>::type m_encodeQueue;
	Deque<String::type*>::type m_spare;
	size_t m_encodeQueueSize;
	EncodeState m_encodeState;
	bool m_pauseTransmit;
	bool m_didTransmit;
	uint8_t m_retransmits;

	uint8_t m_sendSeq;
	uint8_t m_recvSeq;
};

/*!
 * \brief A layer that performs Automatic Repeat Request operations on messages for
 * #stored::Debugger.
 *
 * Only apply this layer on #stored::Debugger, as it assumes a REQ/REP
 * mechanism. For a general purpose ARQ, use #stored::ArqLayer.
 *
 * This layer allows messages that are lost, to be retransmitted on both
 * the request and response side. The implementation assumes that lost
 * message is possible, but rare. It optimizes on the normal case that
 * message arrive.  Retransmits may be relatively expensive.
 *
 * Messages must be either lost or arrive correctly. Make sure to do
 * checksumming in the layer below.  Moreover, you might want the
 * #stored::SegmentationLayer on top of this layer to make sure that
 * packets have a deterministic (small) size.
 *
 * Every message is prefixed with a sequence number in the range 0-0x7ffffff.
 * Sequence numbers are normally incremented after every message.
 * It can wrap around, but if it does, 0 should be skipped. So, the next
 * sequence number after 0x7ffffff is 1.
 *
 * This sequence number is encoded like VLQ (Variable-length quantity, see
 * https://en.wikipedia.org/wiki/Variable-length_quantity), with the exception
 * that the most significant bit of the first byte is a reset flag.
 * So, the prefix is one to four bytes.
 *
 * A request (to be received by the target) transmits every chunk with
 * increasing sequence number. When the target has received all messages of
 * the request (probably determined by a stored::SegmentationLayer on top),
 * the request is executed and the response is sent. When everything is OK,
 * the next request can be sent, continuing with the sequence number. There
 * should be no gap in these numbers.
 *
 * The client can decide to reset the sequence numbers.  To do this, send a
 * message with only the new sequence number that the client will use from
 * now on, but with the reset flag set. There is no payload.  The target
 * will respond with a message containing 0x80 (and no further payload).
 * This can be used to recover the connection if the client lost track of
 * the sequence numbers (e.g., it restarted). After this reset operation,
 * the next request shall use the sequence number used to reset + 1. The
 * response will start with sequence number 1.
 *
 * Individual messages are not ACKed, like TCP does. If the client does not
 * receive a response to its request, either the request or the response
 * has been lost.  In any case, it has to resend its full request, using
 * the same sequence numbers as used with the first attempt. The target
 * will (re)send its response.  There is no timeout specified. Use a
 * timeout value that fits the infrastructure of your device. There is no
 * limit in how often a retransmit can occur.
 *
 * The application has limited buffering. So, neither the request nor the
 * full response may be buffered for (partial) retransmission. Therefore,
 * it may be the case that when the response was lost, the request is
 * reexecuted. It is up to the buffer size as specified in DebugArqLayer's
 * constructor and stored::Debugger to determine when it is safe or
 * required to reexected upon every retransmit. For example, writes are not
 * reexecuted, as a write may have unexpected side-effects, while it is
 * safe to reexecute a read of a normal variable. When the directory is
 * requested, the response is often too long to buffer, and the response is
 * constant, so it is not buffered either and just reexecuted. Note that if
 * the buffer is too small, reading from a stream (s command) will do a
 * destructive read, but this data may be lost if the response is lost.
 * Configure the stream size and DebugArqLayer's buffer appropriate if that is
 * unacceptable for you.
 *
 * Because of this limited buffering, the response may reset the sequence
 * numbers more often. Upon retransmission of the same data, the same
 * sequence numbers are used, just like the retransmission of the request.
 * However, if the data may have been changed, as the response was not
 * buffered and the request was reexecuted, the reset flag is set of the
 * first response message, while it has a new sequence number. The client
 * should accept this new sequence number and discard all previously
 * collected response messages.
 *
 * Within one request or response, the same sequence number should be used
 * twice; even if the request or response is very long. Worst-case,
 * when there is only one payload byte per message, this limits the
 * request and response to 128 MB.  As the payload is allowed to be
 * of any size, this should not be a real limitation in practice.
 *
 * This protocol is verified by the Promela model in tests/DebugArqLayer.pml.
 */
class DebugArqLayer : public ProtocolLayer {
	STORED_CLASS_NOCOPY(DebugArqLayer)
public:
	typedef ProtocolLayer base;

	explicit DebugArqLayer(
		size_t maxEncodeBuffer = 0, ProtocolLayer* up = nullptr,
		ProtocolLayer* down = nullptr);
	/*! \brief Dtor. */
	virtual ~DebugArqLayer() override is_default

	static uint8_t const ResetFlag = 0x80;

	virtual void decode(void* buffer, size_t len) override;
	virtual void encode(void const* buffer, size_t len, bool last = true) override;
#  ifndef DOXYGEN
	using base::encode;
#  endif

	virtual void setPurgeableResponse(bool purgeable = true) override;
	virtual size_t mtu() const override;
	virtual void reset() override;

protected:
	static uint32_t nextSeq(uint32_t seq);
	static uint32_t decodeSeq(uint8_t*& buffer, size_t& len);
	static size_t encodeSeq(uint32_t seq, void* buffer);

private:
	enum {
		DecodeStateIdle,
		DecodeStateDecoding,
		DecodeStateDecoded,
		DecodeStateRetransmit
	} m_decodeState;
	uint32_t m_decodeSeq;
	uint32_t m_decodeSeqStart;

	enum {
		EncodeStateIdle,
		EncodeStateEncoding,
		EncodeStateUnbufferedIdle,
		EncodeStateUnbufferedEncoding
	} m_encodeState;
	uint32_t m_encodeSeq;
	bool m_encodeSeqReset;

	size_t m_maxEncodeBuffer;
	Vector<String::type>::type m_encodeBuffer;
	size_t m_encodeBufferSize;
};

/*!
 * \brief A layer that adds a CRC-8 to messages.
 *
 * If the CRC does not match during decoding, it is silently dropped.
 * You probably want #stored::DebugArqLayer or #stored::ArqLayer somewhere
 * higher in the stack.
 *
 * An 8-bit CRC is used with polynomial 0xA6.  This polynomial seems to be
 * a good choice according to <i>Cyclic Redundancy Code (CRC) Polynomial
 * Selection For Embedded Networks</i> (Koopman et al., 2004).
 *
 * 8-bit is quite short, so it works only reliable on short messages.  For
 * proper two bit error detection, the message can be 256 bytes.  For three
 * bits, messages should only be up to 30 bytes.  Use an appropriate
 * stored::SegmentationLayer somewhere higher in the stack to accomplish
 * this. Consider using stored::Crc16Layer instead.
 */
class Crc8Layer : public ProtocolLayer {
	STORED_CLASS_NOCOPY(Crc8Layer)
public:
	typedef ProtocolLayer base;

	enum { polynomial = 0xa6, init = 0xff };

	explicit Crc8Layer(ProtocolLayer* up = nullptr, ProtocolLayer* down = nullptr);
	/*! \brief Dtor. */
	virtual ~Crc8Layer() override is_default

	virtual void decode(void* buffer, size_t len) override;
	virtual void encode(void const* buffer, size_t len, bool last = true) override;
#  ifndef DOXYGEN
	using base::encode;
#  endif

	virtual size_t mtu() const override;
	virtual void reset() override;

protected:
	static uint8_t compute(uint8_t input, uint8_t crc = init);

private:
	uint8_t m_crc;
};

/*!
 * \brief A layer that adds a CRC-16 to messages.
 *
 * Like #stored::Crc8Layer, but using a 0xbaad as polynomial.
 */
class Crc16Layer : public ProtocolLayer {
	STORED_CLASS_NOCOPY(Crc16Layer)
public:
	typedef ProtocolLayer base;

	enum { polynomial = 0xbaad, init = 0xffff };

	explicit Crc16Layer(ProtocolLayer* up = nullptr, ProtocolLayer* down = nullptr);
	/*! \brief Dtor. */
	virtual ~Crc16Layer() override is_default

	virtual void decode(void* buffer, size_t len) override;
	virtual void encode(void const* buffer, size_t len, bool last = true) override;
#  ifndef DOXYGEN
	using base::encode;
#  endif

	virtual size_t mtu() const override;
	virtual void reset() override;

protected:
	static uint16_t compute(uint8_t input, uint16_t crc = init);

private:
	uint16_t m_crc;
};

/*!
 * \brief Buffer partial encoding frames.
 *
 * By default, layers pass encoded data immediately to lower layers.
 * However, one might collect as much data as possible to reduce overhead
 * of the actual transport.  This layer buffers partial messages until the
 * maximum buffer capacity is reached, or the \c last flag is encountered.
 */
class BufferLayer : public ProtocolLayer {
	STORED_CLASS_NOCOPY(BufferLayer)
public:
	typedef ProtocolLayer base;

	explicit BufferLayer(
		size_t size = 0, ProtocolLayer* up = nullptr, ProtocolLayer* down = nullptr);
	/*! \brief Destructor. */
	virtual ~BufferLayer() override is_default

	virtual void encode(void const* buffer, size_t len, bool last = true) override;
#  ifndef DOXYGEN
	using base::encode;
#  endif

	virtual void reset() override;

private:
	size_t m_size;
	String::type m_buffer;
};

/*!
 * \brief Prints all messages to a \c FILE.
 *
 * Messages are printed on a line.
 * Decoded message start with &lt;, encoded messages with &gt;, partial encoded messages with *.
 *
 * Printing can be suspended and resumed by calling #enable() or #disable().
 * The default state is enabled.
 *
 * Mainly for debugging purposes.
 */
class PrintLayer : public ProtocolLayer {
	STORED_CLASS_NOCOPY(PrintLayer)
public:
	typedef ProtocolLayer base;

	explicit PrintLayer(
		FILE* f = stdout, char const* name = nullptr, ProtocolLayer* up = nullptr,
		ProtocolLayer* down = nullptr);
	virtual ~PrintLayer() override is_default

	virtual void decode(void* buffer, size_t len) override;
	virtual void encode(void const* buffer, size_t len, bool last = true) override;
#  ifndef DOXYGEN
	using base::encode;
#  endif

	void setFile(FILE* f);
	FILE* file() const;
	void enable(bool enable = true);
	void disable();
	bool enabled() const;

private:
	FILE* m_f;
	char const* m_name;
	bool m_enable;
};

/*!
 * \brief A layer that tracks if it sees communication through the stack.
 *
 * This may be used to check of long inactivity on stalled or disconnected
 * communication channels.
 */
class IdleCheckLayer : public ProtocolLayer {
	STORED_CLASS_NOCOPY(IdleCheckLayer)
public:
	typedef ProtocolLayer base;

	explicit IdleCheckLayer(ProtocolLayer* up = nullptr, ProtocolLayer* down = nullptr)
		: base(up, down)
		, m_idleUp(true)
		, m_idleDown(true)
	{}

	virtual ~IdleCheckLayer() override is_default

	virtual void decode(void* buffer, size_t len) override
	{
		if(m_idleUp)
			connected();

		m_idleUp = false;
		base::decode(buffer, len);
	}

	virtual void encode(void const* buffer, size_t len, bool last = true) override
	{
		m_idleDown = false;
		base::encode(buffer, len, last);
	}

#  ifndef DOXYGEN
	using base::encode;
#  endif

	/*!
	 * \brief Checks if both up and down the stack was idle since the last call to #setIdle().
	 */
	bool idle() const
	{
		return idleUp() && idleDown();
	}

	/*!
	 * \brief Checks if upstream was idle since the last call to #setIdle().
	 */
	bool idleUp() const
	{
		return m_idleUp;
	}

	/*!
	 * \brief Checks if downstream was idle since the last call to #setIdle().
	 */
	bool idleDown() const
	{
		return m_idleDown;
	}

	/*!
	 * \brief Resets idle flags.
	 */
	void setIdle()
	{
		m_idleUp = true;
		m_idleDown = true;
	}

private:
	bool m_idleUp;
	bool m_idleDown;
};

#  if STORED_cplusplus >= 201103L
template <typename Up, typename Down, typename Connected>
class CallbackLayer;

template <typename Up, typename Down>
static inline CallbackLayer<
	typename std::decay<Up>::type, typename std::decay<Down>::type, void (*)()>
make_callback(Up&& up, Down&& down);

template <typename Up, typename Down, typename Connected>
static inline CallbackLayer<
	typename std::decay<Up>::type, typename std::decay<Down>::type,
	typename std::decay<Connected>::type>
make_callback(Up&& up, Down&& down, Connected&& connected);

/*!
 * \brief Callback class that invokes a callback for every messages through the stack.
 *
 * \copydetails #stored::make_callback()
 */
template <typename Up, typename Down, typename Connected>
class CallbackLayer : public ProtocolLayer {
public:
	typedef ProtocolLayer base;

protected:
	template <typename U, typename D, typename C>
	CallbackLayer(U&& u, D&& d, C&& c)
		: m_up{std::forward<U>(u)}
		, m_down{std::forward<D>(d)}
		, m_connected{std::forward<C>(c)}
	{}

	template <typename U, typename D>
	friend CallbackLayer<typename std::decay<U>::type, typename std::decay<D>::type, void (*)()>
	make_callback(U&& up, D&& down);

	template <typename U, typename D, typename C>
	friend CallbackLayer<
		typename std::decay<U>::type, typename std::decay<D>::type,
		typename std::decay<C>::type>
	make_callback(U&& up, D&& down, C&& connected);

public:
	CallbackLayer(CallbackLayer&& l) noexcept
		: m_up{std::move(l.m_up)}
		, m_down{std::move(l.m_down)}
		, m_connected{std::move(l.m_connected)}
	{}

	CallbackLayer(CallbackLayer const&) = delete;

	void operator=(CallbackLayer const&) = delete;
	void operator=(CallbackLayer&&) = delete;

	virtual ~CallbackLayer() override = default;

	virtual void decode(void* buffer, size_t len) override
	{
		m_up(buffer, len);
		base::decode(buffer, len);
	}

	virtual void encode(void const* buffer, size_t len, bool last = true) override
	{
		m_down(buffer, len, last);
		base::encode(buffer, len, last);
	}

	virtual void connected() override
	{
		m_connected();
		base::connected();
	}

#    ifndef DOXYGEN
	using base::encode;
#    endif

private:
	Up m_up;
	Down m_down;
	Connected m_connected;
};

/*!
 * \brief Creates a ProtocolLayer that invokes a given callback on every messages through the layer.
 *
 * Use as follows:
 *
 * \code
 * auto cb = stored::make_callback(
 *               [&](void*, size_t){ ... },
 *               [&](void const&, size_t, bool){ ... });
 * \endcode
 *
 * The first argument (a lambda in the example above), gets the parameters as passed to \c decode().
 * The second argument get the parameters as passed to \c encode().
 */
template <typename Up, typename Down>
static inline CallbackLayer<
	typename std::decay<Up>::type, typename std::decay<Down>::type, void (*)()>
make_callback(Up&& up, Down&& down)
{
	return CallbackLayer<
		typename std::decay<Up>::type, typename std::decay<Down>::type, void (*)()>{
		std::forward<Up>(up), std::forward<Down>(down), []() {}};
}

/*!
 * \brief Creates a ProtocolLayer that invokes a given callback on every messages or connected event
 *	through the layer.
 *
 * Use as follows:
 *
 * \code
 * auto cb = stored::make_callback(
 *               [&](void*, size_t){ ... },
 *               [&](void const&, size_t, bool){ ... },
 *               [&](){ ... });
 * \endcode
 *
 * The first argument (a lambda in the example above), gets the parameters as passed to \c decode().
 * The second argument get the parameters as passed to \c encode(). The third one gets invoked upon
 * \c connected().
 */
template <typename Up, typename Down, typename Connected>
static inline CallbackLayer<
	typename std::decay<Up>::type, typename std::decay<Down>::type,
	typename std::decay<Connected>::type>
make_callback(Up&& up, Down&& down, Connected&& connected)
{
	return CallbackLayer<
		typename std::decay<Up>::type, typename std::decay<Down>::type,
		typename std::decay<Connected>::type>{
		std::forward<Up>(up), std::forward<Down>(down), std::forward<Connected>(connected)};
}
#  endif // C++11

namespace impl {
class Loopback1 final : public ProtocolLayer {
	STORED_CLASS_NOCOPY(Loopback1)
public:
	typedef ProtocolLayer base;
	enum { ExtraAlloc = 32 };

	Loopback1(ProtocolLayer& from, ProtocolLayer& to);
	/*! \brief Destructor. */
	~Loopback1() override final;

	void encode(void const* buffer, size_t len, bool last = true) override final;
	void reset() override final;
	void reserve(size_t capacity);

private:
	ProtocolLayer* m_to;
	char* m_buffer;
	size_t m_capacity;
	size_t m_len;
};
} // namespace impl

/*!
 * \brief Loopback between two protocol stacks.
 */
class Loopback {
	STORED_CLASS_NOCOPY(Loopback)
public:
	Loopback(ProtocolLayer& a, ProtocolLayer& b);
	~Loopback() is_default
	void reserve(size_t capacity);

private:
	impl::Loopback1 m_a2b;
	impl::Loopback1 m_b2a;
};

class Poller;

/*!
 * \brief A generalized layer that needs a call to \c recv() to get decodable data from somewhere
 * else.
 *
 * This includes files, sockets, etc.
 * \c recv() reads data, and passes the data upstream.
 */
class PolledLayer : public ProtocolLayer {
	STORED_CLASS_NOCOPY(PolledLayer)
public:
	typedef ProtocolLayer base;

protected:
	/*!
	 * \brief Ctor.
	 */
	explicit PolledLayer(ProtocolLayer* up = nullptr, ProtocolLayer* down = nullptr)
		: base(up, down)
		, m_lastError()
		, m_poller()
	{}

public:
	/*!
	 * \brief Dtor.
	 *
	 * Make sure to #close() the related file descriptor prior to destruction.
	 */
	virtual ~PolledLayer() override;

	/*!
	 * \brief Returns the last error of an invoked method of this class.
	 *
	 * This is required after construction or \c encode(), for example,
	 * where no error return value is possible.
	 */
	int lastError() const
	{
		return m_lastError;
	}

	/*!
	 * \brief Checks if the file descriptor is open.
	 */
	virtual bool isOpen() const
	{
		return true;
	}

	/*!
	 * \brief Try to receive and decode data.
	 * \return 0 on success, otherwise an \c errno
	 */
	virtual int recv(long timeout_us = 0) = 0;

protected:
	Poller& poller();

	/*!
	 * \brief Close the file descriptor.
	 */
	virtual void close() {}

	/*!
	 * \brief Registers an error code for later retrieval by #lastError().
	 * \return \p e
	 */
	int setLastError(int e)
	{
		return errno = m_lastError = e;
	}

private:
	int m_lastError;
	Poller* m_poller;
};

/*!
 * \brief A generalized layer that reads from and writes to a file descriptor.
 */
class PolledFileLayer : public PolledLayer {
	STORED_CLASS_NOCOPY(PolledFileLayer)
public:
	typedef PolledLayer base;

#  ifdef STORED_OS_WINDOWS
	typedef HANDLE fd_type;
#  else
	typedef int fd_type;
#  endif

protected:
	/*!
	 * \brief Ctor.
	 */
	explicit PolledFileLayer(ProtocolLayer* up = nullptr, ProtocolLayer* down = nullptr)
		: base(up, down)
	{}

public:
	/*!
	 * \brief Dtor.
	 */
	virtual ~PolledFileLayer() override;

	/*!
	 * \brief The file descriptor you may poll before calling #recv().
	 */
	virtual fd_type fd() const = 0;

protected:
	virtual int block(fd_type fd, bool forReading, long timeout_us = -1, bool suspend = false);
};

#  ifdef STORED_OS_WINDOWS
/*!
 * \brief A generalized layer that reads from and writes to a SOCKET.
 */
class PolledSocketLayer : public PolledLayer {
	STORED_CLASS_NOCOPY(PolledSocketLayer)
public:
	typedef PolledLayer base;
	typedef SOCKET fd_type;

protected:
	explicit PolledSocketLayer(ProtocolLayer* up = nullptr, ProtocolLayer* down = nullptr)
		: base(up, down)
	{}

public:
	virtual ~PolledSocketLayer() override;
	virtual fd_type fd() const = 0;

protected:
	virtual int
	block(fd_type fd, bool forReading, long timeout_us = -1, bool suspend = false) = 0;
};
#  else // !STORED_OS_WINDOWS

// Sockets are just files.
typedef PolledFileLayer PolledSocketLayer;

#  endif // !STORED_OS_WINDOWS

#  ifdef STORED_HAVE_STDIO
/*!
 * \brief A layer that reads from and writes to file descriptors.
 *
 * For POSIX, this applies to everything that is a file descriptor.  \c
 * read() and \c write() is done non-blocking, by using a #stored::Poller.
 *
 * For Windows, this can only be used for files. See
 * stored::NamedPipeLayer.  All files reads and writes use overlapped IO,
 * in combination with a #stored::Poller. The file handle must be opened
 * that way. The implementation always has an overlapped read pending, and
 * checks the corresponding event for completion. Every \c decode()
 * triggers an overlapped write. It uses a completion routine, so the
 * thread must be put in an alertable state once in a while (which the
 * #stored::Poller does when blocking).
 */
class FileLayer : public PolledFileLayer {
	STORED_CLASS_NOCOPY(FileLayer)
public:
	typedef PolledFileLayer base;
	using base::fd_type;

	enum { DefaultBufferSize = 128 };

protected:
	explicit FileLayer(ProtocolLayer* up = nullptr, ProtocolLayer* down = nullptr);

public:
	explicit FileLayer(
		int fd_r, int fd_w = -1, size_t bufferSize = DefaultBufferSize,
		ProtocolLayer* up = nullptr, ProtocolLayer* down = nullptr);
	explicit FileLayer(
		char const* name_r, char const* name_w = nullptr,
		size_t bufferSize = DefaultBufferSize, ProtocolLayer* up = nullptr,
		ProtocolLayer* down = nullptr);
#    ifdef STORED_OS_WINDOWS
	explicit FileLayer(
		HANDLE h_r, HANDLE h_w = INVALID_HANDLE_VALUE,
		size_t bufferSize = DefaultBufferSize, ProtocolLayer* up = nullptr,
		ProtocolLayer* down = nullptr);
#    endif

	virtual ~FileLayer() override;

	virtual void encode(void const* buffer, size_t len, bool last = true) override;
#    ifndef DOXYGEN
	using base::encode;
#    endif

	virtual fd_type fd() const override;
	virtual int recv(long timeout_us = 0) override;
	virtual bool isOpen() const override;

protected:
	void init(fd_type fd_r, fd_type fd_w, size_t bufferSize = DefaultBufferSize);
	fd_type fd_r() const;
	fd_type fd_w() const;
	virtual void close() override;
	void close_();

#    ifdef STORED_OS_WINDOWS
	OVERLAPPED& overlappedRead();
	OVERLAPPED& overlappedWrite();
	void resetOverlappedRead();
	void resetOverlappedWrite();
	virtual size_t available();
	virtual int startRead();
	virtual int finishWrite(bool block);

private:
	static void writeCompletionRoutine(
		DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped);
#    endif

private:
	fd_type m_fd_r;
	fd_type m_fd_w;
	Vector<char>::type m_bufferRead;

#    ifdef STORED_OS_WINDOWS
	OVERLAPPED m_overlappedRead;

	struct {
		// The order of this struct is assumed by writeCompletionRoutine().
		OVERLAPPED m_overlappedWrite;
		FileLayer* const m_this;
	};

	Vector<char>::type m_bufferWrite;
	size_t m_writeLen;
#    endif
};

#    if defined(STORED_OS_WINDOWS) || defined(DOXYGEN)
/*!
 * \brief Server end of a named pipe.
 *
 * On Windows, the client end is easier; it is just a file-like create/open/write/close API.
 */
class NamedPipeLayer : public FileLayer {
	STORED_CLASS_NOCOPY(NamedPipeLayer)
public:
	typedef FileLayer base;

	enum {
		BufferSize = 1024,
		Inbound = PIPE_ACCESS_INBOUND,
		Outbound = PIPE_ACCESS_OUTBOUND,
		Duplex = PIPE_ACCESS_DUPLEX,
	};

	NamedPipeLayer(
		char const* name, DWORD openMode = Duplex, ProtocolLayer* up = nullptr,
		ProtocolLayer* down = nullptr);
	virtual ~NamedPipeLayer() override;
	String::type const& name() const;
	int recv(long timeout_us = 0) final;
	HANDLE handle() const;
	bool isConnected() const;

	virtual void encode(void const* buffer, size_t len, bool last = true) override;
#      ifndef DOXYGEN
	using base::encode;
#      endif

	virtual void reopen();

protected:
	void close() final;
	void close_();
	int startRead() final;
	enum State {
		StateInit = 0,
		StateConnecting,
		StateConnected,
		StateError,
	};

private:
	State m_state;
	String::type m_name;
	DWORD m_openMode;
};

#    elif defined(STORED_OS_POSIX)
/*!
 * \brief Named pipe.
 *
 * This is always unidirectional.
 */
class NamedPipeLayer : public FileLayer {
	STORED_CLASS_NOCOPY(NamedPipeLayer)
public:
	typedef FileLayer base;

	enum Access {
		Inbound,
		Outbound,
		// Duplex, // Not supported.
	};

	NamedPipeLayer(
		char const* name, Access openMode, ProtocolLayer* up = nullptr,
		ProtocolLayer* down = nullptr);
	virtual ~NamedPipeLayer() override;

	String::type const& name() const;

	virtual void encode(void const* buffer, size_t len, bool last = true) override;
#      ifndef DOXYGEN
	using base::encode;
#      endif

	bool isConnected() const;
	virtual void reopen();

private:
	String::type m_name;
	Access m_openMode;
};
#    else  // !STORED_OS_WINDOWS && !STORED_OS_POSIX
// Pipes are just files.
typedef FileLayer NamedPipeLayer;
#    endif // !STORED_OS_WINDOWS && !STORED_OS_POSIX

/*!
 * \brief Server end of a pair of named pipes.
 *
 * This is like NamedPipeLayer, but it uses two unidirectional pipes
 * instead of one bidirectional.
 */
class DoublePipeLayer : public PolledFileLayer {
	STORED_CLASS_NOCOPY(DoublePipeLayer)
public:
	typedef PolledFileLayer base;

	explicit DoublePipeLayer(
		char const* name_r, char const* name_w, ProtocolLayer* up = nullptr,
		ProtocolLayer* down = nullptr);
	virtual ~DoublePipeLayer() override;

	virtual void encode(void const* buffer, size_t len, bool last = true) override;
#    ifndef DOXYGEN
	using base::encode;
#    endif

	virtual bool isOpen() const override;
	virtual int recv(long timeout_us = 0) override;
	virtual fd_type fd() const override;
	bool isConnected() const;
	virtual void reset() override;
	virtual void reopen();

protected:
	virtual void close() override;

private:
	NamedPipeLayer m_r;
	NamedPipeLayer m_w;
};

#    if defined(STORED_OS_WINDOWS) || defined(STORED_OS_POSIX)
/*!
 * \brief XSIM interaction.
 *
 * It is based on a DoublePipeLayer, having two named pipes.  In VHDL,
 * normal file read/write can be used to pass data.  This class also adds
 * keep alive messages, such that a read from the pipe never blocks (which
 * lets XSIM hang), but gets dummy data.
 *
 * This keep alive uses a third pipe. XSIM is supposed to forward all data
 * it receives to this third pipe. This way, the C++ side knows how many
 * bytes are in flight, and if XSIM would block on the next one.  Based on
 * this counter, keep alive bytes may be injected.
 */
class XsimLayer : public DoublePipeLayer {
	STORED_CLASS_NOCOPY(XsimLayer)
public:
	typedef DoublePipeLayer base;

	static char const KeepAlive = '\x16'; // SYN

	explicit XsimLayer(
		char const* pipe_prefix, ProtocolLayer* up = nullptr,
		ProtocolLayer* down = nullptr);
	virtual ~XsimLayer() override;

	virtual void encode(void const* buffer, size_t len, bool last = true) override;
#      ifndef DOXYGEN
	using base::encode;
#      endif

	virtual int recv(long timeout_us = 0) override;
	virtual void reset() override;
	void keepAlive();
	virtual void reopen() override;

	NamedPipeLayer& req();

protected:
	void decoded(size_t len);

	/*!
	 * \brief Helper to call XsimLayer::decoded() when data arrives on the req channel.
	 */
	class DecodeCallback final : public ProtocolLayer {
		STORED_CLASS_NOCOPY(DecodeCallback)
	public:
		explicit DecodeCallback(XsimLayer& xsim)
			: m_xsim(&xsim)
		{}
		~DecodeCallback() final is_default
		void decode(void* buffer, size_t len) final
		{
			STORED_UNUSED(buffer)
			m_xsim->decoded(len);
		}

	private:
		XsimLayer* m_xsim;
	};

	friend class DecodeCallback;

private:
	DecodeCallback m_callback;
	NamedPipeLayer m_req;
	size_t m_inFlight;
};
#    endif // STORED_OS_WINDOWS || STORED_OS_POSIX

#    if defined(STORED_OS_WINDOWS) || defined(DOXYGEN)
/*!
 * \brief A stdin/stdout layer.
 *
 * Although a Console HANDLE can be read/written as a normal file in
 * Windows, it does not support polling or overlapped IO. Moreover, if
 * stdin/stdout are redirected, the Console becomes a Named Pipe HANDLE.
 * This class handles both.
 *
 * For POSIX, the StdioLayer is just a FileLayer with preset stdin/stdout
 * file descriptors.
 */
class StdioLayer : public PolledFileLayer {
	STORED_CLASS_NOCOPY(StdioLayer)
public:
	typedef PolledFileLayer base;
	using base::fd_type;

	enum { DefaultBufferSize = 128 };

	explicit StdioLayer(
		size_t bufferSize = DefaultBufferSize, ProtocolLayer* up = nullptr,
		ProtocolLayer* down = nullptr);
	virtual ~StdioLayer() override;

	virtual bool isOpen() const override;
	virtual fd_type fd() const override;
	virtual int recv(long timeout_us = 0) override;

	virtual void encode(void const* buffer, size_t len, bool last = true) override;
#      ifndef DOXYGEN
	using base::encode;
#      endif

	bool isPipeIn() const;
	bool isPipeOut() const;

protected:
	fd_type fd_r() const;
	fd_type fd_w() const;
	virtual int
	block(fd_type fd, bool forReading, long timeout_us = -1, bool suspend = false) override;
	virtual void close() override;
	void close_();

private:
	fd_type m_fd_r;
	fd_type m_fd_w;
	bool m_pipe_r;
	bool m_pipe_w;
	Vector<char>::type m_bufferRead;
};

#    else // !STORED_OS_WINDOWS

/*!
 * \brief A stdin/stdout layer.
 *
 * This is just a FileLayer, with predefined stdin/stdout as file descriptors.
 */
class StdioLayer : public FileLayer {
	STORED_CLASS_NOCOPY(StdioLayer)
public:
	typedef FileLayer base;
	explicit StdioLayer(
		size_t bufferSize = DefaultBufferSize, ProtocolLayer* up = nullptr,
		ProtocolLayer* down = nullptr);
	virtual ~StdioLayer() override is_default
};

#    endif // !STORED_OS_WINDOWS

#    if defined(STORED_OS_WINDOWS) || defined(STORED_OS_POSIX)
/*!
 * \brief A serial port layer.
 *
 * This is just a FileLayer, but initializes the serial port communication
 * parameters during construction.
 */
class SerialLayer : public FileLayer {
	STORED_CLASS_NOCOPY(SerialLayer)
public:
	enum { BufferSize = 4096 };

	typedef FileLayer base;
	explicit SerialLayer(
		char const* name, unsigned long baud, bool rtscts = false, bool xonxoff = false,
		ProtocolLayer* up = nullptr, ProtocolLayer* down = nullptr);
	virtual ~SerialLayer() override is_default
	int resetAutoBaud();
};
#    endif // STORED_OS_WINDOWS || STORED_OS_POSIX
#  endif   // STORED_HAVE_STDIO

} // namespace stored
#endif // __cplusplus

#include <libstored/zmq.h>

#endif // LIBSTORED_PROTOCOL_H
