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

ZmqLayer::ZmqLayer(void* context, int port, ProtocolLayer* up, ProtocolLayer* down)
	: base(up, down)
	, m_context(context ? context : zmq_ctx_new())
	, m_contextCleanup(!context)
	, m_socket()
	, m_buffer()
	, m_bufferCapacity()
	, m_bufferSize()
{
	m_socket = zmq_socket(this->context(), ZMQ_REP);

	char bind[32] = {};
	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg,hicpp-vararg)
	snprintf(bind, sizeof(bind), "tcp://*:%" PRIu16, (uint16_t)port);

	// Bind, but ignore errors. They will be reported when calling poll() later on.
	zmq_bind(m_socket, bind);
}

ZmqLayer::~ZmqLayer() {
	// NOLINTNEXTLINE(cppcoreguidelines-owning-memory,cppcoreguidelines-no-malloc)
	free(m_buffer);

	zmq_close(m_socket);

	if(m_contextCleanup)
		zmq_ctx_term(context());
}

void* ZmqLayer::context() const {
	return m_context;
}

ZmqLayer::socket_type ZmqLayer::fd() {
	socket_type socket;
	size_t size = sizeof(socket);

	if(zmq_getsockopt(m_socket, ZMQ_FD, &socket, &size) == -1) {
#ifdef STORED_OS_WINDOWS
		return INVALID_SOCKET;
#else
		return -1;
#endif
	}

	return socket;
}

int ZmqLayer::recv(bool block) {
	int res = 0;
	int more;

	zmq_msg_t msg;

	if(unlikely(zmq_msg_init(&msg) == -1)) {
		res = errno;
		goto error_msg;
	}

	if(unlikely(zmq_msg_recv(&msg, m_socket, block ? 0 : ZMQ_DONTWAIT) == -1)) {
		res = errno;
		if(res == EFSM) {
			// We should not be receiving at the moment.
			// That is odd, but we can try to recover from it by sending an (error) reply.
			encode("?", 1);
		}
		goto error_recv;
	}

	more = zmq_msg_more(&msg);

	if(unlikely(!m_bufferSize || more || zmq_msg_get(&msg, ZMQ_SHARED))) {
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
	return 0;

error_buffer:
error_recv:
	zmq_msg_close(&msg);
error_msg:
	return res ? res : EIO;
}

#ifndef DOXYGEN
void ZmqLayer::encode(void* buffer, size_t len, bool last) {
	encode((void const*)buffer, len, last);
}
#endif

void ZmqLayer::encode(void const* buffer, size_t len, bool last) {
	zmq_send(m_socket, buffer, len, last ? 0 : ZMQ_SNDMORE);
}

} // namespace
#endif // STORED_HAVE_ZMQ
