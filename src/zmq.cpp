// SPDX-FileCopyrightText: 2020-2024 Jochem Rutgers
//
// SPDX-License-Identifier: MPL-2.0

#include <libstored/poller.h>
#include <libstored/protocol.h>

#ifdef STORED_HAVE_ZMQ
#	include <zmq.h>

#	ifdef STORED_HAVE_ZTH
// Allow Zth to provide wrappers for blocking ZMQ calls.
#		include <libzth/zmq.h>
#	endif

#	include <cerrno>
#	include <cstdlib>

#	if STORED_cplusplus < 201103L
#		include <inttypes.h>
#	else
#		include <cinttypes>
#	endif

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
	, m_buffer()
	, m_bufferCapacity()
	, m_bufferSize()
	, m_socket(zmq_socket(this->context(), type))
{
	if(!m_socket)
		setLastError(errno);
}

/*!
 * \copydoc stored::ProtocolLayer::~ProtocolLayer()
 *
 * The sockets are closed (which may block).
 * If a ZeroMQ context was allocated, it is terminated here.
 */
ZmqLayer::~ZmqLayer()
{
	// NOLINTNEXTLINE(cppcoreguidelines-owning-memory,cppcoreguidelines-no-malloc)
	free(m_buffer);

	zmq_close(m_socket);

	if(m_contextCleanup) {
		zmq_ctx_term(context());
	}
}

/*!
 * \brief The ZeroMQ context.
 */
void* ZmqLayer::context() const
{
	return m_context;
}

/*!
 * \brief The ZeroMQ socket.
 *
 * Do not use this function to manipulate the socket, only for calls like \c zmq_poll().
 */
void* ZmqLayer::socket() const
{
	return m_socket;
}

/*!
 * \brief The ZeroMQ socket, which can be used for \c poll() or \c select().
 *
 * Use this socket to determine if recv() would block.
 */
ZmqLayer::fd_type ZmqLayer::fd() const
{
	fd_type socket; // NOLINT(cppcoreguidelines-init-variables)
	size_t size = sizeof(socket);

	if(zmq_getsockopt(m_socket, ZMQ_FD, &socket, &size) == -1) {
#	ifdef STORED_OS_WINDOWS
		return INVALID_SOCKET; // NOLINT(hicpp-signed-bitwise)
#	else
		return -1;
#	endif
	}

	return socket;
}

int ZmqLayer::block(fd_type fd, bool forReading, long timeout_us, bool suspend)
{
	STORED_UNUSED(fd)
	// Just use our socket.
	return block(forReading, timeout_us, suspend);
}

/*!
 * \brief Like #block(fd_type,bool,long,bool), but using the #socket() by default.
 */
int ZmqLayer::block(bool forReading, long timeout_us, bool suspend)
{
	STORED_UNUSED(suspend)

	setLastError(0);

	Poller& poller = this->poller();

	Pollable::Events events = forReading ? Pollable::PollIn : Pollable::PollOut;

	int err = 0;
	int res = 0;
	PollableZmqLayer pollable(*this, events);
	res = poller.add(pollable);
	if(res) {
		err = res;
		goto done;
	}

	while(true) {
		Poller::Result const& pres = poller.poll((int)(timeout_us / 1000L));

		if(pres.empty()) {
			err = errno;
			if(!err) {
				// Should not happen.
				err = EINVAL;
			}
			break;
		} else if((pres[0]->events
			   & (Pollable::Events)(Pollable::PollErr | Pollable::PollHup))
				  .any()) {
			// Something is wrong with the socket.
			err = EIO;
			break;
		} else if((pres[0]->events & events).any()) {
			// Got it.
			break;
		}
	}

	res = poller.remove(pollable);
	if(res)
		if(!err)
			err = res;

done:
	return setLastError(err);
}

/*!
 * \brief Try to receive a message from the ZeroMQ REP socket, and decode() it.
 * \param timeout_us if zero, this function does not block. -1 blocks indefinitely.
 */
