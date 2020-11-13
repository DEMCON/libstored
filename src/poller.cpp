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

#include <libstored/macros.h>
#include <libstored/poller.h>

#ifdef STORED_OS_WINDOWS
#  include <io.h>
#endif

namespace stored {

// NOLINTNEXTLINE(hicpp-use-equals-default)
Poller::Poller()
{
#ifdef STORED_POLL_ZMQ
	m_poller = zmq_poller_new();
#endif
}

// NOLINTNEXTLINE(hicpp-use-equals-default)
Poller::~Poller() {
#ifdef STORED_POLL_ZMQ
	zmq_poller_destroy(&m_poller);
	m_poller = nullptr;
#endif
}

#ifdef STORED_OS_WINDOWS
static int socket_to_SOCKET(void* zmq, SOCKET& win) {
	size_t len = sizeof(win);
	return zmq_getsockopt(zmq, ZMQ_FD, &win, &len) ? errno : 0;
}

static int setEvents(SOCKET s, HANDLE h, short events) {
	stored_assert(s != INVALID_SOCKET);
	stored_assert(h);

	long e = FD_CLOSE;
	if(events & Poller::PollIn)
		e |= FD_READ | FD_ACCEPT | FD_OOB;
	if(events & Poller::PollOut)
		e |= FD_WRITE;

	if(WSAEventSelect(s, h, e))
		return EIO;

	return 0;
}

static int SOCKET_to_HANDLE(SOCKET UNUSED_PAR(s), HANDLE& h) {
	stored_assert(s != INVALID_SOCKET);

	if(!h)
		if(!(h = WSACreateEvent()))
			return ENOMEM;

	return 0;
}

int Poller::add(SOCKET socket, void* user_data, short events) {
	return add(Event(Event::TypeWinSock, socket), user_data, events);
}

int Poller::modify(SOCKET socket, short events) {
	return modify(Event(Event::TypeWinSock, socket), events);
}

int Poller::remove(SOCKET socket) {
	return remove(Event(Event::TypeWinSock, socket));
}

int Poller::addh(HANDLE handle, void* user_data, short events) {
	return add(Event(Event::TypeHandle, handle), user_data, events);
}

int Poller::modifyh(HANDLE handle, short events) {
	return modify(Event(Event::TypeHandle, handle), events);
}

int Poller::removeh(HANDLE handle) {
	return remove(Event(Event::TypeHandle, handle));
}

static HANDLE dummyEventSet;
static HANDLE dummyEventReset;

static int fd_to_HANDLE(int fd, HANDLE& h) {
	intptr_t res = _get_osfhandle(fd);
	if((HANDLE)res == INVALID_HANDLE_VALUE) {
		return EINVAL;
	} else if(res == -2) {
		// stdin/stdout/stderr has no stream associated
		// Always block.
		if(!dummyEventReset)
			if(!(dummyEventReset = CreateEvent(NULL, TRUE, FALSE, NULL)))
				return EIO;

		h = dummyEventReset;
		return 0;
	} else if(fd == fileno(stdout) || fd == fileno(stderr)) {
		// Cannot WaitFor... on stdout/stderr. Always return immediately using a dummy event.
		if(!dummyEventSet)
			if(!(dummyEventSet = CreateEvent(NULL, TRUE, TRUE, NULL)))
				return EIO;

		h = dummyEventSet;
		return 0;
	} else {
		h = (HANDLE)res;
		return 0;
	}
}

int Poller::add(int fd, void* user_data, short events) {
	Event e(Event::TypeFd, fd);
	int res = fd_to_HANDLE(fd, e.h);
	return res ? res : add(e, user_data, events);
}

int Poller::modify(int fd, short events) {
	Event e(Event::TypeFd, fd);
	int res = fd_to_HANDLE(fd, e.h);
	return res ? res : modify(e, events);
}

int Poller::remove(int fd) {
	Event e(Event::TypeFd, fd);
	int res = fd_to_HANDLE(fd, e.h);
	return res ? res : remove(e);
}

int Poller::add(void* socket, void* user_data, short events) {
	return add(Event(Event::TypeZmqSock, socket), user_data, events);
}

int Poller::modify(void* socket, short events) {
	return modify(Event(Event::TypeZmqSock, socket), events);
}

int Poller::remove(void* socket) {
	return remove(Event(Event::TypeZmqSock, socket));
}

int Poller::add(ZmqLayer& layer, void* user_data, short events) {
	return add(Event(Event::TypeZmq, &layer), user_data, events);
}

int Poller::modify(ZmqLayer& layer, short events) {
	return modify(Event(Event::TypeZmq, &layer), events);
}

int Poller::remove(ZmqLayer& layer) {
	return remove(Event(Event::TypeZmq, &layer));
}

#else // !STORED_OS_WINDOWS

int Poller::add(int fd, void* user_data, short events) {
	return add(Event(Event::TypeFd, fd), user_data, events);
}

int Poller::modify(int fd, short events) {
	return modify(Event(Event::TypeFd, fd), events);
}

int Poller::remove(int fd) {
	return remove(Event(Event::TypeFd, fd));
}

#  ifdef STORED_HAVE_ZMQ
int Poller::add(void* socket, void* user_data, short events) {
	return add(Event(Event::TypeZmqSock, socket), user_data, events);
}

int Poller::modify(void* socket, short events) {
	return modify(Event(Event::TypeZmqSock, socket), events);
}

int Poller::remove(void* socket) {
	return remove(Event(Event::TypeZmqSock, socket));
}

int Poller::add(ZmqLayer& layer, void* user_data, short events) {
	return add(Event(Event::TypeZmq, &layer), user_data, events);
}

int Poller::modify(ZmqLayer& layer, short events) {
	return modify(Event(Event::TypeZmq, &layer), events);
}

int Poller::remove(ZmqLayer& layer) {
	return remove(Event(Event::TypeZmq, &layer));
}
#  endif
#endif

#ifdef STORED_POLL_ZMQ
int Poller::add(Poller::Event const& e, void* user_data, short events) {
	switch(e.type) {
	case Event::TypeFd:
		return zmq_poller_add_fd(m_poller, e.fd, user_data, events) == -1 ? zmq_errno() : 0;
	case Event::TypeZmqSock:
		return zmq_poller_add(m_poller, e.zmqsock, user_data, events) == -1 ? zmq_errno() : 0;
	case Event::TypeZmq:
		return zmq_poller_add(m_poller, e.zmq->socket(), user_data, events) == -1 ? zmq_errno() : 0;
	default:
		return EINVAL;
	}
}

int Poller::modify(Poller::Event const& e, short events) {
	switch(e.type) {
	case Event::TypeFd:
		return zmq_poller_modify_fd(m_poller, e.fd, events) == -1 ? zmq_errno() : 0;
	case Event::TypeZmqSock:
		return zmq_poller_modify(m_poller, e.zmqsock, events) == -1 ? zmq_errno() : 0;
	case Event::TypeZmq:
		return zmq_poller_modify(m_poller, e.zmq->socket(), events) == -1 ? zmq_errno() : 0;
	default:
		return EINVAL;
	}
}

int Poller::remove(Poller::Event const& e) {
	switch(e.type) {
	case Event::TypeFd:
		return zmq_poller_remove_fd(m_poller, e.fd) == -1 ? zmq_errno() : 0;
	case Event::TypeZmqSock:
		return zmq_poller_remove(m_poller, e.zmqsock) == -1 ? zmq_errno() : 0;
	case Event::TypeZmq:
		return zmq_poller_remove(m_poller, e.zmq->socket()) == -1 ? zmq_errno() : 0;
	default:
		return EINVAL;
	}
}
#else // !STORED_POLL_ZMQ

int Poller::add(Poller::Event const& e, void* user_data, short events) {
#ifdef STORED_POLL_WFMO
	if(m_events.size() == MAXIMUM_WAIT_OBJECTS)
		return EINVAL;
#endif

	for(std::deque<Event>::iterator it = m_events.begin(); it != m_events.end(); ++it)
		if(*it == e)
			return EINVAL;

	m_events.push_back(e);

	Event& b = m_events.back();
	b.user_data = user_data;
	b.events = events;

#ifdef STORED_OS_WINDOWS
	int res = 0;
	switch(b.type) {
	case Event::TypeWinSock:
		if((res = SOCKET_to_HANDLE(b.winsock, b.h)))
			return res;
		if((res = setEvents(b.winsock, b.h, events)))
			WSACloseEvent(b.h);
		return res;
#  ifdef STORED_HAVE_ZMQ
	case Event::TypeZmqSock: {
		SOCKET s = INVALID_SOCKET;
		if((res = socket_to_SOCKET(b.zmqsock, s)))
			return res;
		if((res = SOCKET_to_HANDLE(s, b.h)))
			return res;
		if((res = setEvents(s, b.h, events)))
			WSACloseEvent(b.h);
		return res;
	}
	case Event::TypeZmq: {
		SOCKET s = b.zmq->fd();
		if((res = SOCKET_to_HANDLE(s, b.h)))
			return res;
		if((res = setEvents(s, b.h, events)))
			WSACloseEvent(b.h);
		return res;
	}
#  endif
	default:;
	}
#endif

	return 0;
}

int Poller::modify(Poller::Event const& e, short events) {
	for(std::deque<Event>::iterator it = m_events.begin(); it != m_events.end(); ++it)
		if(*it == e) {
			it->events = events;
#ifdef STORED_OS_WINDOWS
			switch(it->type) {
			case Event::TypeWinSock:
				return setEvents(it->winsock, it->h, events);
#  ifdef STORED_HAVE_ZMQ
			case Event::TypeZmqSock: {
				SOCKET s = INVALID_SOCKET;
				int res = 0;
				if((res = socket_to_SOCKET(it->zmqsock, s)))
					return res;
				return setEvents(s, it->h, events);
			}
			case Event::TypeZmq: {
				return setEvents(it->zmq->fd(), it->h, events);
#  endif
			default:;
			}
#endif
			return 0;
		}
	}

	return EINVAL;
}

int Poller::remove(Poller::Event const& e) {
	for(std::deque<Event>::iterator it = m_events.begin(); it != m_events.end();)
		if(*it == e) {
			m_events.erase(it);
#ifdef SOTRED_OS_WINDOWS
			switch(it->type) {
			case Event::TypeWinSock:
#  ifdef STORED_HAVE_ZMQ
			case Event::TypeZmqSock:
			case Event::TypeZmq:
#  endif
				WSACloseEvent(it->h);
				break;
			default:;
			}
#endif
			return 0;
		} else
			++it;

	return EINVAL;
}

Poller::Event* Poller::find(Poller::Event const& e) {
	for(std::deque<Event>::iterator it = m_events.begin(); it != m_events.end(); ++it)
		if(*it == e)
			return &*it;

	return nullptr;
}
#endif // !STORED_POLL_ZMQ

#ifdef STORED_POLL_ZTH_WFMO
Poller::Result const* Poller::poll(long timeout_us) {
	// TODO
	return m_lastEvents;
}
#endif

#ifdef STORED_POLL_WFMO
Poller::Result const* Poller::poll(long timeout_us) {
	m_lastEvents.clear();
	m_lastEventsH.clear();

	for(std::deque<Event>::iterator it = m_events.begin(); it != m_events.end(); ++it)
		if(it->h)
			m_lastEventsH.push_back(it->h);

	stored_assert(m_lastEventsH.size() < MAXIMUM_WAIT_OBJECTS);

	// First try.
	DWORD res = WaitForMultipleObjects((DWORD)m_lastEventsH.size(), &m_lastEventsH[0], FALSE, timeout_us >= 0 ? (DWORD)((timeout_us + 999L) / 1000L) : INFINITE);

	HANDLE h = nullptr;
	int index = 0;
	short revents = 0;

	while(true) {
		if(res == WAIT_TIMEOUT) {
			errno = 0;
			break;
		} else if(res >= WAIT_OBJECT_0 && res < WAIT_OBJECT_0 + m_lastEventsH.size()) {
			h = m_lastEventsH[index = res - WAIT_OBJECT_0];
			revents = PollIn | PollOut;
		} else if(res >= WAIT_ABANDONED_0 && res < WAIT_ABANDONED_0 + m_lastEventsH.size()) {
			h = m_lastEventsH[index = res - WAIT_ABANDONED_0];
			revents = PollErr;
		} else {
			errno = EINVAL;
			break;
		}

		for(std::deque<Event>::iterator it = m_events.begin(); it != m_events.end(); ++it)
			if(it->h == h) {
#ifdef STORED_HAVE_ZMQ
				int zmq_events = ZMQ_POLLIN | ZMQ_POLLOUT;
				size_t len = sizeof(zmq_events);
				switch(it->type) {
				case Event::TypeZmqSock:
					if(zmq_getsockopt(it->zmqsock, ZMQ_EVENTS, &zmq_events, &len))
						revents = 0;
					revents &= (short)zmq_events;
					break;
				case Event::TypeZmq:
					if(zmq_getsockopt(it->zmq->socket(), ZMQ_EVENTS, &zmq_events, &len))
						revents = 0;
					revents &= (short)zmq_events;
					break;
				default:;
				}
#endif
				if(revents) {
					m_lastEvents.push_back(*it);
					m_lastEvents.back().events = revents;
				}
				break;
			}

		// WaitForMultipleObjects() only returns the first handle that was finished.
		// But we would like to return them all. Check the rest too.
		m_lastEventsH.erase(m_lastEventsH.begin() + index);
		if(m_lastEventsH.empty())
			break;

		res = WaitForMultipleObjects((DWORD)m_lastEventsH.size(), &m_lastEventsH[0], FALSE, 0);
	}

	return m_lastEvents.empty() ? nullptr : &m_lastEvents;
}
#endif

#ifdef STORED_POLL_ZMQ
Poller::Result const* Poller::poll(long timeout_us) {
	m_lastEvents.resize(16); // Some arbitrary number. re-poll() to get the rest.

	int res = zmq_poller_wait_all(m_poller, &m_lastEvents[0], (int)m_lastEvents.size(), timeout_us >= 0 ? (timeout_us + 999L) / 1000L : -1L);

	if(res < 0) {
		errno = zmq_errno();
		return nullptr;
	}

	stored_assert((size_t)res <= m_lastEvents.size());
	m_lastEvents.resize((size_t)res);
	return &m_lastEvents;
}
#endif

#if defined(STORED_POLL_POLL) || defined(STORED_POLL_ZTH)
Poller::Result const* Poller::poll(long timeout_us) {
	m_lastEventsFd.resize(m_events.size());
	size_t i = 0;
	for(std::deque<Event>::iterator it = m_events.begin(); it != m_events.end(); ++it, ++i) {
		pollfd& e = m_lastEventsFd[i];

		it->pollfd = &e;
		e.events = it->events;

		switch(it->type) {
		case Event::TypeFd:
			e.fd = it->fd;
#if defined(STORED_POLL_ZTH) && defined(ZTH_HAVE_ZMQ)
			e.socket = nullptr;
			break;
		case Event::TypeZmqSock:
			e.fd = -1;
			e.socket = it->zmqsock;
			break;
		case Event::TypeZmq:
			e.fd = -1;
			e.socket = it->zmq->socket();
#endif
			break;
		default:
			errno = EINVAL;
			return nullptr;
		}
	}

retry:
	int res =
#ifdef STORED_POLL_ZTH
		zth::io
#endif
		::poll(&m_lastEventsFd[0], m_lastEventsFd.size(), (int)(timeout_us > 0 ? (timeout_us + 999L) / 1000L : timeout_us));

	switch(res) {
	case -1:
		switch(errno) {
		case EINTR:
		case EAGAIN:
			goto retry;
		default:
			return nullptr;
		}
	case 0:
		errno = EAGAIN;
		return nullptr;
	default:;
	}

	m_lastEvents.clear();
	for(std::deque<Event>::iterator it = m_events.begin(); it != m_events.end(); ++it) {
		if(it->pollfd && it->pollfd->revents) {
			m_lastEvents.push_back(*it);
			m_lastEvents.back().events = it->pollfd->revents;
		}
	}

	return &m_lastEvents;
}
#endif

#if defined(STORED_POLL_LOOP) || defined(STORED_POLL_ZTH_LOOP)
bool Poller::poll_once() {
	if(!m_lastEvents.empty())
		return true;

	for(std::deque<Event>::iterator it = m_events.begin(); it != m_events.end(); ++it) {
		short revents = 0;
		if(stored::poll_once(*it, revents))
			// Error
			continue;
		if(!((unsigned short)revents & (unsigned short)it->events))
			// Not the events we requested
			continue;
		m_lastEvents.push_back(*it);
		m_lastEvents.back().events = revents;
	}

	return !m_lastEvents.empty();
}
#endif

#ifdef STORED_POLL_LOOP
Poller::Result const* Poller::poll(long timeout_us) {
	m_lastEvents.clear();

	do {
		if(poll_once())
			return &m_lastEvents;

		// We don't have a proper platform-independent way to get the current time.
		// So, only keep polling if infinite time was requested.
	} while(timeout_us < 0);

	return nullptr;
}
#endif

#ifdef STORED_POLL_ZTH_LOOP
bool Poller::zth_poll_once() {
	return poll_once() || zth::Timestamp::now() > m_timeout;
}

Poller::Result const* Poller::poll(long timeout_us) {
	m_lastEvents.clear();

	if(timeout_us == 0) {
		poll_once();
	} else if(timeout_us < 0) {
		zth::waitUntil(*this, &Poller::poll_once);
	} else {
		m_timeout = zth::Timestamp::now();
		m_timeout += zth::TimeInterval(
			(time_t)(timeout_us / 1000000l),
			(timeout_us % 1000000l) * 1000l);
		zth::waitUntil(*this, &Poller::zth_poll_once);
	}

	return m_lastEvents.empty() ? nullptr : &m_lastEvents;
}
#endif

} // namespace

