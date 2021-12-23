/*
 * libstored, distributed debuggable data stores.
 * Copyright (C) 2020-2021  Jochem Rutgers
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

//////////////////////////////////////////////
// ZmqPoller
//

#ifdef STORED_HAVE_ZMQ
int ZmqPoller::init(Pollable const& p, zmq_pollitem_t& item) noexcept
{
	// We only have TypedPollables.
	TypedPollable const& tp = static_cast<TypedPollable const&>(p);

	item.socket = 0;
	item.fd = -1;

	if(tp.type() == PollableFd::staticType())
		item.fd = static_cast<PollableFd const&>(tp).fd;
	else if(tp.type() == PollableFileLayer::staticType())
		item.fd = static_cast<PollableFileLayer const&>(tp).layer->fd();
	else if(tp.type() == PollableZmqSocket::staticType())
		item.socket = static_cast<PollableZmqSocket const&>(tp).socket;
	else if(tp.type() == PollableZmqLayer::staticType())
		item.socket = static_cast<PollableZmqLayer const&>(tp).layer->socket();
	else
		return EINVAL;

	item.events = 0;
	if((tp.events.test(Pollable::PollInIndex)))
		item.events |= ZMQ_POLLIN;
	if((tp.events.test(Pollable::PollOutIndex)))
		item.events |= ZMQ_POLLOUT;

	return 0;
}

#	ifndef STORED_POLL_ZTH_ZMQ
int ZmqPoller::doPoll(int timeout_ms, PollItemList& items) noexcept
{
	int res = ::zmq_poll(&items[0], (int)items.size(), (long)timeout_ms);

	if(res < 0)
		// Error.
		return errno;

	for(size_t i = 0; res > 0 && i < items.size(); i++) {
		zmq_pollitem_t& item = items[i];
		Pollable::Events revents = 0;

		if(item.revents) {
			res--;

			if(item.revents & ZMQ_POLLIN)
				revents.set(Pollable::PollInIndex);
			if(item.revents & ZMQ_POLLOUT)
				revents.set(Pollable::PollOutIndex);
			if(item.revents & ZMQ_POLLERR)
				revents.set(Pollable::PollErrIndex);
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
	TypedPollable const& tp = static_cast<TypedPollable const&>(p);

	if(tp.type() == PollableFd::staticType())
		item.fd = static_cast<PollableFd const&>(tp).fd;
	else if(tp.type() == PollableFileLayer::staticType())
		item.fd = static_cast<PollableFileLayer const&>(tp).layer->fd();
	else
		return EINVAL;

	item.events = 0;
	if((tp.events.test(Pollable::PollInIndex)))
		item.events |= POLLIN;
	if((tp.events.test(Pollable::PollOutIndex)))
		item.events |= POLLOUT;
	if((tp.events.test(Pollable::PollPriIndex)))
		item.events |= POLLPRI;
	if((tp.events.test(Pollable::PollHupIndex)))
		item.events |= POLLHUP;

	return 0;
}

#	ifndef STORED_POLL_ZTH_POLL
int PollPoller::doPoll(int timeout_ms, PollItemList& items) noexcept
{
	int res = ::poll(&items[0], items.size(), timeout_ms);

	if(res < 0)
		// Error.
		return errno;

	for(size_t i = 0; res > 0 && i < items.size(); i++) {
		struct pollfd& item = items[i];
		Pollable::Events revents = 0;

		if(item.revents) {
			res--;

			if(item.revents & POLLIN)
				revents.set(Pollable::PollInIndex);
			if(item.revents & POLLOUT)
				revents.set(Pollable::PollOutIndex);
			if(item.revents & POLLERR)
				revents.set(Pollable::PollErrIndex);
			if(item.revents & POLLPRI)
				revents.set(Pollable::PollPriIndex);
			if(item.revents & POLLHUP)
				revents.set(Pollable::PollHupIndex);
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
#	pragma comment(linker, "/alternatename:_poll_once=_poll_once_weak")
int poll_once_weak(TypedPollable const& p, Pollable::Events& revents) noexcept
#elif defined(STORED_COMPILER_GCC) || defined(STORED_COMPILER_CLANG) \
	|| defined(STORED_COMPILER_ARMCC)
__attribute__((weak)) int poll_once(TypedPollable const& p, Pollable::Events& revents) noexcept
#else
int poll_once_default(TypedPollable const& p, Pollable::Events& revents) noexcept
#endif
{
	if(p.type() == PollableCallbackBase::staticType()) {
		revents = static_cast<PollableCallbackBase const&>(p)();
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
	do {
		int res = 0;
		bool gotSomething = false;

		for(size_t i = 0; i < items.size(); i++) {
			Pollable const& p = *items[i];
			Pollable::Events revents = 0;

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
	clear();
}
#endif // !STORED_HAVE_ZTH

} // namespace stored
