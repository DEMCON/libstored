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

#include <libstored/poller.h>

namespace stored {

Poller::Poller()
{
#ifdef STORED_POLL_ZMQ
	m_poller = zmq_poller_new();
#endif
}

Poller::~Poller() {
#ifdef STORED_POLL_ZMQ
	zmq_poller_destroy(m_poller);
	m_poller = nullptr;
#endif
}

int Poller::add(int fd, void* user_data, short events) {
	return add(Event(Event::TypeFd, fd), user_data, events);
}

int Poller::modify(int fd, short events) {
	return modify(Event(Event::TypeFd, fd), events);
}

int Poller::remove(int fd) {
	return remove(Event(Event::TypeFd, fd));
}

#ifdef STORED_OS_WINDOWS
int Poller::add(SOCKET socket, void* user_data, short events) {
	return add(Event(Event::TypeWinSock, socket), user_data, events);
}

int Poller::modify(SOCKET socket, short events) {
	return modify(Event(Event::TypeWinSock, socket), events);
}

int Poller::remove(SOCKET socket) {
	return remove(Event(Event::TypeWinSock, socket));
}

int Poller::add(HANDLE handle, void* user_data, short events) {
	return add(Event(Event::TypeHandle, handle), user_data, events);
}

int Poller::modify(HANDLE handle, short events) {
	return modify(Event(Event::TypeHandle, handle), events);
}

int Poller::remove(HANDLE handle) {
	return remove(Event(Event::TypeHandle, handle));
}
#endif

#ifdef STORED_HAVE_ZMQ
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
#endif

int Poller::add(Poller::Event const& e, void* user_data, short events) {
	for(std::deque<Event>::iterator it = m_events.begin(); it != m_events.end(); ++it)
		if(*it == e)
			return EINVAL;

	m_events.push_back(e);

	Event& b = *m_events.back();
	b.user_data = user_data;
	b.events = evens;

#ifdef STORED_POLL_ZMQ
	switch(e.type) {
	case Event::TypeFd:
		return zmq_poller_add_fd(m_poller, e.fd, e.user_data, e.events) == -1 ? zmq_errno() : 0;
	case Event::TypeZmqSock:
		return zmq_poller_add(m_poller, e.zmqsock, e.user_data, e.events) == -1 ? zmq_errno() : 0;
	case Event::TypeZmq:
		return zmq_poller_add(m_poller, e.zmq->socket(), e.user_data, e.events) == -1 ? zmq_errno() : 0;
	default:
		m_events.pop_back();
		return EINVAL;
	}
#else
	return 0;
#endif
}

int Poller::modify(Poller::Event const& e, short events) {
	for(std::deque<Event>::iterator it = m_events.begin(); it != m_events.end(); ++it)
		if(*it == e) {
			it->events = events;
#ifdef STORED_POLL_ZMQ
			switch(e.type) {
			case Event::TypeFd:
				return zmq_poller_modify_fd(m_poller, e.fd, e.events) == -1 ? zmq_errno() : 0;
			case Event::TypeZmqSock:
				return zmq_poller_modify(m_poller, e.zmqsock, e.events) == -1 ? zmq_errno() : 0;
			case Event::TypeZmq:
				return zmq_poller_modify(m_poller, e.zmq->socket(), e.events) == -1 ? zmq_errno() : 0;
			default:
				return EINVAL;
			}
		}
#endif

	return EINVAL;
}

int Poller::remove(Poller::Event const& e) {
	for(std::deque<Event>::iterator it = m_events.begin(); it != m_events.end();)
		if(*it == e) {
			m_events.erase(it);
#ifdef STORED_POLL_ZMQ
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
#endif
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

#ifdef STORED_POLL_ZTH_WAITER
std::vector<Poller::Event> const* Poller::poll(long timeout_us) {
	// TODO
	return m_lastEvents;
}
#endif

#ifdef STORED_POLL_ZTH_WFMO
std::vector<Poller::Event> const* Poller::poll(long timeout_us) {
	// TODO
	return m_lastEvents;
}
#endif

#ifdef STORED_POLL_ZMQ
std::vector<zmq_poller_event_t> const* Poller::poll(long timeout_us) {
	m_lastEvents.resize(m_events.size());

	int res = zmq_poller_wait_all(m_poller, &m_lastEvents[0], m_lastEvents.size(), (timeout_us + 999L) / 1000L);

	if(res == -1) {
		errno = zmq_errno();
		return nullptr;
	}

	stored_assert(res <= m_lastEvents.size());
	m_lastEvents.resize(res);
	return &m_lastEvents;
}
#endif

#if defined(STORED_POLL_POLL) || defined(STORED_POLL_ZTH)
std::vector<Poller::Event> const* Poller::poll(long timeout_us) {
	m_lastEventsFd.resize(m_events.size());
	size_t i = 0;
	for(std::deque<Events>::iterator it = m_events.begin(); it != m_events.end(); ++it) {
		pollfd_t& e = m_lastEventsFd[i];

		it->pollfd = &e;
		e.events = it->events;

		switch(e.type) {
		case Event::TypeFd:
			e.fd = it->fd;
#if defined(STORED_POLL_ZTH) && defined(ZTH_HAVE_ZMQ)
			e.socket = nullptr;
			break;
		case Event::TypeZmqSock:
			e.fd = -1;
			e.socket = i->zmqsock;
			break;
		case Event::TypeZmq:
			e.fd = -1;
			e.socket = i->zmq->socket();
#endif
			break;
		default:
			errno = EINVAL;
			return nullptr;
		}
	}

	int res =
#ifdef STORED_POLL_ZTH
		zth::io
#endif
		::poll(&m_lastEvents[0], m_lastEvents.size(), timeout_us > 0 ? (timeout_us + 999L) / 1000L : timeout_us);

	switch(res) {
	case -1:
		return nullptr;
	case 0:
		errno = EAGAIN;
		return nullptr;
	default:;
	}

	m_lastEvents.clear();
	for(std::deque<Events>::iterator it = m_events.begin(); it != m_events.end(); ++it) {
		if(it->pollfd && it->pollfd->revents) {
			m_lastEvents.push_back(*it);
			m_lastEvents.back().events = it->pollfd->revents;
		}
	}

	return m_lastEvents;
}
#endif

#ifdef STORED_POLL_LOOP
std::vector<Poller::Event> const* Poller::poll(long timeout_us) {
	// TODO
	return m_lastEvents;
}
#endif

} // namespace

