#ifndef __LIBSTORED_ZMQ_H
#define __LIBSTORED_ZMQ_H
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

#ifdef __cplusplus

#  include <libstored/macros.h>

#  ifdef STORED_HAVE_ZMQ
#    include <zmq.h>
#    include <libstored/protocol.h>

#  if defined(STORED_OS_WINDOWS) && !defined(STORED_COMPILER_MSVC)
#    include <winsock2.h>
#  endif

namespace stored {

	/*!
	 * \brief A protocol layer that wraps the protocol stack and implements ZeroMQ REQ/REP around it.
	 * \ingroup libstored_protocol
	 */
	class ZmqLayer : public ProtocolLayer {
		CLASS_NOCOPY(ZmqLayer)
	public:
		typedef ProtocolLayer base;

		enum {
			DefaultPort = 19026,
		};

		ZmqLayer(void* context = nullptr, int port = DefaultPort, ProtocolLayer* up = nullptr, ProtocolLayer* down = nullptr);
		virtual ~ZmqLayer() override;

		void* context() const;
		void* socket() const;
		virtual int recv(bool block = false);

		/*! \brief The socket type. */
#ifdef STORED_OS_WINDOWS
		typedef SOCKET socket_type;
#else
		typedef int socket_type;
#endif
		socket_type fd();

		void encode(void const* buffer, size_t len, bool last = true) final;
		using base::encode;

	private:
		/*! \brief The ZeroMQ context. */
		void* m_context;
		/*! \brief Flag to indicate if we created #m_context or not. */
		bool m_contextCleanup;
		/*! \brief The REP socket. */
		void* m_socket;
		/*! \brief A buffer to save partial request before decode() can start. */
		void* m_buffer;
		/*! \brief Number of bytes in #m_buffer. */
		size_t m_bufferCapacity;
		/*! \brief Allocated size of #m_buffer. */
		size_t m_bufferSize;
	};
} // namespace

#  endif // STORED_HAVE_ZMQ
#endif // __cplusplus
#endif // __LIBSTORED_ZMQ_H
