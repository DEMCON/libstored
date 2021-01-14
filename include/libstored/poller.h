#ifndef LIBSTORED_POLLER_H
#define LIBSTORED_POLLER_H
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

#include <libstored/macros.h>
#include <libstored/util.h>
#include <libstored/zmq.h>
#include <libstored/protocol.h>

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

#if defined(STORED_POLL_POLL) || defined(STORED_POLL_ZTH) || defined(STORED_POLL_ZTH_WFMO) || defined(STORED_POLL_ZMQ)
#  include <poll.h>
#endif
#ifdef STORED_POLL_ZMQ
#  include <zmq.h>
#endif

namespace stored {

	/*!
	 * \brief A generic way of \c poll() for any blockable resource.
	 *
	 * Depending on the platform, different poll strategies are automatically chosen:
	 *
	 * - STORED_POLL_WFMO (Windows without Zth): Block using \c WaitForMultipleObjectEx() in alertable state.
	 * - STORED_POLL_ZTH_WFMO (Windows with Zth): Non-blocking call to \c WaitForMultipleObjectEx() by the zth::Waiter.
	 * - STORED_POLL_ZTH (Non-Windows with Zth, without ZMQ): rely on zth::poll(), which uses the OS's \c %poll()
	 * - STORED_POLL_ZTH_LOOP (Bare metal with Zth): call stored::poll_once() by zth::Waiter
	 * - STORED_POLL_ZMQ (Non-Windows without Zth, with ZMQ): rely on \c zmq_poll()
	 * - STORED_POLL_LOOP (Bare metal without Zth): busy-wait calling stored::poll_once()
	 * - STORED_POLL_POLL (Non-Windows without Zth and ZMQ): rely on OS's %poll()
	 *
	 * Auto-detect is bypassed when one of the macros above is defined.
	 *
	 * To use the poller, basically do this:
	 *
	 * \code
	 * Poller poller;
	 *
	 * // Pick some user_data you can recognize.
	 * poller.add(fd1, &fd1, Poller::PollIn);
	 * poller.add(fd2, &fd2, Poller::PollOut);
	 *
	 * while(true) {
	 *     auto const& res = poller.poll();
	 *
	 *     for(auto it : res) {
	 *         if(it->user_data == &fd1)
	 *             read(fd1, ...);
	 *         if(it->user_data == &fd2)
	 *             write(fd2, ...)
	 *     }
	 * }
	 * \endcode
	 */
	class Poller {
		CLASS_NOCOPY(Poller)
	public:
		typedef unsigned short events_t;

#ifdef STORED_POLL_ZMQ
		static events_t const PollIn = (events_t)ZMQ_POLLIN;
		static events_t const PollOut = (events_t)ZMQ_POLLOUT;
		static events_t const PollErr = (events_t)ZMQ_POLLERR;
		static events_t const PollHup = (events_t)POLLHUP;
#elif defined(STORED_POLL_POLL) || defined(STORED_POLL_ZTH) || defined(STORED_POLL_ZTH_WFMO)
		static events_t const PollIn = (events_t)POLLIN;
		static events_t const PollOut = (events_t)POLLOUT;
		static events_t const PollErr = (events_t)POLLERR;
#  ifdef POLLHUP
		static events_t const PollHup = (events_t)POLLHUP;
#  endif
#else
		static events_t const PollIn = 1u;
		static events_t const PollOut = 2u;
		static events_t const PollErr = 4u;
		static events_t const PollHup = 8u;
#endif

		class Event {
		public:
			enum Type {
				TypeNone,
				TypeFd,
				TypePolledFileLayer,
#ifdef STORED_OS_WINDOWS
				TypeWinSock,
				TypeHandle,
#endif
#ifdef STORED_HAVE_ZMQ
				TypeZmqSock,
				TypeZmq
#endif
			};

			// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
			Event()
				: type(TypeNone)
#ifndef STORED_POLL_ZMQ
				, user_data(), events()
#endif
#ifdef STORED_OS_WINDOWS
				, h()
#endif
			{}

			// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
			explicit Event(int /*Type*/ type, ...)
				: type((Type)type)
#ifndef STORED_POLL_ZMQ
				, user_data(), events()
#endif
#ifdef STORED_OS_WINDOWS
				, h()
#endif
			{
				va_list args;
				va_start(args, type);

				switch(type) {
				case TypeFd: fd = va_arg(args, int); break;
				case TypePolledFileLayer: polledFileLayer = va_arg(args, PolledFileLayer*); break;
#ifdef STORED_OS_WINDOWS
				case TypeWinSock: winsock = va_arg(args, SOCKET); break;
				case TypeHandle: h = handle = va_arg(args, HANDLE); break;
#endif
#ifdef STORED_HAVE_ZMQ
				case TypeZmqSock: zmqsock = va_arg(args, void*); break;
				case TypeZmq: zmq = va_arg(args, ZmqLayer*); break;
#endif
				default:;
				}

				va_end(args);
			}

			Type type;
			union {
				int fd;
				PolledFileLayer* polledFileLayer;
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
			events_t events;
#endif

#if defined(STORED_POLL_ZTH)
			zth_pollfd_t* pollfd;
#elif defined(STORED_POLL_POLL)
			struct ::pollfd* pollfd;
#elif defined(STORED_OS_WINDOWS)
			HANDLE h;
#endif

			bool operator==(Event const& e) const {
				if(type != e.type)
					return false;

				switch(type) {
				case TypeNone: return true;
				case TypeFd: return fd == e.fd;
				case TypePolledFileLayer: return polledFileLayer == e.polledFileLayer;
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
		typedef std::vector<zmq_poller_event_t> Result;
		Result const& poll(long timeout_us = -1, bool suspend = false);
#else
		typedef std::vector<Event> Result;
		Result const& poll(long timeout_us = -1, bool suspend = false);
#endif

		int add(int fd, void* user_data, events_t events);
		int modify(int fd, events_t events);
		int remove(int fd);

		int add(PolledFileLayer& layer, void* user_data, events_t events);
		int modify(PolledFileLayer& layer, events_t events);
		int remove(PolledFileLayer& layer);

#if defined(STORED_OS_WINDOWS) || defined(DOXYGEN)
		int add(SOCKET socket, void* user_data, events_t events);
		int modify(SOCKET socket, events_t events);
		int remove(SOCKET socket);

		int addh(HANDLE handle, void* user_data, events_t events);
		int modifyh(HANDLE handle, events_t events);
		int removeh(HANDLE handle);
#endif
#if defined(STORED_HAVE_ZMQ) || defined(DOXYGEN)
		int add(void* socket, void* user_data, events_t events);
		int modify(void* socket, events_t events);
		int remove(void* socket);

		int add(ZmqLayer& layer, void* user_data, events_t events);
		int modify(ZmqLayer& layer, events_t events);
		int remove(ZmqLayer& layer);
#endif
	protected:
		int add(Event const& e, void* user_data, events_t events);
		int modify(Event const& e, events_t events);
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
#ifdef STORED_OS_WINDOWS
		std::vector<HANDLE> m_lastEventsH;
#endif
	};

#if defined(STORED_POLL_LOOP) || defined(STORED_POLL_ZTH_LOOP) || defined(DOXYGEN)
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
	int poll_once(Poller::Event const& e, Poller::events_t& revents);
#endif

} // namespace
#endif // __cplusplus
#endif // LIBSTORED_POLLER_H
