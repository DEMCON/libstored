#ifndef __LIBSTORED_POLLER_H
#define __LIBSTORED_POLLER_H
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

#define ZMQ_BUILD_DRAFT_API

#include <libstored/macros.h>
#include <libstored/util.h>
#include <libstored/zmq.h>

#include <cstdarg>
#include <vector>
#include <deque>

#ifdef STORED_HAVE_ZMQ
#  include <zmq.h>
#endif

#ifdef STORED_HAVE_ZTH
#  include <zth>
#endif

#ifdef STORED_OS_WINDOWS
#  include <winsock2.h>
#endif

#if !defined(STORED_POLL_ZTH_WFMO) && \
    !defined(STORED_POLL_WFMO) && \
    !defined(STORED_POLL_ZTH) && \
    !defined(STORED_POLL_ZTH_LOOP) && \
    !defined(STORED_POLL_ZMQ) && \
    !defined(STORED_POLL_LOOP) && \
    !defined(STORED_POLL_POLL)
// Do auto-detect

#  ifdef STORED_OS_WINDOWS
#    ifdef STORED_HAVE_ZTH
// We cannot use Zth's poll, as it does not support Windows very well.
// But we have to use Zth's waiter and polling for events instead using WFMO.
#      define STORED_POLL_ZTH_WFMO
#    else
// Use Windows's WaitForMultipleObjects.
#      define STORED_POLL_WFMO
#    endif
#  else
#    ifdef STORED_HAVE_ZTH
#      ifdef ZTH_HAVE_POLLER
// Use Zth's poll to do a fiber-aware poll.
#        define STORED_POLL_ZTH
#      else
// There is no poller. Bare metal?
#        define STORED_POLL_ZTH_LOOP
#      endif
#    elif defined(STORED_HAVE_ZMQ)
// Use libzmq's poll, as it might handle poll on ZMQ sockets more efficiently.
#      define STORED_POLL_ZMQ
#    elif defined(STORED_OS_GENERIC) || defined(STORED_OS_BAREMETAL)
// There is no poll() available.
#      define STORED_POLL_LOOP
#    else
// Use use the OS's poll.
#      define STORED_POLL_POLL
#    endif
#  endif
#endif // auto-detect

#if defined(STORED_POLL_POLL) || defined(STORED_POLL_ZTH) || defined(STORED_POLL_ZTH_WFMO)
#  include <poll.h>
#endif
#ifdef STORED_POLL_ZMQ
#  include <zmq.h>
#endif

namespace stored {

	/*!
	 * \brief A generic way of \c poll() for any blockable resource.
	 */
	class Poller {
		CLASS_NOCOPY(Poller)
	public:
#ifdef STORED_POLL_ZMQ
		enum { PollIn = ZMQ_POLLIN, PollOut = ZMQ_POLLOUT, PollErr = ZMQ_POLLERR };
#elif defined(STORED_POLL_POLL) || defined(STORED_POLL_ZTH) || defined(STORED_POLL_ZTH_WFMO)
		enum { PollIn = POLLIN, PollOut = POLLOUT, PollErr = POLLERR };
#else
		enum { PollIn = 1, PollOut = 2, PollErr = 4 };
#endif

		class Event {
		public:
			enum Type {
				TypeNone,
				TypeFd,
#ifdef STORED_OS_WINDOWS
				TypeWinSock,
				TypeHandle,
#endif
#ifdef STORED_HAVE_ZMQ
				TypeZmqSock,
				TypeZmq
#endif
			};

			Event()
				: type(TypeNone)
#ifndef STORED_POLL_ZMQ
				, user_data(), events()
#endif
#ifdef STORED_POLL_WFMO
				, h()
#endif
			{}

			Event(Type type, ...)
				: type(type)
#ifndef STORED_POLL_ZMQ
				, user_data(), events()
#endif
#ifdef STORED_POLL_WFMO
				, h()
#endif
			{
				va_list args;
				va_start(args, type);

				switch(type) {
				case TypeFd: fd = va_arg(args, int); break;
#ifdef STORED_OS_WINDOWS
				case TypeWinSock: winsock = va_arg(args, SOCKET); break;
				case TypeHandle: handle = va_arg(args, HANDLE); break;
#endif
#ifdef STORED_HAVE_ZMQ
				case TypeZmqSock: zmqsock = va_arg(args, void*); break;
				case TypeZmq: zmq = va_arg(args, stored::ZmqLayer*); break;
#endif
				default:;
				}

				va_end(args);
			}