int ZmqLayer::recv1(long timeout_us)
{
	int res = 0;
	int more = 0;

	zmq_msg_t msg;

	if(unlikely(zmq_msg_init(&msg) == -1)) {
		res = errno;
		goto error_msg;
	}

	if(unlikely(zmq_msg_recv(&msg, m_socket, ZMQ_DONTWAIT) == -1)) {
		res = errno;
		if(timeout_us == 0 || errno != EAGAIN) {
			goto error_recv;
		} else {
			// Go block first, then retry.
			res = block(true, timeout_us);
			if(res)
				goto error_recv;

			if(zmq_msg_recv(&msg, m_socket, 0) == -1) {
				// Still an error. Give up.
				res = errno;
				goto error_recv;
			}

			// Success.
		}
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
	return setLastError(0);

error_buffer:
error_recv:
	zmq_msg_close(&msg);
error_msg:
	if(!res)
		res = EAGAIN;
	return setLastError(res);
}

/*!
 * \brief Try to receive all available data from the ZeroMQ REP socket, and decode() it.
 * \param timeout_us if zero, this function does not block. -1 blocks indefinitely.
 */
int ZmqLayer::recv(long timeout_us)
{
	bool first = true;

	while(true) {
		int res = recv1(first ? timeout_us : 0);

		switch(res) {
		case 0:
			// Got more.
			break;
		case EAGAIN:
			// That's it.
			if(!first) {
				// We already got something, so don't make this an error.
				return setLastError(0);
			}
			STORED_FALLTHROUGH
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
void ZmqLayer::encode(void const* buffer, size_t len, bool last)
{
	// First try, assume we are writable.
	// NOLINTNEXTLINE(hicpp-signed-bitwise,cppcoreguidelines-pro-type-cstyle-cast)
	if(likely(zmq_send(m_socket, (void*)buffer, len, ZMQ_DONTWAIT | (last ? 0 : ZMQ_SNDMORE))
		  != -1)) {
		// Success.
		setLastError(0);
	} else if(setLastError(errno) != EAGAIN) { // NOLINT(bugprone-branch-clone)
						   // Some other error occurred.
	} else if(block(false)) {
		// Socket is not writable, and blocking failed.
	} else {
		// Socket should be writable now. This is a blocking call,
		// but it should not block anymore.
		// NOLINTNEXTLINE(cppcoreguidelines-pro-type-cstyle-cast)
		if(zmq_send(m_socket, (void*)buffer, len, last ? 0 : ZMQ_SNDMORE) != -1)
			// Some error.
			setLastError(errno);
		else
			// Success.
			setLastError(0);
	}

	base::encode(buffer, len, last);
}


//////////////////////////////
// DebugZmqLayer
//

/*!
 * \brief Constructor.
 *
 * The given \p port used for a REQ/REP socket over TCP.
 * This is the listening side, where a client like the \c libstored.gui can connect to.
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
	// flawfinder: ignore
	if(snprintf(bind, sizeof(bind) - 1, "tcp://*:%" PRIu16, (uint16_t)port) < 0)
		setLastError(ENOMEM);
	else if(zmq_bind(socket(), bind) == -1)
		setLastError(errno);
}

int DebugZmqLayer::recv(long timeout_us)
{
	int res = base::recv(timeout_us);

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
 * The given \p endpoint is used for a DEALER socket.
 * If \p listen is \c true, it binds to the endpoint, otherwise it connects to it.
 *
 * \see #stored::Synchronizer
 */
SyncZmqLayer::SyncZmqLayer(
	void* context, char const* endpoint, bool listen, ProtocolLayer* up, ProtocolLayer* down)
	: base(context, ZMQ_DEALER, up, down)
{
	if(lastError())
		return;

	int res = 0;
	if(listen)
		res = zmq_bind(socket(), endpoint);
	else
		res = zmq_connect(socket(), endpoint);

	if(res)
		setLastError(errno);
}

} // namespace stored
#else  // !STORED_HAVE_ZMQ
char dummy_char_to_make_zmq_cpp_non_empty = 0; // NOLINT
#endif // STORED_HAVE_ZMQ
