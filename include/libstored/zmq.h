#ifndef LIBSTORED_ZMQ_H
#define LIBSTORED_ZMQ_H
// SPDX-FileCopyrightText: 2020-2025 Jochem Rutgers
//
// SPDX-License-Identifier: MPL-2.0

#ifdef __cplusplus

#  include <libstored/macros.h>

#  ifdef STORED_HAVE_ZMQ
#    include <libstored/protocol.h>
#    include <zmq.h>

#    if defined(STORED_OS_WINDOWS) && !defined(STORED_COMPILER_MSVC)
#      include <winsock2.h>
#    endif

namespace stored {

/*!
 * \brief A protocol layer that wraps the protocol stack and implements ZeroMQ
 *        socket around it.
 *
 * This is a generic ZeroMQ class, for practical usages, instantiate
 * #stored::DebugZmqLayer or #stored::SyncZmqLayer instead.
 */
class ZmqBaseLayer : public PolledSocketLayer {
	STORED_CLASS_NOCOPY(ZmqBaseLayer)
public:
	typedef PolledSocketLayer base;
	using base::fd_type;

	ZmqBaseLayer(
		void* context, int type, ProtocolLayer* up = nullptr,
		ProtocolLayer* down = nullptr);
	virtual ~ZmqBaseLayer() override;

	void* context() const;
	void* socket() const;
	virtual int recv(long timeout_us = 0) override;

	virtual fd_type fd() const override;

	void encode(void const* buffer, size_t len, bool last = true) final;
#    ifndef DOXYGEN
	using base::encode;
#    endif

protected:
	int block(fd_type fd, bool forReading, long timeout_us = -1, bool suspend = false) final;
	int block(bool forReading, long timeout_us = -1, bool suspend = false);
	int recv1(long timeout_us = 0);

private:
	/*! \brief The ZeroMQ context. */
	void* m_context;
	/*! \brief Flag to indicate if we created #m_context or not. */
	bool m_contextCleanup;
	/*! \brief A buffer to save partial request before decode() can start. */
	void* m_buffer;
	/*! \brief Number of bytes in #m_buffer. */
	size_t m_bufferCapacity;
	/*! \brief Allocated size of #m_buffer. */
	size_t m_bufferSize;
	/*! \brief The REP socket. */
	void* m_socket;
};

/*!
 * \brief Constructs a protocol stack on top of a REQ/REP ZeroMQ socket, specifically for the
 * #stored::Debugger.
 */
class DebugZmqLayer : public ZmqBaseLayer {
	STORED_CLASS_NOCOPY(DebugZmqLayer)
public:
	typedef ZmqBaseLayer base;

	enum STORED_ANONYMOUS {
		DefaultPort = 19026,
	};

	explicit DebugZmqLayer(
		void* context = nullptr, int port = DefaultPort, ProtocolLayer* up = nullptr,
		ProtocolLayer* down = nullptr);
	/*! \brief Dtor. */
	virtual ~DebugZmqLayer() override is_default

	virtual int recv(long timeout_us = 0) override;
};

/*!
 * \brief Generic ZeroMQ DEALER socket, for raw byte I/O via sockets.
 */
class ZmqLayer : public ZmqBaseLayer {
	STORED_CLASS_NOCOPY(ZmqLayer)
public:
	typedef ZmqBaseLayer base;

	ZmqLayer(
		void* context, char const* endpoint, bool listen, ProtocolLayer* up = nullptr,
		ProtocolLayer* down = nullptr);
	/*! \brief Dtor. */
	virtual ~ZmqLayer() override is_default
};

/*!
 * \brief Constructs a protocol stack on top of a DEALER ZeroMQ socket,
 *        specifically for the #stored::Synchronizer.
 */
typedef ZmqLayer SyncZmqLayer;

} // namespace stored

#  endif // STORED_HAVE_ZMQ
#endif	 // __cplusplus
#endif	 // LIBSTORED_ZMQ_H
