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
#    include <libstored/protocol.h>

#  ifdef STORED_OS_WINDOWS
#    include <winsock2.h>
#  endif

namespace stored {

	/*!
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
		virtual int recv(bool block = false);

#ifdef STORED_OS_WINDOWS
		typedef SOCKET socket_type;
#else
		typedef int socket_type;
#endif
		socket_type fd();


#ifndef DOXYGEN // Doxygen gets confused about the const and non-const overload.
		void encode(void* buffer, size_t len, bool last = true) final;
#endif
		void encode(void const* buffer, size_t len, bool last = true) final;

	private:
		void* m_context;
		bool m_contextCleanup;
		void* m_socket;
		void* m_buffer;
		size_t m_bufferCapacity;
		size_t m_bufferSize;
	};
} // namespace

#  endif // STORED_HAVE_ZMQ
#endif // __cplusplus
#endif // __LIBSTORED_ZMQ_H
