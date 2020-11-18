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

#ifdef STORED_HAVE_ZMQ
#  include <zmq.h>

#  ifdef STORED_HAVE_ZTH
// Allow Zth to provide wrappers for blocking ZMQ calls.
#    include <libzth/zmq.h>
#  endif

#  include <cerrno>
#  include <cstdlib>
#  include <cinttypes>

namespace stored {


//////////////////////////////
// ZmqLayer
//

/*!
 * \copydoc stored::ProtocolLayer::ProtocolLayer(ProtocolLayer*,ProtocolLayer*)
 * \param context the ZeroMQ context to use. If \c nullptr, a new context is allocated.
 * \param type the ZeroMQ socket type to create.
 */
ZmqLayer::ZmqLayer(void* context, int type, ProtocolLayer* up, ProtocolLayer* down)
	: base(up, down)
	, m_context(context ? context : zmq_ctx_new())
	, m_contextCleanup(!context)
	, m_socket()
	, m_buffer()
	, m_bufferCapacity()
	, m_bufferSize()
	, m_error()
{
	if(!(m_socket = zmq_socket(this->context(), type)))
		setLastError(errno);
}

/*!
 * \copydoc stored::ProtocolLayer::~ProtocolLayer()
 *
 * The sockets are closed (which may block).
 * If a ZeroMQ context was allocated, it is terminated here.
 */
ZmqLayer::~ZmqLayer() {
	// NOLINTNEXTLINE(cppcoreguidelines-owning-memory,cppcoreguidelines-no-malloc)
	free(m_buffer);

	zmq_close(m_socket);

	if(m_contextCleanup)
		zmq_ctx_term(context());
}

/*!
 * \brief The ZeroMQ context.
 */
void* ZmqLayer::context() const {
	return m_context;
}

/*!
 * \brief The ZeroMQ socket.
 *
 * Do not use this function to manipulate the socket, only for calls like \c zmq_poll().
 */
void* ZmqLayer::socket() const {
	return m_socket;
}

/*!
 * \brief The ZeroMQ socket, which can be used for \c poll() or \c select().
 *
 * Use this socket to determine if recv() would block.
 */
ZmqLayer::socket_type ZmqLayer::fd() {
	socket_type socket; // NOLINT(cppcoreguidelines-init-variables)
	size_t size = sizeof(socket);

	if(zmq_getsockopt(m_socket, ZMQ_FD, &socket, &size) == -1) {
		setLastError(errno);
#ifdef STORED_OS_WINDOWS
		return INVALID_SOCKET; // NOLINT(hicpp-signed-bitwise)
#else
		return -1;
#endif
	} else
		setLastError(0);

	return socket;
}

/*!
 * \brief Try to receive a message from the ZeroMQ REP socket, and decode() it.
 * \param block if \c true, this function will block on receiving data from the ZeroMQ socket
 */
int ZmqLayer::recv1(bool block) {
	int res = 0;
	int more = 0;

	zmq_msg_t msg;

	if(unlikely(zmq_msg_init(&msg) == -1)) {
		res = errno;
		goto error_msg;
	}

	if(unlikely(zmq_msg_recv(&msg, m_socket, block ? 0 : ZMQ_DONTWAIT) == -1)) {
		res = errno;
		goto error_recv;
	}

	more = zmq_msg_more(&msg);

	if(unlikely(m_bufferSize || more || zmq_msg_get(&msg, ZMQ_SHARED))) {
		// Save for later processing.
		size_t msgSize = zmq_msg_size(&msg);
		size_t newBufferSize = m_bufferSize + msgSize;

		if(newBufferSize > m_bufferCapacity) {
			// NOLINTNEXTLINE(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory)
			void* p = realloc(m_buffer, newBufferSize);
			if(!p) {
				res = ENOMEM;
				goto error_buffer;
			}

			m_buffer = p;
			m_bufferCapacity = newBufferSize;
		}

		memcpy(static_cast<char*>(m_buffer) + m_bufferSize, zmq_msg_data(&msg), msgSize);
		m_bufferSize = newBufferSize;
	}

	if(likely(!more)) {
		if(likely(!m_bufferSize)) {
			// Process immediately, without copying to the buffer.
			decode(zmq_msg_data(&msg), zmq_msg_size(&msg));
		} else {
			// Process message from buffer.
			decode(m_buffer, m_bufferSize);
			m_bufferSize = 0;
		}
	}

	zmq_msg_close(&msg);
	setLastError(0);
	return 0;

error_buffer:
error_recv:
	zmq_msg_close(&msg);
error_msg:
	if(!res)
		res = EAGAIN;
	setLastError(res);
	return res;
}

/*!
 * \brief Try to receive all available data from the ZeroMQ REP socket, and decode() it.
 * \param block if \c true, this function will block on receiving data from the ZeroMQ socket
 */
int ZmqLayer::recv(bool block) {
	bool first = true;

	while(true) {
		int res = recv1(block && first);

		switch(res) {
		case 0:
			// Got more.
			break;
		case EAGAIN:
			// That's it.
			if(!first) {
				// We already got something, so don't make this an error.
				setLastError(0);
				return 0;
			}
			// fall-through
		default:
			return res;
		}

		first = false;
	}
}

/*!
 * \copydoc stored::ProtocolLayer::encode(void const*, size_t, bool)
 * \details Encoded data is send as REP over the ZeroMQ socket.
 */
void ZmqLayer::encode(void const* buffer, size_t len, bool last) {
	if(zmq_send(m_socket, buffer, len, last ? 0 : ZMQ_SNDMORE) == -1)
		setLastError(errno);
}

/*!
 * \brief Returns the last reported error of any operation on the ZmqLayer.
 *
 * Errors are not reset when the next operation is successful. So, check this
 * function if a previous call returned an error.
 */
int ZmqLayer::lastError() const {
	return m_error;
}

/*!
 * \brief Saves the given error for #lastError().
 */
void ZmqLayer::setLastError(int error) {
	errno = m_error = error;
}


//////////////////////////////
// DebugZmqLayer
//

/*!
 * \brief Constructor.
 *
 * The given \p port used for a REQ/REP socket over TCP.
 * This is the listening side, where a client like the \c ed2.gui can connect to.
 *
 * \see #stored::Debugger
 */
DebugZmqLayer::DebugZmqLayer(void* context, int port, ProtocolLayer* up, ProtocolLayer* down)
	: base(context, ZMQ_REP, up, down)
{
	if(lastError())
		return;

	// NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays)
	char bind[32] = {};
	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg,hicpp-vararg)
	snprintf(bind, sizeof(bind), "tcp://*:%" PRIu16, (uint16_t)port);

	if(zmq_bind(socket(), bind) == -1)
		setLastError(errno);
}

int DebugZmqLayer::recv(bool block) {
	int res = base::recv(block);

	if(res == EFSM) {
		// We should not be receiving at the moment.
		// That is odd, but we can try to recover from it by sending an (error) reply.
		encode("?", 1);
	}

	return res;
}


//////////////////////////////
// SyncZmqLayer
//

/*!
 * \brief Constructor.
 *
 * The given \p endpoint is used for a PAIR socket.
 * If \p listen is \c true, it binds to the endpoint, otherwise it connects to it.
 *
 * \see #stored::Synchronizer
 */
SyncZmqLayer::SyncZmqLayer(void* context, char const* endpoint, bool listen, ProtocolLayer* up, ProtocolLayer* down)
	: base(context, ZMQ_PAIR, up, down)
{
	if(lastError())
		return;

	int res = 0;
	if(listen)
		res = zmq_bind(socket(), endpoint);
	else
		res = zmq_connect(socket(), endpoint);

	if(res)
		setLastError(res);
}

} // namespace
#else // !STORED_HAVE_ZMQ
char dummy_char_to_make_zmq_cpp_non_empty;
#endif // STORED_HAVE_ZMQ
