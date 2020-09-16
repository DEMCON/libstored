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
 * \brief Protocol layers, to be wrapped around a #stored::Debugger instance.
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
 *   stdin/stdout stream. `client/stdio_wrapper.py` is provided to inject/extract
 *   these messages from those streams and prove a ZeroMQ interface.
 * - Application over CAN: like a `client/stdio_wrapper.py`, a CAN extractor to
 *   ZeroMQ bridge is required.
 *
 * Then, the client can be connected to the ZeroMQ interface. The following
 * clients are provided:
 *
 * - `client/ed2.ZmqClient`: a python class that allows easy access to all objects
 *   of the connected store. This is the basis of the clients below.
 * - `client/cli_client.py`: a command line tool that lets you directly enter the
 *   protocol messages as defined above.
 * - `client/gui_client.py`: a simple GUI that shows the list of objects and lets
 *   you manipulate the values. The GUI has support to send samples to `lognplot`.
 *
 * Test it using the `terminal` example, started using the
 * `client/stdio_wrapper.py`. Then connect one of the clients above to it.
 *
 * libstored suggests to use the protocol layers below, where applicable.
 * Standard layer implementations can be used to construct the following stacks (top-down):
 *
 * - Lossless UART: stored::Debugger, stored::AsciiEscapeLayer, stored::TerminalLayer
 * - Lossy UART: stored::Debugger, stored::ArqLayer, stored::Crc16Layer, stored::AsciiEscapeLayer, stored::TerminalLayer
 * - CAN: stored::Debugger, stored::SegmentationLayer, stored::ArqLayer, stored::BufferLayer, CAN driver
 * - ZMQ: stored::Debugger, stored::ZmqLayer
 *
 * ## Application layer
 *
 * See \ref libstored_debugger.
 *
 * ## Presentation layer
 *
 * For CAN/ZeroMQ: nothing required.
 *
 * ## Session layer:
 *
 * For terminal/UART/CAN: no session support, there is only one (implicit) session.
 *
 * For ZeroMQ: use REQ/REP sockets, where the application-layer request and
 * response are exactly one ZeroMQ message. All layers below are managed by ZeroMQ.
 *
 * ## Transport layer
 *
 * If the MTU is limited of the hardware, like for CAN, packet segmentation
 * should be used (see stored::SegmentationLayer).
 *
 * In case of lossy channels (UART/CAN), message sequence number, and
 * retransmits (see stored::ArqLayer) should be implemented, and CRC (see
 * stored::Crc8Layer). Default implementations are provided, but may be
 * dependent on the specific transport hardware and embedded device.
 *
 * ## Network layer
 *
 * For terminal/UART/ZeroMQ, nothing has to be done.
 *
 * For CAN: packet routing is done here.
 *
 * ## Datalink layer
 *
 * For terminal or UART: In case of binary data, escape bytes < 0x20 that
 * conflict with other procotols, as follows: the sequence `DEL` (0x7f) removes
 * the 3 MSb of the successive byte.  For example, the sequence `DEL ;` (0x7f
 * 0x3b) decodes as `ESC` (0x1b).  To encode `DEL` itself, repeat it.  See
 * stored::AsciiEscapeLayer.
 *
 * For terminal or UART: out-of-band message are captured using `ESC _` (APC) and
 * `ESC \` (ST).  A message consists of the bytes in between these sequences.
 * See stored::TerminalLayer.
 *
 * ## Physical layer
 *
 * Depends on the device.
 *
 * \ingroup libstored
 */

#ifdef __cplusplus

#include <libstored/macros.h>
#include <libstored/util.h>

#include <vector>
#include <string>

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
			ProtocolLayer* d = up.down();

			setDown(d);
			if(d)
				d->setUp(this);

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

		AsciiEscapeLayer(bool all = false, ProtocolLayer* up = nullptr, ProtocolLayer* down = nullptr);

		/*!
		 * \copydoc stored::ProtocolLayer::~ProtocolLayer()
		 */
		virtual ~AsciiEscapeLayer() override is_default

		virtual void decode(void* buffer, size_t len) override;
		virtual void encode(void const* buffer, size_t len, bool last = true) override;
		using base::encode;
		virtual size_t mtu() const override;

	protected:
		char needEscape(char c);

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

		explicit TerminalLayer(int nonDebugDecodeFd = -1, int encodeFd = -1, ProtocolLayer* up = nullptr, ProtocolLayer* down = nullptr);
		virtual ~TerminalLayer() override;

		virtual void decode(void* buffer, size_t len) override;
		virtual void encode(void const* buffer, size_t len, bool last = true) override;
		using base::encode;
		virtual size_t mtu() const override;

	protected:
		virtual void nonDebugDecode(void* buffer, size_t len);
		void encodeStart();
		void encodeEnd();

#ifdef STORED_OS_BAREMETAL
		/*!
		 * \brief Write a buffer to a file descriptor.
		 */
		virtual void writeToFd(int fd, void const* buffer, size_t len) = 0;
#else
		void writeToFd(int fd, void const* buffer, size_t len);

public:
		static void writeToFd_(int fd, void const* buffer, size_t len);
#endif

	private:
		/*! \brief The file descriptor to write non-debug decoded data to. */
		int m_nonDebugDecodeFd;
		/*! \brief The file descriptor to write injected debug frames to. */
		int m_encodeFd;

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
	 * the #stored::ArqLayer.
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

		SegmentationLayer(size_t mtu = 0, ProtocolLayer* up = nullptr, ProtocolLayer* down = nullptr);
		virtual ~SegmentationLayer() override is_default

		virtual void decode(void* buffer, size_t len) override;
		virtual void encode(void const* buffer, size_t len, bool last = true) override;
		using base::encode;

		size_t mtu() const final;
		size_t lowerMtu() const;

	private:
		size_t m_mtu;
		std::vector<char> m_decode;
		size_t m_encoded;
	};

	/*!
	 * \brief A layer that performs Automatic Repeat Request operations on messages.
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
	 * reexecuted. It is up to the buffer size as specified in ArqLayer's
	 * constructor and stored::Debugger to determine when it is safe or
	 * required to reexected upon every retransmit. For example, writes are not
	 * reexecuted, as a write may have unexpected side-effects, while it is
	 * safe to reexecute a read of a normal variable. When the directory is
	 * requested, the response is often too long to buffer, and the response is
	 * constant, so it is not buffered either and just reexecuted. Note that if
	 * the buffer is too small, reading from a stream (s command) will do a
	 * destructive read, but this data may be lost if the response is lost.
	 * Configure the stream size and ArqLayer's buffer appropriate if that is
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
	 * This protocol is verified by the Promela model in tests/ArqLayer.pml.
	 *
	 * \ingroup libstored_protocol
	 */
	class ArqLayer : public ProtocolLayer {
		CLASS_NOCOPY(ArqLayer)
	public:
		typedef ProtocolLayer base;

		ArqLayer(size_t maxEncodeBuffer = 0, ProtocolLayer* up = nullptr, ProtocolLayer* down = nullptr);
		virtual ~ArqLayer() override is_default

		static uint8_t const ResetFlag = 0x80;

		virtual void decode(void* buffer, size_t len) override;
		virtual void encode(void const* buffer, size_t len, bool last = true) override;
		using base::encode;

		virtual void setPurgeableResponse(bool purgeable = true) override;
		virtual size_t mtu() const override;

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

		size_t m_maxEncodeBuffer;
		std::vector<std::string> m_encodeBuffer;
		size_t m_encodeBufferSize;
	};

	/*!
	 * \brief A layer that adds a CRC-8 to messages.
	 *
	 * If the CRC does not match during decoding, it is silently dropped.
	 * You probably want #stored::ArqLayer somewhere higher in the stack.
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

		Crc8Layer(ProtocolLayer* up = nullptr, ProtocolLayer* down = nullptr);
		virtual ~Crc8Layer() override is_default

		virtual void decode(void* buffer, size_t len) override;
		virtual void encode(void const* buffer, size_t len, bool last = true) override;
		using base::encode;

		virtual size_t mtu() const override;

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
		CLASS_NOCOPY(Crc16Layer)
	public:
		typedef ProtocolLayer base;

		enum { polynomial = 0xbaad, init = 0xffff };

		Crc16Layer(ProtocolLayer* up = nullptr, ProtocolLayer* down = nullptr);
		virtual ~Crc16Layer() override is_default

		virtual void decode(void* buffer, size_t len) override;
		virtual void encode(void const* buffer, size_t len, bool last = true) override;
		using base::encode;

		virtual size_t mtu() const override;

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
		CLASS_NOCOPY(BufferLayer)
	public:
		typedef ProtocolLayer base;

		BufferLayer(size_t size = 0, ProtocolLayer* up = nullptr, ProtocolLayer* down = nullptr);
		virtual ~BufferLayer() override is_default

		virtual void encode(void const* buffer, size_t len, bool last = true) override;
		using base::encode;

	private:
		size_t m_size;
		std::string m_buffer;
	};

} // namespace
#endif // __cplusplus

#include <libstored/zmq.h>

#endif // __LIBSTORED_PROTOCOL_H
