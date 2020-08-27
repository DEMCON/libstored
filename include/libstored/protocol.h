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
 * ### Application layer
 *
 * See \ref libstored_debugger.
 *
 * ### Presentation layer
 *
 * For terminal or UART: In case of binary data, escape all bytes < 0x20 as
 * follows: the sequence `DEL` (0x7f) removes the 3 MSb of the successive byte.
 * For example, the sequence `DEL ;` (0x7f 0x3b) decodes as `ESC` (0x1b).
 * To encode `DEL` itself, repeat it.
 * See stored::AsciiEscapeLayer.
 *
 * For CAN/ZeroMQ: nothing required.
 *
 * ### Session layer:
 *
 * For terminal/UART/CAN: no session support, there is only one (implicit) session.
 *
 * For ZeroMQ: use REQ/REP sockets, where the application-layer request and
 * response are exactly one ZeroMQ message. All layers below are managed by ZeroMQ.
 *
 * ### Transport layer
 *
 * For terminal or UART: out-of-band message are captured using `ESC _` (APC) and
 * `ESC \` (ST).  A message consists of the bytes in between these sequences.
 * See stored::TerminalLayer.
 *
 * In case of lossly channels (UART/CAN), CRC, message sequence number, and
 * retransmits should be implemented.  This depends on the specific transport
 * hardware and embedded device.
 *
 * ### Network layer
 *
 * For terminal/UART/ZeroMQ, nothing has to be done.
 *
 * For CAN: packet fragmentation/reassembling/routing is done here.
 *
 * ### Datalink layer
 *
 * Depends on the device.
 *
 * ### Physical layer
 *
 * Depends on the device.
 *
 *
 * \ingroup libstored
 */

#ifdef __cplusplus

#include <libstored/macros.h>
#include <libstored/util.h>

#include <vector>

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
		 * The given buffer may be encoded in-place.
		 */
		virtual void encode(void* buffer, size_t len, bool last = true) {
			if(down())
				down()->encode(buffer, len, last);
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

		explicit AsciiEscapeLayer(ProtocolLayer* up = nullptr, ProtocolLayer* down = nullptr);

		/*!
		 * \copydoc stored::ProtocolLayer::~ProtocolLayer()
		 */
		virtual ~AsciiEscapeLayer() override is_default

		virtual void decode(void* buffer, size_t len) override;
		virtual void encode(void* buffer, size_t len, bool last = true) override;
		virtual void encode(void const* buffer, size_t len, bool last = true) override;
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
		virtual void encode(void* buffer, size_t len, bool last = true) override;
		virtual void encode(void const* buffer, size_t len, bool last = true) override;

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
} // namespace
#endif // __cplusplus

#include <libstored/zmq.h>

#endif // __LIBSTORED_PROTOCOL_H
