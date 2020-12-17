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

#    if defined(STORED_OS_WINDOWS) && !defined(STORED_COMPILER_MSVC)
#      include <winsock2.h>
#    endif

namespace stored {

	/*!
	 * \brief A protocol layer that wraps the protocol stack and implements ZeroMQ socket around it.
	 *
	 * This is a generic ZeroMQ class, for practical usages, instantiate
	 * #stored::DebugZmqLayer or #stored::SyncZmqLayer instead.
	 */
	class ZmqLayer : public PolledSocketLayer {
		CLASS_NOCOPY(ZmqLayer)
	public:
		typedef PolledSocketLayer base;
		using base::fd_type;

		ZmqLayer(void* context, int type, ProtocolLayer* up = nullptr, ProtocolLayer* down = nullptr);
		virtual ~ZmqLayer() override;

		void* context() const;
		void* socket() const;
		virtual int recv(long timeout_us = 0) override;

		virtual fd_type fd() const override;

		void encode(void const* buffer, size_t len, bool last = true) final;
#ifndef DOXYGEN
		using base::encode;
#endif

	protected:
		int block(fd_type fd, bool forReading, long timeout_us = -1, bool suspend = false) final;
		int block(bool forReading, long timeout_us = -1, bool suspend = false);
		int recv1(long timeout_us = 0);

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

	/*!
	 * \brief Constructs a protocol stack on top of a REQ/REP ZeroMQ socket, specifically for the #stored::Debugger.
	 */
	class DebugZmqLayer : public ZmqLayer {
		CLASS_NOCOPY(DebugZmqLayer)
	public:
		typedef ZmqLayer base;

		enum {
			DefaultPort = 19026,
		};

		explicit DebugZmqLayer(void* context = nullptr, int port = DefaultPort, ProtocolLayer* up = nullptr, ProtocolLayer* down = nullptr);
		/*! \brief Dtor. */
		virtual ~DebugZmqLayer() override is_default

		virtual int recv(long timeout_us = 0) override;
	};

	/*!
	 * \brief Constructs a protocol stack on top of a PAIR ZeroMQ socket, specifically for the #stored::Synchronizer.
	 */
	class SyncZmqLayer : public ZmqLayer {
		CLASS_NOCOPY(SyncZmqLayer)
	public:
		typedef ZmqLayer base;

		SyncZmqLayer(void* context, char const* endpoint, bool listen, ProtocolLayer* up = nullptr, ProtocolLayer* down = nullptr);
		/*! \brief Dtor. */
		virtual ~SyncZmqLayer() override is_default
	};

} // namespace

#  endif // STORED_HAVE_ZMQ
#endif // __cplusplus
#endif // __LIBSTORED_ZMQ_H
