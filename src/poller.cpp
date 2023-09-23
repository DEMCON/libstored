// SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
//
// SPDX-License-Identifier: MPL-2.0

#include <libstored/poller.h>

namespace stored {

//////////////////////////////////////////////
// WfmoPoller
//

#ifdef STORED_OS_WINDOWS
static HANDLE dummyEventSet;   // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static HANDLE dummyEventReset; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

static int fd_to_HANDLE(int fd, HANDLE& h)
{
	intptr_t res = _get_osfhandle(fd);

	if((HANDLE)res == INVALID_HANDLE_VALUE) { // NOLINT
		return EBADF;
	} else if(res == -2) {
		// stdin/stdout/stderr has no stream associated
		// Always block.
		if(!dummyEventReset)
			if(!(dummyEventReset = CreateEvent(nullptr, TRUE, FALSE, nullptr)))
				return EIO;

		h = dummyEventReset;
		return 0;
	} else if(fd == _fileno(stdout) || fd == _fileno(stderr)) {
		// Cannot WaitFor... on stdout/stderr. Always return immediately using a dummy
		// event.
		if(!dummyEventSet)
			if(!(dummyEventSet = CreateEvent(nullptr, TRUE, TRUE, nullptr)))
				return EIO;

		h = dummyEventSet;
		return 0;
	} else if(fd == _fileno(stdin)) {
		// We can wait for stdin's HANDLE.
		h = (HANDLE)res; // NOLINT
		return 0;
	} else {
		// Although res is a HANDLE, it cannot be used for WaitForMultipleObjects.
		return EBADF;
	}
}

#	ifdef STORED_HAVE_ZMQ
static int socket_to_SOCKET(void* zmq, SOCKET& win)
{
	size_t len = sizeof(win);
	return zmq_getsockopt(zmq, ZMQ_FD, &win, &len) ? errno : 0;
}
#	endif // STORED_HAVE_ZMQ

static int SOCKET_to_HANDLE(SOCKET s, HANDLE& h)
{
	STORED_UNUSED(s)
	stored_assert(s != INVALID_SOCKET); // NOLINT(hicpp-signed-bitwise)

	if(!h) {
		// Note that this is just an Event, unrelated to the SOCKET.
		// Use setEvents() for that.
		if(!(h = WSACreateEvent()))
			return ENOMEM;
	}

	return 0;
}

static int setEvents(SOCKET s, HANDLE h, Pollable::Events events)
{
	stored_assert(s != INVALID_SOCKET); // NOLINT(hicpp-signed-bitwise)
	stored_assert(h);

	long e = FD_CLOSE; // NOLINT(hicpp-signed-bitwise)
	if(events.test(Pollable::PollInIndex))
		e |= FD_READ | FD_ACCEPT | FD_OOB; // NOLINT(hicpp-signed-bitwise)
	if(events.test(Pollable::PollOutIndex))
		e |= FD_WRITE; // NOLINT(hicpp-signed-bitwise)

	if(WSAEventSelect(s, h, e))
		return EIO;

	return 0;
}

int WfmoPoller::init(Pollable const& p, WfmoPollerItem& item) noexcept
{
	// We only have TypedPollables.
	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
	TypedPollable const& tp = static_cast<TypedPollable const&>(p);
	item.pollable = &tp;
	item.h = nullptr;

	int res = 0;

	if(tp.type() == PollableFd::staticType()) {
		if((res = fd_to_HANDLE(down_cast<PollableFd const*>(item.pollable)->fd, item.h)))
			return res;
	} else if(tp.type() == PollableFileLayer::staticType()) {
		// fd() is actually an Event related to the overlapped read.
		// Checking for PollOut wouldn't make sense.
		item.h = down_cast<PollableFileLayer const*>(item.pollable)->layer->fd();
	} else if(tp.type() == PollableHandle::staticType()) {
		// Not every HANDLE can be waited for. However, we don't check its type.
		item.h = down_cast<PollableHandle const*>(item.pollable)->handle;
	} else if(tp.type() == PollableSocket::staticType()) {
		PollableSocket const* ps = down_cast<PollableSocket const*>(item.pollable);
		if((res = SOCKET_to_HANDLE(ps->socket, item.h)))
			return res;
		if((res = setEvents(ps->socket, item.h, tp.events))) {
			WSACloseEvent(item.h);
			return res;
		}
	}
#	ifdef STORED_HAVE_ZMQ
	else if(tp.type() == PollableZmqSocket::staticType()) {
		SOCKET s = INVALID_SOCKET; // NOLINT(hicpp-signed-bitwise)
		if((res = socket_to_SOCKET(down_cast<PollableZmqSocket const&>(tp).socket, s)))
			return res;
		if((res = SOCKET_to_HANDLE(s, item.h)))
			return res;
		if((res = setEvents(s, item.h, tp.events))) {
			WSACloseEvent(item.h);
			return res;
		}
	} else if(tp.type() == PollableZmqLayer::staticType()) {
		SOCKET s = down_cast<PollableZmqLayer const&>(tp).layer->fd();
		if((res = SOCKET_to_HANDLE(s, item.h)))
			return res;
		if((res = setEvents(s, item.h, tp.events))) {
			WSACloseEvent(item.h);
			return res;
		}
	}
#	endif

	return 0;
}

void WfmoPoller::deinit(Pollable const& p, WfmoPollerItem& item) noexcept
{
	// We only have TypedPollables.
	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
	TypedPollable const& tp = static_cast<TypedPollable const&>(p);

	if(
#	ifdef STORED_HAVE_ZMQ
		tp.type() == PollableZmqSocket::staticType()
		|| tp.type() == PollableZmqLayer::staticType() ||
#	endif
		tp.type() == PollableSocket::staticType())
		WSACloseEvent(item.h);
}

#	ifdef STORED_HAVE_ZMQ
static Pollable::Events zmqCheckSocket(WfmoPollerItem& item) noexcept
{
	void* socket = nullptr;

	if(item.pollable->type() == PollableZmqSocket::staticType())
		socket = down_cast<PollableZmqSocket const*>(item.pollable)->socket;
	else if(item.pollable->type() == PollableZmqLayer::staticType())
		socket = down_cast<PollableZmqLayer const*>(item.pollable)->layer->socket();
	else
		return 0;

	int zmq_events = 0;
	size_t len = sizeof(zmq_events);
	Pollable::Events revents;

	if(zmq_getsockopt(socket, ZMQ_EVENTS, &zmq_events, &len))
		revents = 0;
	if((zmq_events & ZMQ_POLLIN)) // NOLINT
		revents.set(Pollable::PollInIndex);
	if((zmq_events & ZMQ_POLLOUT)) // NOLINT
		revents.set(Pollable::PollOutIndex);

	return revents & item.pollable->events;
}
#	endif

int WfmoPoller::doPoll(int timeout_ms, PollItemList& items) noexcept
{
	if(items.empty()) {
		// Nothing to poll. Mimic the timeout anyway.
		if(timeout_ms > 0)
			Sleep((DWORD)timeout_ms);
		return EAGAIN;
	}

	if(items.size() > MAXIMUM_WAIT_OBJECTS)
		return EINVAL;

	m_handles.clear();
	m_indexMap.clear();

	try {
		m_handles.reserve(items.size());
		m_indexMap.reserve(items.size());
	} catch(std::bad_alloc const&) {
		return ENOMEM;
	} catch(...) {
		return EFAULT;
	}

	Pollable::Events revents = 0;
	bool gotSomething = false;

	for(size_t i = 0; i < items.size(); i++) {
#	ifdef STORED_HAVE_ZMQ
		if((revents = zmqCheckSocket(items[i])).any()) {
			// The socket is already triggered, so skip the WFMO,
			// as the corresponding SOCKET may not be triggered at
			// the moment.  See ZeroMQ documentation regarding edge
			// triggered events.

			// Report immediately...
			event(revents, i);
			gotSomething = true;
			// ...and make sure that the WMFO will not block...
			timeout_ms = 0;
			// ...and skip this Pollable for the WFMO.
			continue;
		}
#	endif
		try {
			m_handles.push_back(items[i].h);
			m_indexMap.push_back(i);
		} catch(...) {
			// Should not happen; already reserved memory above.
			return EFAULT;
		}
	}

#	ifdef STORED_HAVE_ZMQ
	if(m_handles.empty()) {
		// Apparently, all items have already been passed through
		// even() because of the zmqCheckSocket(). No need to proceed
		// to WFMO.
		return 0;
	}
#	endif

	// First try.
	DWORD res = WaitForMultipleObjectsEx(
		(DWORD)m_handles.size(), m_handles.data(), FALSE,
		timeout_ms >= 0 ? (DWORD)timeout_ms : INFINITE, TRUE);

	DWORD index = 0;
	int ret = EAGAIN;

	while(true) {
		WfmoPollerItem* item = nullptr;
		revents = 0;

		if(res == WAIT_TIMEOUT) {
			break;
		} else if(res == WAIT_IO_COMPLETION) {
			// Completion routine is executed.
			// Just retry.
			ret = EINTR;
			break;
		} else if(res >= WAIT_OBJECT_0 && res < WAIT_OBJECT_0 + m_handles.size()) {
			index = res - WAIT_OBJECT_0;
			item = &items[m_indexMap[index]];
			Pollable::Events const& events = item->pollable->events;
			if(events.test(Pollable::PollInIndex))
				revents.set(Pollable::PollInIndex);
			if(events.test(Pollable::PollOutIndex))
				revents.set(Pollable::PollOutIndex);
		} else if(res >= WAIT_ABANDONED_0 && res < WAIT_ABANDONED_0 + m_handles.size()) {
			index = res - WAIT_ABANDONED_0;
			item = &items[m_indexMap[index]];
			revents.set(Pollable::PollErrIndex);
		} else {
			ret = EINVAL;
			break;
		}

		if(item->pollable->type() == PollableSocket::staticType())
			WSAResetEvent(item->h);
#	ifdef STORED_HAVE_ZMQ
		else if(item->pollable->type() == PollableZmqSocket::staticType()
			|| item->pollable->type() == PollableZmqLayer::staticType()) {
			revents = zmqCheckSocket(*item);
			WSAResetEvent(item->h);
		}
#	endif

		gotSomething = true;
		event(revents, m_indexMap[index]);

		// WaitForMultipleObjects() only returns the first handle that was finished.
		// But we would like to return them all. Check the rest too.
		m_handles[index] = m_handles.back();
		m_indexMap[index] = m_indexMap.back();

		m_handles.pop_back();
		m_indexMap.pop_back();

		if(m_handles.empty())
			break;

		res = WaitForMultipleObjects((DWORD)m_handles.size(), m_handles.data(), FALSE, 0);
	}

	stored_assert(m_handles.size() == m_indexMap.size());

	// Do report also all other pollables's events.
	for(size_t i = 0; i < m_indexMap.size(); i++)
		event(0, m_indexMap[i]);

	return gotSomething ? 0 : ret;
}

#endif // STORED_OS_WINDOWS



//////////////////////////////////////////////
// ZmqPoller
//

#ifdef STORED_HAVE_ZMQ
int ZmqPoller::init(Pollable const& p, zmq_pollitem_t& item) noexcept
{
	// We only have TypedPollables.
	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
	TypedPollable const& tp = static_cast<TypedPollable const&>(p);

	item.socket = nullptr;
#	ifdef STORED_OS_WINDOWS
	item.fd = INVALID_SOCKET; // NOLINT(hicpp-signed-bitwise)
#	else
	item.fd = -1;
#	endif

	if(tp.type() == PollableZmqSocket::staticType())
		item.socket = down_cast<PollableZmqSocket const&>(tp).socket;
	else if(tp.type() == PollableZmqLayer::staticType())
		item.socket = down_cast<PollableZmqLayer const&>(tp).layer->socket();
#	ifndef STORED_OS_WINDOWS
	else if(tp.type() == PollableFd::staticType())
		item.fd = down_cast<PollableFd const&>(tp).fd;
	else if(tp.type() == PollableFileLayer::staticType())
		item.fd = down_cast<PollableFileLayer const&>(tp).layer->fd();
#	endif
	else
		return EINVAL;

	try {
		item.events = 0;
		if((tp.events.test(Pollable::PollInIndex)))
			item.events |= ZMQ_POLLIN; // NOLINT(hicpp-signed-bitwise)
		if((tp.events.test(Pollable::PollOutIndex)))
			item.events |= ZMQ_POLLOUT; // NOLINT(hicpp-signed-bitwise)
	} catch(std::out_of_range const&) {
		stored_assert(false); // NOLINT
	}

	return 0;
}

#	ifndef STORED_POLL_ZTH_ZMQ
int ZmqPoller::doPoll(int timeout_ms, PollItemList& items) noexcept
{
	int res = ::zmq_poll(items.data(), (int)items.size(), (long)timeout_ms);

	if(res < 0)
		// Error.
		return errno;

	for(size_t i = 0; res > 0 && i < items.size(); i++) {
		zmq_pollitem_t& item = items[i];
		Pollable::Events revents = 0;

		if(item.revents) {
			res--;

			try {
				if(item.revents & ZMQ_POLLIN) // NOLINT(hicpp-signed-bitwise)
					revents.set(Pollable::PollInIndex);
				if(item.revents & ZMQ_POLLOUT) // NOLINT(hicpp-signed-bitwise)
					revents.set(Pollable::PollOutIndex);
				if(item.revents & ZMQ_POLLERR) // NOLINT(hicpp-signed-bitwise)
					revents.set(Pollable::PollErrIndex);
			} catch(std::out_of_range const&) {
				stored_assert(false); // NOLINT
			}
		}

		event(revents, i);
	}

	return 0;
}
#	endif // !STORED_POLL_ZTH_ZMQ
#endif	       // STORED_HAVE_ZMQ



//////////////////////////////////////////////
// PollPoller
//

#if defined(STORED_OS_POSIX)
int PollPoller::init(Pollable const& p, struct pollfd& item) noexcept
{
	// We only have TypedPollables.
	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
	TypedPollable const& tp = static_cast<TypedPollable const&>(p);

	if(tp.type() == PollableFd::staticType())
		item.fd = down_cast<PollableFd const&>(tp).fd;
	else if(tp.type() == PollableFileLayer::staticType())
		item.fd = down_cast<PollableFileLayer const&>(tp).layer->fd();
	else
		return EINVAL;

	try {
		item.events = 0;
		if((tp.events.test(Pollable::PollInIndex)))
			item.events |= POLLIN; // NOLINT(hicpp-signed-bitwise)
		if((tp.events.test(Pollable::PollOutIndex)))
			item.events |= POLLOUT; // NOLINT(hicpp-signed-bitwise)
		if((tp.events.test(Pollable::PollPriIndex)))
			item.events |= POLLPRI; // NOLINT(hicpp-signed-bitwise)
		if((tp.events.test(Pollable::PollHupIndex)))
			item.events |= POLLHUP; // NOLINT(hicpp-signed-bitwise)
	} catch(std::out_of_range const&) {
		stored_assert(false); // NOLINT
	}

	return 0;
}

#	ifndef STORED_POLL_ZTH_POLL
int PollPoller::doPoll(int timeout_ms, PollItemList& items) noexcept
{
	int res = ::poll(items.data(), (nfds_t)items.size(), timeout_ms);

	if(res < 0)
		// Error.
		return errno;

	for(size_t i = 0; res > 0 && i < items.size(); i++) {
		struct pollfd& item = items[i];
		Pollable::Events revents = 0;

		if(item.revents) {
			res--;

			try {
				if(item.revents & POLLIN) // NOLINT(hicpp-signed-bitwise)
					revents.set(Pollable::PollInIndex);
				if(item.revents & POLLOUT) // NOLINT(hicpp-signed-bitwise)
					revents.set(Pollable::PollOutIndex);
				if(item.revents & POLLERR) // NOLINT(hicpp-signed-bitwise)
					revents.set(Pollable::PollErrIndex);
				if(item.revents & POLLPRI) // NOLINT(hicpp-signed-bitwise)
					revents.set(Pollable::PollPriIndex);
				if(item.revents & POLLHUP) // NOLINT(hicpp-signed-bitwise)
					revents.set(Pollable::PollHupIndex);
			} catch(std::out_of_range const&) {
				stored_assert(false); // NOLINT
			}
		}

		event(revents, i);
	}

	return 0;
}
#	endif // !STORED_POLL_ZTH_POLL
#endif	       // STORED_OS_POSIX



//////////////////////////////////////////////
// LoopPoller
//

#if defined(STORED_COMPILER_MSVC)
#	if defined(_WIN64)
#		pragma comment(linker, "/alternatename:poll_once=poll_once_weak")
#	else
#		pragma comment(linker, "/alternatename:_poll_once=_poll_once_weak")
#	endif
extern "C" int poll_once_weak(TypedPollable const& p, Pollable::Events& revents) noexcept
{
	return poll_once_default(p, revents);
}
#elif defined(STORED_OS_OSX)
__attribute__((weak, visibility("hidden"))) int
poll_once(TypedPollable const& p, Pollable::Events& revents) noexcept
{
	return poll_once_default(p, revents);
}
#elif defined(STORED_COMPILER_GCC) || defined(STORED_COMPILER_CLANG) \
	|| defined(STORED_COMPILER_ARMCC)
__attribute__((weak)) int poll_once(TypedPollable const& p, Pollable::Events& revents) noexcept
{
	return poll_once_default(p, revents);
}
#endif

int poll_once_default(TypedPollable const& p, Pollable::Events& revents) noexcept
{
	if(p.type() == PollableCallbackBase::staticType()) {
		revents = down_cast<PollableCallbackBase const&>(p)();
		return 0;
	} else {
		// Not supported by default poll_once().
		return EINVAL;
	}
}

int LoopPoller::init(Pollable const& p, Pollable const*& item) noexcept
{
	item = &p;
	return 0;
}

int LoopPoller::doPoll(int timeout_ms, PollItemList& items) noexcept
{
	// NOLINTNEXTLINE(cppcoreguidelines-avoid-do-while)
	do {
		int res = 0;
		bool gotSomething = false;

		for(size_t i = 0; i < items.size(); i++) {
			Pollable const& p = *items[i];
			Pollable::Events revents = 0;

			// NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
			int e = poll_once(static_cast<TypedPollable const&>(p), revents);

			switch(e) {
			case 0:
				if(revents.any())
					gotSomething = true;

				event(revents, i);
				break;
			case EAGAIN:
				break;
			default:
				if(!res)
					res = e;
			}
		}

		if(res)
			return res;
		if(gotSomething)
			return 0;
	} while(timeout_ms < 0);

	return EAGAIN;
}



//////////////////////////////////////////////
// Poller
//

#ifndef STORED_HAVE_ZTH
Poller::~Poller()
{
#	ifndef CPPCHECK
	// cppcheck trips on this virtual call, even though the class is final.
	// Suppressing it here does not work, so hide it from cppcheck.
	clear();
#	endif
}
#endif // !STORED_HAVE_ZTH

} // namespace stored
