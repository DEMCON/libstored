#ifndef __LIBSTORED_PROTOCOL_H
#define __LIBSTORED_PROTOCOL_H
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

/*!
 * \defgroup libstored_protocol protocol
 * \brief Protocol layers, to be wrapped around a #stored::Debugger or #stored::Synchronizer instance.
 *
 * Every embedded device is different, so the required protocol layers are too.
 * What is common, is the Application layer, but as the Transport and Physical
 * layer are often different, the layers in between are often different too.
 * To provide a common Embedded Debugger interface, the client (e.g., GUI, CLI,
 * python scripts), we standardize on ZeroMQ REQ/REP over TCP.
 *
 * Not every device supports ZeroMQ, or even TCP. For this, several bridges are
 * required. Different configurations may be possible:
 *
 * - In case of a Linux/Windows application: embed ZeroMQ server into the
 *   application, such that the application binds to a REP socket.  A client can
 *   connect to the application directly.
 * - Terminal application with only stdin/stdout: use escape sequences in the
 *   stdin/stdout stream. `client/ed2.wrapper.stdio` is provided to inject/extract
 *   these messages from those streams and prove a ZeroMQ interface.
 * - Application over CAN: like a `client/ed2.wrapper.stdio`, a CAN extractor to
 *   ZeroMQ bridge is required.
 *
 * Then, the client can be connected to the ZeroMQ interface. The following
 * clients are provided:
 *
 * - `client/ed2.ZmqClient`: a python class that allows easy access to all objects
 *   of the connected store. This is the basis of the clients below.
 * - `client/ed2.cli`: a command line tool that lets you directly enter the
 *   protocol messages as defined above.
 * - `client/ed2.gui`: a simple GUI that shows the list of objects and lets
 *   you manipulate the values. The GUI has support to send samples to `lognplot`
 *   and to write samples to a CSV file for KST, for example.
 *
 * Test it using the `terminal` example, started using the
 * `client/ed2.wrapper.stdio`. Then connect one of the clients above to it.
 *
 * libstored suggests to use the protocol layers below, where applicable.
 * Standard layer implementations can be used to construct the following stacks (top-down):
 *
 * - Lossless UART: stored::Debugger, stored::AsciiEscapeLayer, stored::TerminalLayer, stored::StdioLayer
 * - Lossy UART: stored::Debugger, stored::DebugArqLayer, stored::Crc16Layer, stored::AsciiEscapeLayer, stored::TerminalLayer, stored::StdioLayer
 * - CAN: stored::Debugger, stored::SegmentationLayer, stored::DebugArqLayer, stored::BufferLayer, CAN driver
 * - ZMQ: stored::Debugger, stored::ZmqLayer
 *
 * \ingroup libstored
 */

#ifdef __cplusplus

#include <libstored/macros.h>
#include <libstored/util.h>

#include <vector>
#include <string>
#include <deque>
#include <cerrno>

#if STORED_cplusplus >= 201103L
#  include <functional>
#endif

#ifdef STORED_COMPILER_MSVC
#  include <io.h>
#  ifndef STDERR_FILENO
#    define STDERR_FILENO		_fileno(stderr)
#  endif
#  ifndef STDOUT_FILENO
#    define STDOUT_FILENO		_fileno(stdout)
#  endif
#  ifndef STDIN_FILENO
#    define STDIN_FILENO		_fileno(stdin)
#  endif
#elif !defined(STORED_OS_BAREMETAL)
#  include <unistd.h>
#endif