			Type type;
			union {
				int fd;
#ifdef STORED_OS_WINDOWS
				SOCKET winsock;
				HANDLE handle;
#endif
#ifdef STORED_HAVE_ZMQ
				void* zmqsock;
				ZmqLayer* zmq;
#endif
			};
#if !defined(STORED_POLL_ZMQ)
			void* user_data;
			short events;
#endif

#if defined(STORED_POLL_ZTH)
			zth_pollfd_t* pollfd;
#elif defined(STORED_POLL_POLL)
			struct ::pollfd* pollfd;
#elif defined(STORED_POLL_WFMO)
			HANDLE h;
#endif

			bool operator==(Event const& e) const {
				if(type != e.type)
					return false;

				switch(type) {
				case TypeNone: return true;
				case TypeFd: return fd == e.fd;
#ifdef STORED_OS_WINDOWS
				case TypeWinSock: return winsock == e.winsock;
				case TypeHandle: return handle == e.handle;
#endif
#ifdef STORED_HAVE_ZMQ
				case TypeZmqSock: return zmqsock == e.zmqsock;
				case TypeZmq: return zmq == e.zmq;
#endif
				default:
					return false;
				}
			}
		};

		Poller();
		~Poller();

#ifdef STORED_POLL_ZMQ
		typedef std::vector<struct zmq_poller_event_t> Result;
		Result const* poll(long timeout_us = -1);
#else
		typedef std::vector<Event> Result;
		Result const* poll(long timeout_us = -1);
#endif

		int add(int fd, void* user_data, short events);
		int modify(int fd, short events);
		int remove(int fd);
#ifdef STORED_OS_WINDOWS
		int add(SOCKET socket, void* user_data, short events);
		int modify(SOCKET socket, short events);
		int remove(SOCKET socket);

		int addh(HANDLE handle, void* user_data, short events);
		int modifyh(HANDLE handle, short events);
		int removeh(HANDLE handle);
#endif
#ifdef STORED_HAVE_ZMQ
		int add(void* socket, void* user_data, short events);
		int modify(void* socket, short events);
		int remove(void* socket);

		int add(ZmqLayer& layer, void* user_data, short events);
		int modify(ZmqLayer& layer, short events);
		int remove(ZmqLayer& layer);
#endif
	protected:
		int add(Event const& e, void* user_data, short events);
		int modify(Event const& e, short events);
		int remove(Event const& e);
#ifndef STORED_POLL_ZMQ
		Event* find(Event const& e);
#endif

#if defined(STORED_POLL_LOOP) || defined(STORED_POLL_ZTH_LOOP)
		bool poll_once();
#endif
#if defined(STORED_POLL_ZTH_LOOP)
		bool zth_poll_once();
	private:
		zth::Timestamp m_timeout;
#endif

	private:
#ifdef STORED_POLL_ZMQ
		void* m_poller;
		Result m_lastEvents;
#else
		std::deque<Event> m_events;
		Result m_lastEvents;
#endif
#if defined(STORED_POLL_POLL) || defined(STORED_POLL_ZTH)
		std::vector<pollfd> m_lastEventsFd;
#endif
#ifdef STORED_POLL_WFMO
		std::vector<HANDLE> m_lastEventsH;
#endif
	};

#if defined(STORED_POLL_LOOP) || defined(STORED_POLL_ZTH_LOOP)
	/*!
	 * \brief Supply a custom \c poll() implementation.
	 *
	 * This function is used when there is no way to do the actual poll with
	 * standard functions, like on bare metal. The user must supply this
	 * function.
	 *
	 * \param e the event to poll
	 * \param revents the events of the poll, which may be 0 if there were no events
	 * \return 0 on success, otherwise an errno
	 */
	int poll_once(Poller::Event const& e, short& revents);
#endif

} // namespace
#endif // __cplusplus
#endif // __LIBSTORED_POLLER_H