#include <cstdio>

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
	 *
	 * \ingroup libstored_protocol
	 */
	class ProtocolLayer {
		CLASS_NOCOPY(ProtocolLayer)
	public:
		/*!
		 * \brief Constructor.
		 * \param up the layer above, which receives our decoded frames
		 * \param down the layer below, which receives our encoded frames
		 */
		explicit ProtocolLayer(ProtocolLayer* up = nullptr, ProtocolLayer* down = nullptr)
			: m_up(up), m_down(down)
		{
		}

		virtual ~ProtocolLayer();

		/*!
		 * \brief Change the layer that receives our decoded frames.
		 * \param up the layer, which can be \c nullptr
		 */
		void setUp(ProtocolLayer* up) { m_up = up; }

		/*!
		 * \brief Change the layer that receives our encoded frames.
		 * \param down the layer, which can be \c nullptr
		 */
		void setDown(ProtocolLayer* down) { m_down = down; }

		/*!
		 * \brief Sets the up/down layers of this layer and the given layer, such that this layer wraps the given one.
		 *
		 * If the given layer was not the bottom of the stack, this layer
		 * injects itself in between the given layer and its wrapper.
		 */
		void wrap(ProtocolLayer& up) {
			if(!down()) {
				ProtocolLayer* d = up.down();

				setDown(d);
				if(d)
					d->setUp(this);
			}

			up.setDown(this);
			setUp(&up);
		}

		/*!
		 * \brief Sets the up/down layers of this layer and the given layer, such that this layer is stacked on (or wrapped by) the given one.
		 *
		 * If the given layer was not the top of the stack, this layer injects
		 * itself between the given layer and its stacked one.
		 */
		void stack(ProtocolLayer& down) {
			ProtocolLayer* u = down.up();

			setUp(u);
			if(u)
				u->setDown(this);

			down.setUp(this);
			setDown(&down);
		}

		/*!
		 * \brief Returns the layer above this one.
		 * \return the layer, or \c nullptr if there is none.
		 */
		ProtocolLayer* up() const { return m_up; }

		/*!
		 * \brief Returns the layer below this one.
		 * \return the layer, or \c nullptr if there is none.
		 */
		ProtocolLayer* down() const { return m_down; }

		/*!
		 * \brief Decode a frame and forward the decoded frame to the upper layer.
		 *
		 * The given buffer may be decoded in-place.
		 */
		virtual void decode(void* buffer, size_t len) {
			if(up())
				up()->decode(buffer, len);
		}

		/*!
		 * \brief Encodes the last part of the current frame.
		 */
		void encode() {
			encode(static_cast<void const*>(nullptr), 0, true);
		}

		/*!
		 * \brief Encode a (partial) frame and forward it to the lower layer.
		 *
		 * The given buffer will not be modified.
		 * A new buffer is allocated when required.
		 */
		virtual void encode(void const* buffer, size_t len, bool last = true) {
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
		virtual void setPurgeableResponse(bool purgeable = true) {
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
		virtual size_t mtu() const {
			return down() ? down()->mtu() : 0;
		}

		/*!
		 * \brief Flushes all buffered message out of the stack (top-down), if possible.
		 *
		 * Any buffered, held back, queued messages are tried to be sent
		 * immediately.  A flush is always safe; it never destroys data in the
		 * stack, it only tries to force it out.
		 *
		 * \return \c true if successful and the stack is empty, or \c false if message are still blocked
		 */
		virtual bool flush() {
			return down() ? down()->flush() : true;
		}

		/*!
		 * \brief Reset the stack (top-down), and drop all messages.
		 */
		virtual void reset() {
			if(down())
				down()->reset();
		}

	private:
		/*! \brief The layer above this one. */
		ProtocolLayer* m_up;
		/*! \brief The layer below this one. */
		ProtocolLayer* m_down;
	};

	/*!
	 * \brief Escape non-ASCII bytes.
	 * \ingroup libstored_protocol
	 */
	class AsciiEscapeLayer : public ProtocolLayer {
		CLASS_NOCOPY(AsciiEscapeLayer)
	public:
		typedef ProtocolLayer base;

		static char const Esc      = '\x7f'; // DEL
		static char const EscMask  = '\x1f'; // data bits of the next char

		explicit AsciiEscapeLayer(bool all = false, ProtocolLayer* up = nullptr, ProtocolLayer* down = nullptr);

		/*!
		 * \copydoc stored::ProtocolLayer::~ProtocolLayer()
		 */
		virtual ~AsciiEscapeLayer() override is_default

		virtual void decode(void* buffer, size_t len) override;
		virtual void encode(void const* buffer, size_t len, bool last = true) override;
		using base::encode;
		virtual size_t mtu() const override;

	protected:
		char needEscape(char c) const;

	private:
		bool const m_all;
	};

	/*!
	 * \brief Extracts and injects Embedded Debugger messages in a stream of data, such as a terminal.
	 *
	 * The frame's boundaries are marked with APC and ST C1 control characters.
	 *
	 * \ingroup libstored_protocol
	 */
	class TerminalLayer : public ProtocolLayer {
		CLASS_NOCOPY(TerminalLayer)
	public:
		typedef ProtocolLayer base;

		static char const Esc      = '\x1b'; // ESC
		static char const EscStart = '_';    // APC
		static char const EscEnd   = '\\';   // ST
		enum { MaxBuffer = 1024 };

		typedef void(NonDebugDecodeCallback)(void* buf, size_t len);

#if STORED_cplusplus >= 201103L
		template <typename F,
			SFINAE_IS_FUNCTION(F, NonDebugDecodeCallback, int) = 0>
		explicit TerminalLayer(F&& cb, ProtocolLayer* up = nullptr, ProtocolLayer* down = nullptr)
			: TerminalLayer(up, down)
		{
			m_nonDebugDecodeCallback = std::forward<F>(cb);
		}
#else
		explicit TerminalLayer(NonDebugDecodeCallback* cb, ProtocolLayer* up = nullptr, ProtocolLayer* down = nullptr);
#endif
		explicit TerminalLayer(ProtocolLayer* up = nullptr, ProtocolLayer* down = nullptr);

		virtual ~TerminalLayer() override;

		virtual void decode(void* buffer, size_t len) override;
		virtual void encode(void const* buffer, size_t len, bool last = true) override;
		using base::encode;
		virtual size_t mtu() const override;
		virtual void reset() override;

		virtual void nonDebugEncode(void* buffer, size_t len);

	protected:
		virtual void nonDebugDecode(void* buffer, size_t len);
		void encodeStart();
		void encodeEnd();

	private:
		/*! \brief The callback to write non-debug decoded data to. */
#if STORED_cplusplus >= 201103L
		std::function<NonDebugDecodeCallback> m_nonDebugDecodeCallback;
#else
		NonDebugDecodeCallback* m_nonDebugDecodeCallback;
#endif

		/*! \brief States of frame extraction. */
		enum State { StateNormal, StateNormalEsc, StateDebug, StateDebugEsc };
		/*! \brief State of frame extraction. */
		State m_decodeState;
		/*! \brief Buffer of to-be-decoded data. */
		std::vector<char> m_buffer;

		/*! \brief State of frame injection. */
		bool m_encodeState;
	};

	/*!
	 * \brief A layer that performs segmentation of the messages.
	 *
	 * Messages to be encoded are split with a maximum chunk size (MTU). At the
	 * end of each chunk, either #ContinueMarker or the #EndMarker is inserted,
	 * depending of this was the last chunk.  Incoming messages are reassembled
	 * until the #EndMarker is encountered.
	 *
	 * This layer assumes a lossless channel; all messages are received in
	 * order. If that is not the case for your transport, wrap this layer in
	 * the #stored::DebugArqLayer or #stored::ArqLayer.
	 *
	 * \ingroup libstored_protocol
	 */
	class SegmentationLayer : public ProtocolLayer {
		CLASS_NOCOPY(SegmentationLayer)
	public:
		typedef ProtocolLayer base;

		// Pick markers outside of ASCII control, as it might imply escaping.
		static char const ContinueMarker = 'C';
		static char const EndMarker = 'E';

		explicit SegmentationLayer(size_t mtu = 0, ProtocolLayer* up = nullptr, ProtocolLayer* down = nullptr);
		/*! \brief Dtor. */
		virtual ~SegmentationLayer() override is_default

		virtual void decode(void* buffer, size_t len) override;
		virtual void encode(void const* buffer, size_t len, bool last = true) override;
		using base::encode;

		size_t mtu() const final;
		size_t lowerMtu() const;
		virtual void reset() override;

	private:
		size_t m_mtu;
		std::vector<char> m_decode;
		size_t m_encoded;
	};

	/*!
	 * \brief A general purpose layer that performs Automatic Repeat Request operations on messages.
	 *
	 * This layer does not assume a specific message pattern. For
	 * #stored::Debugger, use #stored::DebugArqLayer.
	 *
	 * Every message sent has to be acknowledged. There is no window; after
	 * sending a message, an ack must be received before continuing.  The queue
	 * of messages is by default unlimited, but can be set via the constructor.
	 * If the limit is hit, the event callback is invoked.
	 *
	 * This layer prepends the message with a sequence number byte.  The MSb
	 * indicates if it is an ack, the 6 LSb are the sequence number.
	 * Sequence 0 is special; it resets the connection. It should not be used
	 * during normal operation, so the next sequence number after 63 is 1.
	 * Message that do not have a payload (so, no decode() has to be invoked
	 * upon receival), should set bit 6. This also applies to the reset
	 * message.
	 *
	 * Retransmits are triggered every time a message is queued for encoding,
	 * or when #flush() is called. There is no timeout specified.
	 *
	 * One may decide to use a #stored::SegmentationLayer higher in the
	 * protocol stack to reduce the amount of data to retransmit when a message
	 * is lost (only one segment is retransmitted, not the full message), but
	 * this may add the overhead of the sequence number and round-trip time per
	 * segment. If the #stored::SegmentationLayer is used below the ArqLayer,
	 * normal-case behavior (no packet loss) is most efficient, but the penalty
	 * of a retransmit may be higher. It is up to the infrastructure and
	 * application requirements what is best.
	 *
	 * The layer has no notion of time, or time out for retransmits and acks.
	 * The application must call #flush() (for the whole stack), or
	 * #keepAlive() at a regular interval. Every invocation of either function
	 * will do a retransmit of the head of the encode queue. If called to
	 * often, retransmits may be done before the other party had a change to
	 * respond. If called not often enough, retransmits may take long and
	 * communication may be slowed down. Either way, it is functionally
	 * correct. Determine for you application what is wise to do.
	 *
	 * \ingroup libstored_protocol
	 */
	class ArqLayer : public ProtocolLayer {
		CLASS_NOCOPY(ArqLayer)
	public:
		typedef ProtocolLayer base;

		static uint8_t const NopFlag = 0x40u; //!< \brief Flag to indicate that the payload should be ignored.
		static uint8_t const AckFlag = 0x80u; //!< \brief Ack flag.
		static uint8_t const SeqMask = 0x3fu; //!< \brief Mask for sequence number.

		enum {
			/*! \brief Number of successive retransmits before the event is emitted. */
			RetransmitCallbackThreshold = 10,
		};

		explicit ArqLayer(size_t maxEncodeBuffer = 0, ProtocolLayer* up = nullptr, ProtocolLayer* down = nullptr);
		virtual ~ArqLayer() override;

		virtual void decode(void* buffer, size_t len) override;
		virtual void encode(void const* buffer, size_t len, bool last = true) override;
		using base::encode;

		virtual size_t mtu() const override;
		virtual bool flush() override;
		virtual void reset() override;
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
			 * Memory allocation will just continue. If no callback function is set (the default),
			 * \c abort() is called when this event happens.
			 */
			EventEncodeBufferOverflow,

			/*!
			 * \brief #RetransmitCallbackThreshold has been reached on the current message.
			 *
			 * This is an indicator that the connection has been lost.
			 */
			EventRetransmit,
		};

		/*!
		 * \brief Callback type for #setEventCallback(EventCallbackArg*,void*).
		 */
		typedef void(EventCallbackArg)(ArqLayer&, Event, void*);

#if STORED_cplusplus < 201103L
		/*!
		 * \brief Set event callback.
		 */
		void setEventCallback(EventCallbackArg* cb = nullptr, void* arg = nullptr) {
			m_cb = cb;
			m_cbArg = arg;
		}
#else
		/*!
		 * \brief Set event callback.
		 */
		void setEventCallback(EventCallbackArg* cb = nullptr, void* arg = nullptr) {
			if(cb)
				setEventCallback([arg,cb](ArqLayer& l, Event e) { cb(l, e, arg); });
			else
				m_cb = nullptr;
		}

		/*!
		 * \brief Callback type for #setEventCallback(F&&).
		 */
		typedef void(EventCallback)(ArqLayer&, Event);

		/*!
		 * \brief Set event callback.
		 */
		template <typename F>
		SFINAE_IS_FUNCTION(F, EventCallback, void)
		setEventCallback(F&& cb) {
			m_cb = std::forward<F>(cb);
		}
#endif

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
		void pushEncodeQueue(void const* buffer, size_t len);
		std::string& pushEncodeQueueRaw();

	private:
#if STORED_cplusplus < 201103L
		EventCallbackArg* m_cb;
		void* m_cbArg;
#else
		std::function<EventCallback> m_cb;
#endif

		size_t const m_maxEncodeBuffer;
		std::deque<std::string*> m_encodeQueue;
		std::deque<std::string*> m_spare;
		size_t m_encodeQueueSize;
		EncodeState m_encodeState;
		bool m_didTransmit;
		uint8_t m_retransmits;

		uint8_t m_sendSeq;
		uint8_t m_recvSeq;
	};

	/*!
	 * \brief A layer that performs Automatic Repeat Request operations on messages for #stored::Debugger.
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
	 *
	 * \ingroup libstored_protocol
	 */
	class DebugArqLayer : public ProtocolLayer {
		CLASS_NOCOPY(DebugArqLayer)
	public:
		typedef ProtocolLayer base;

		explicit DebugArqLayer(size_t maxEncodeBuffer = 0, ProtocolLayer* up = nullptr, ProtocolLayer* down = nullptr);
		/*! \brief Dtor. */
		virtual ~DebugArqLayer() override is_default

		static uint8_t const ResetFlag = 0x80;

		virtual void decode(void* buffer, size_t len) override;
		virtual void encode(void const* buffer, size_t len, bool last = true) override;
		using base::encode;

		virtual void setPurgeableResponse(bool purgeable = true) override;
		virtual size_t mtu() const override;
		virtual void reset() override;

	protected:
		static uint32_t nextSeq(uint32_t seq);
		static uint32_t decodeSeq(uint8_t*& buffer, size_t& len);
		static size_t encodeSeq(uint32_t seq, void* buffer);

	private:
		enum { DecodeStateIdle, DecodeStateDecoding, DecodeStateDecoded, DecodeStateRetransmit } m_decodeState;
		uint32_t m_decodeSeq;
		uint32_t m_decodeSeqStart;

		enum { EncodeStateIdle, EncodeStateEncoding, EncodeStateUnbufferedIdle, EncodeStateUnbufferedEncoding } m_encodeState;
		uint32_t m_encodeSeq;
		bool m_encodeSeqReset;

		size_t const m_maxEncodeBuffer;
		std::vector<std::string> m_encodeBuffer;
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
	 *
	 * \ingroup libstored_protocol
	 */
	class Crc8Layer : public ProtocolLayer {
		CLASS_NOCOPY(Crc8Layer)
	public:
		typedef ProtocolLayer base;

		enum { polynomial = 0xa6, init = 0xff };

		explicit Crc8Layer(ProtocolLayer* up = nullptr, ProtocolLayer* down = nullptr);
		/*! \brief Dtor. */
		virtual ~Crc8Layer() override is_default

		virtual void decode(void* buffer, size_t len) override;
		virtual void encode(void const* buffer, size_t len, bool last = true) override;
		using base::encode;

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
	 *
	 * \ingroup libstored_protocol
	 */
	class Crc16Layer : public ProtocolLayer {
		CLASS_NOCOPY(Crc16Layer)
	public:
		typedef ProtocolLayer base;

		enum { polynomial = 0xbaad, init = 0xffff };

		explicit Crc16Layer(ProtocolLayer* up = nullptr, ProtocolLayer* down = nullptr);
		/*! \brief Dtor. */
		virtual ~Crc16Layer() override is_default

		virtual void decode(void* buffer, size_t len) override;
		virtual void encode(void const* buffer, size_t len, bool last = true) override;
		using base::encode;

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
	 *
	 * \ingroup libstored_protocol
	 */
	class BufferLayer : public ProtocolLayer {
		CLASS_NOCOPY(BufferLayer)
	public:
		typedef ProtocolLayer base;

		explicit BufferLayer(size_t size = 0, ProtocolLayer* up = nullptr, ProtocolLayer* down = nullptr);
		/*! \brief Destructor. */
		virtual ~BufferLayer() override is_default

		virtual void encode(void const* buffer, size_t len, bool last = true) override;
		using base::encode;

		virtual void reset() override;

	private:
		size_t const m_size;
		std::string m_buffer;
	};

	/*!
	 * \brief Prints all messages to a \c FILE.
	 *
	 * Messages are printed on a line.
	 * Decoded message start with &lt;, encoded messages with &gt;, partial encoded messages with *.
	 *
	 * Mainly for debugging purposes.
	 *
	 * \ingroup libstored_protocol
	 */
	class PrintLayer : public ProtocolLayer {
		CLASS_NOCOPY(PrintLayer)
	public:
		typedef ProtocolLayer base;

		explicit PrintLayer(FILE* f = stdout, char const* name = nullptr, ProtocolLayer* up = nullptr, ProtocolLayer* down = nullptr);
		virtual ~PrintLayer() override is_default

		virtual void decode(void* buffer, size_t len) override;
		virtual void encode(void const* buffer, size_t len, bool last = true) override;
		using base::encode;

		void setFile(FILE* f);

	private:
		FILE* m_f;
		char const* const m_name;
	};

	namespace impl {
		class Loopback1 : public ProtocolLayer {
			CLASS_NOCOPY(Loopback1)
		public:
			typedef ProtocolLayer base;
			enum { ExtraAlloc = 32 };

			Loopback1(ProtocolLayer& from, ProtocolLayer& to);
			/*! \brief Destructor. */
			~Loopback1() override final;

			void encode(void const* buffer, size_t len, bool last = true) override final;
			void reset() override final;
		private:
			ProtocolLayer& m_to;
			char* m_buffer;
			size_t m_capacity;
			size_t m_len;
		};
	}

	/*!
	 * \brief Loopback between two protocol stacks.
	 *
	 * \ingroup libstored_protocol
	 */
	class Loopback {
		CLASS_NOCOPY(Loopback)
	public:
		Loopback(ProtocolLayer& a, ProtocolLayer& b);
		~Loopback() is_default
	private:
		impl::Loopback1 m_a2b;
		impl::Loopback1 m_b2a;
	};

	class Poller;

	/*!
	 * \brief A generalized layer that needs a call to \c recv() to get decodable data from somewhere else.
	 *
	 * This includes files, sockets, etc.
	 * \c recv() reads data, and passes the data upstream.
	 */
	class PolledLayer : public ProtocolLayer {
		CLASS_NOCOPY(PolledLayer)
	public:
		typedef ProtocolLayer base;

	protected:
		explicit PolledLayer(ProtocolLayer* up = nullptr, ProtocolLayer* down = nullptr)
			: base(up, down)
			, m_lastError()
			, m_poller()
		{}

	public:
		virtual ~PolledLayer() override;

		int lastError() const { return m_lastError; }

		virtual bool isOpen() const { return true; }

		virtual int recv(bool block = false) = 0;

	protected:
		Poller& poller();

		virtual void close() {}

		int setLastError(int e) {
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
		CLASS_NOCOPY(PolledFileLayer)
	public:
		typedef PolledLayer base;

#ifdef STORED_OS_WINDOWS
		typedef HANDLE fd_type;
#else
		typedef int fd_type;
#endif

	protected:
		explicit PolledFileLayer(ProtocolLayer* up = nullptr, ProtocolLayer* down = nullptr)
			: base(up, down)
		{}

	public:
		virtual ~PolledFileLayer() override;
		virtual fd_type fd() const = 0;

	protected:
		virtual int block(fd_type fd, bool forReading, bool suspend = false);
	};

#ifdef STORED_OS_WINDOWS
	/*!
	 * \brief A generalized layer that reads from and writes to a SOCKET.
	 */
	class PolledSocketLayer : public PolledLayer {
		CLASS_NOCOPY(PolledFileLayer)
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
		virtual int block(fd_type fd, bool forReading, bool suspend = false) = 0;
	};
#else // !STORED_OS_WINDOWS
	typedef PolledFileLayer PolledSocketLayer;
#endif // !STORED_OS_WINDOWS

	/*!
	 * \brief A layer that reads from and writes to file descriptors.
	 *
	 * For POSIX, this applies to everything that is a file descriptor.
	 * For Windows, this can only be used for files. See stored::NamedPipeLayer.
	 *
	 * \ingroup libstored_protocol
	 */
	class FileLayer : public PolledFileLayer {
		CLASS_NOCOPY(FileLayer)
	public:
		typedef PolledFileLayer base;
		using base::fd_type;

		enum { BufferSize = 128 };

protected:
		explicit FileLayer(ProtocolLayer* up = nullptr, ProtocolLayer* down = nullptr);
public:
		explicit FileLayer(int fd_r, int fd_w = -1, ProtocolLayer* up = nullptr, ProtocolLayer* down = nullptr);
		explicit FileLayer(char const* name_r, char const* name_w = nullptr, ProtocolLayer* up = nullptr, ProtocolLayer* down = nullptr);
#ifdef STORED_OS_WINDOWS
		explicit FileLayer(HANDLE h_r, HANDLE h_w = INVALID_HANDLE_VALUE, ProtocolLayer* up = nullptr, ProtocolLayer* down = nullptr);
#endif

		virtual ~FileLayer() override;

		virtual void encode(void const* buffer, size_t len, bool last = true) override;
		using base::encode;

		virtual fd_type fd() const override;
		virtual int recv(bool block = false) override;
		virtual bool isOpen() const override;

	protected:
		void init(fd_type fd_r, fd_type fd_w);
		fd_type fd_r() const;
		fd_type fd_w() const;
		virtual void close() override;
		void close_();

#ifdef STORED_OS_WINDOWS
		OVERLAPPED& overlappedRead();
		OVERLAPPED& overlappedWrite();
		void resetOverlappedRead();
		void resetOverlappedWrite();
		virtual size_t available();
		virtual int startRead();
		virtual int finishWrite(bool block);

	private:
		static void writeCompletionRoutine(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped);
#endif

	private:
		fd_type m_fd_r;
		fd_type m_fd_w;
		std::vector<char> m_bufferRead;

#ifdef STORED_OS_WINDOWS
		OVERLAPPED m_overlappedRead;

		struct {
			// The order of this struct is assumed by writeCompletionRoutine().
			OVERLAPPED m_overlappedWrite;
			FileLayer* const m_this;
		};

		std::vector<char> m_bufferWrite;
		size_t m_writeLen;
#endif
	};

#if defined(STORED_OS_WINDOWS) || defined(DOXYGEN)
	/*!
	 * \brief Server end of a named pipe.
	 *
	 * The client end is easier; it is just a file-like create/open/write/close API.
	 *
	 * \ingroup libstored_protocol
	 */
	class NamedPipeLayer : public FileLayer {
		CLASS_NOCOPY(NamedPipeLayer)
	public:
		typedef FileLayer base;

		enum { BufferSize = 1024 };

		NamedPipeLayer(char const* name, ProtocolLayer* up = nullptr, ProtocolLayer* down = nullptr);
		virtual ~NamedPipeLayer() override;
		std::string const& name() const;
		int recv(bool block = false) final;
		HANDLE handle() const;

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
		std::string m_name;
	};

	/*!
	 * \brief A stdin/stdout layer.
	 *
	 * \ingroup libstored_protocol
	 */
	class StdioLayer : public PolledFileLayer {
		CLASS_NOCOPY(StdioLayer)
	public:
		typedef PolledFileLayer base;
		using base::fd_type;

		enum { BufferSize = 128 };

		explicit StdioLayer(ProtocolLayer* up = nullptr, ProtocolLayer* down = nullptr);
		virtual ~StdioLayer() override;

		virtual bool isOpen() const override;
		virtual fd_type fd() const override;
		virtual int recv(bool block = false) override;

		virtual void encode(void const* buffer, size_t len, bool last = true) override;
		using base::encode;

		bool isPipeIn() const;
		bool isPipeOut() const;
	protected:
		fd_type fd_r() const;
		fd_type fd_w() const;
		virtual int block(fd_type fd, bool forReading, bool suspend = false) override;
		virtual void close() override;
		void close_();
	private:
		fd_type m_fd_r;
		fd_type m_fd_w;
		bool m_pipe_r;
		bool m_pipe_w;
		std::vector<char> m_bufferRead;
	};

#else // !STORED_OS_WINDOWS

	/*!
	 * \brief A stdin/stdout layer.
	 *
	 * This is just a FileLayer, with predefined stdin/stdout as file descriptors.
	 *
	 * \ingroup libstored_protocol
	 */
	class StdioLayer : public FileLayer {
		CLASS_NOCOPY(StdioLayer)
	public:
		typedef FileLayer base;
		explicit StdioLayer(ProtocolLayer* up = nullptr, ProtocolLayer* down = nullptr);
		virtual ~StdioLayer() override is_default
	};

#endif // !STORED_OS_WINDOWS

} // namespace
#endif // __cplusplus

#include <libstored/zmq.h>

#endif // __LIBSTORED_PROTOCOL_H
