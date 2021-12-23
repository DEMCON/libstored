#ifndef LIBSTORED_POLLER_H
#define LIBSTORED_POLLER_H
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

#ifdef __cplusplus

#	include <libstored/macros.h>
#	include <libstored/allocator.h>
#	include <libstored/config.h>
#	include <libstored/protocol.h>
#	include <libstored/util.h>
#	include <libstored/version.h>

#	include <bitset>
#	include <memory>
#	include <stdexcept>
#	include <typeinfo>
#	include <vector>

#	if STORED_cplusplus >= 201103L
#		include <initializer_list>
#		include <utility>
#	endif

#	ifdef STORED_OS_WINDOWS
#		include <winsock2.h>
#	endif

#	ifdef STORED_OS_POSIX
#		include <poll.h>
#	endif

#	ifdef STORED_HAVE_ZTH
#		include <zth>
#		if ZTH_VERSION_NUM < 10000
#			error Unsupport Zth version.
#		endif
#	endif

#	if !defined(STORED_POLL_ZTH_WFMO) && !defined(STORED_POLL_WFMO)        \
		&& !defined(STORED_POLL_ZTH_ZMQ) && !defined(STORED_POLL_ZMQ)   \
		&& !defined(STORED_POLL_ZTH_POLL) && !defined(STORED_POLL_POLL) \
		&& !defined(STORED_POLL_ZTH_LOOP) && !defined(STORED_POLL_LOOP)
// Do auto-detect

#		ifdef STORED_OS_WINDOWS
#			ifdef STORED_HAVE_ZTH
// Use WaitForMultipleObjects via the zth::Waiter.
#				define STORED_POLL_ZTH_WFMO
#			else
// Use WaitForMultipleObjects.
#				define STORED_POLL_WFMO
#			endif
#		elif defined(STORED_HAVE_ZMQ)
#			ifdef STORED_HAVE_ZTH
// Use zmq_poll() via zth::Waiter.
#				define STORED_POLL_ZTH_ZMQ
#			else
// Use zmq_poll().
#				define STORED_POLL_ZMQ
#			endif
#		elif define(STORED_OS_POSIX)
#			ifdef STORED_HAVE_ZTH
// Use poll() via zth::Waiter.
#				define STORED_POLL_ZTH_POLL
#			else
// Use poll().
#				define STORED_POLL_POLL
#			endif
#		elif defined(STORED_HAVE_ZTH)
// Use poll_once() via zth::Waiter.
#			define STORED_POLL_ZTH_LOOP
#		else
// Use poll_once() in a loop.
#			define STORED_POLL_LOOP
#		endif
#	endif // auto-detect

#	if defined(STORED_POLL_POLL) || defined(STORED_POLL_ZTH_POLL)
#		include <poll.h>
#	endif
#	if STORED_HAVE_ZMQ
#		include <zmq.h>
#	endif

#	if !defined(STORED_HAVE_ZTH) && STORED_VERSION_NUM < 20000
#		define STORED_POLL_OLD
#	endif

namespace stored {

//////////////////////////////////////////////
// Pollable
//

#	ifdef STORED_HAVE_ZTH

// In case we have Zth, we will forward all poll requests to Zth instead.  So,
// base our types on Zth's types.
using zth::Pollable;

#	else  // !STORED_HAVE_ZTH

// We don't have Zth, but we still want to have an equivalent interface. Define
// it here.

/*!
 * \brief A pollable thing.
 */
struct Pollable {
	/*!
	 * \brief Flags to be used with #events and #revents.
	 */
	enum EventsFlags {
		PollInIndex,
		PollOutIndex,
		PollErrIndex,
		PollPriIndex,
		PollHupIndex,
		FlagCount,
	};

	/*! \brief Type of #events and #revents. */
	typedef std::bitset<FlagCount> Events;

	static unsigned long const PollIn = 1UL << PollInIndex;
	static unsigned long const PollOut = 1UL << PollOutIndex;
	static unsigned long const PollErr = 1UL << PollErrIndex;
	static unsigned long const PollPri = 1UL << PollPriIndex;
	static unsigned long const PollHup = 1UL << PollHupIndex;

	/*!
	 * \brief Ctor.
	 */
	explicit constexpr Pollable(Events const& e, void* user = nullptr) noexcept
		: user_data(user)
		, events(e)
		, revents()
	{}

	/*!
	 * \brief User data.
	 *
	 * This can be changed, even after adding a Pollable to a Poller.
	 */
	void* user_data;

	/*!
	 * \brief Events to poll.
	 *
	 * Do not change these events after adding the Pollable to a Poller.
	 *
	 * Not every Poller may support all flags.
	 */
	Events events;

	/*!
	 * \brief Returned events by a poll.
	 *
	 * Do not set manually, let the Poller do that.
	 */
	Events revents;
};
#	endif // !STORED_HAVE_ZTH

/*!
 * \brief A Pollable with run-time type information.
 *
 * A subclass must use \c STORED_POLLABLE_TYPE(subclass_type) in its
 * definition.
 */
class TypedPollable : public Pollable {
	STORED_CLASS_NEW_DELETE(TypedPollable)
public:
	explicit constexpr TypedPollable(Events const& events, void* user = nullptr) noexcept
		: Pollable(events, user)
	{}

	virtual ~TypedPollable() is_default

#	ifdef __cpp_rtti
	typedef std::type_info const& Type;
	static Type staticType() noexcept
	{
		return typeid(Pollable);
	}
#	else  // __cpp_rtti
	typedef void const* Type;
	static Type staticType() noexcept
	{
		return nullptr;
	}
#	endif // !__cpp_rtti
	virtual Type type() const noexcept = 0;
};

/*!
 * \def STORED_POLLABLE_TYPE
 * \brief Helper macro to generate the TypedPollable::staticType() and
 *	TypedPollable::type() functions.
 *
 * The type functions will be final. So, the class cannot be subclassed any
 * further with more specific (sub)types.
 */
#	ifdef __cpp_rtti
#		define STORED_POLLABLE_TYPE(T)                                           \
		public:                                                                   \
			static ::stored::TypedPollable::Type staticType() noexcept        \
			{                                                                 \
				return typeid(T);                                         \
			}                                                                 \
			virtual ::stored::TypedPollable::Type type() const noexcept final \
			{                                                                 \
				return staticType();                                      \
			}                                                                 \
                                                                                          \
		private:                                                                  \
			STORED_CLASS_NEW_DELETE(T)
#	else // !__cpp_rtti
#		define STORED_POLLABLE_TYPE(T)                                        \
			static ::zth::TypedPollable::Type staticType() noexcept        \
			{                                                              \
				static char const t = 0;                               \
				return (::zth::Pollable::Type)&t;                      \
			}                                                              \
			virtual ::zth::TypedPollable::Type type() const noexcept final \
			{                                                              \
				return staticType();                                   \
			}                                                              \
                                                                                       \
		private:                                                               \
			STORED_CLASS_NEW_DELETE(T)
#	endif // !__cpp_rtti

class PollableCallbackBase : public TypedPollable {
	STORED_POLLABLE_TYPE(PollableCallbackBase)
protected:
	explicit constexpr PollableCallbackBase(Events const& events, void* user = nullptr) noexcept
		: TypedPollable(events, user)
	{}

	virtual ~PollableCallbackBase() override is_default;

public:
	virtual Events operator()() const noexcept = 0;
};

/*!
 * \brief Use a callback function while polling.
 *
 * The function type \p F must be compatible with
 * <tt>Pollable::Events& (Pollable const&)</tt>.
 */
template <typename F = Pollable::Events (*)(Pollable const&)>
class PollableCallback final : public PollableCallbackBase {
	STORED_CLASS_NOCOPY(PollableCallback)
	STORED_CLASS_NEW_DELETE(PollableCallback)
public:
	typedef Events(f_type)(Pollable const&);

	PollableCallback(F f, Events const& events, void* user = nullptr)
		: PollableCallbackBase(events, user)
		, f(f)
	{}

#	if STORED_cplusplus >= 201103L
	template <typename F_>
	PollableCallback(F_&& f, Events const& events, void* user = nullptr)
		: PollableCallbackBase{events, user}
		, f{std::forward<F_>(f)}
	{}
#	endif

	virtual ~PollableCallback() override is_default

	virtual Events operator()() const noexcept final
	{
		try {
			return f(static_cast<Pollable const&>(*this));
		} catch(...) {
			return Pollable::PollErr;
		}
	}

	F f;
};

inline PollableCallback<>
pollable(PollableCallback<>::f_type f, Pollable::Events const& events, void* user = nullptr)
{
	return PollableCallback<>(f, events, user);
}

#	if STORED_cplusplus >= 201103L
template <typename F>
PollableCallback<typename std::decay<F>::type>
pollable(F&& f, Pollable::Events const& events, void* user = nullptr)
{
	return PollableCallback<typename std::decay<F>::type>{std::forward<F>(f), events, user};
}
#	else
template <typename F>
PollableCallback<F> pollable(F const& f, Pollable::Events const& events, void* user)
{
	return PollableCallback<F>(f, events, user);
}
#	endif

class PollableFd final : public TypedPollable {
	STORED_POLLABLE_TYPE(PollableFd)
public:
	constexpr PollableFd(int fd, Events const& events, void* user = nullptr) noexcept
		: TypedPollable(events, user)
		, fd(fd)
	{}

	virtual ~PollableFd() override is_default

	int fd;
};

class PollableFileLayer final : public TypedPollable {
	STORED_POLLABLE_TYPE(PollableFileLayer)
public:
	constexpr PollableFileLayer(
		PolledFileLayer& layer, Events const& events, void* user = nullptr) noexcept
		: TypedPollable(events, user)
		, layer(&layer)
	{}

	virtual ~PollableFileLayer() override is_default

	PolledFileLayer* layer;
};

#	ifdef STORED_OS_WINDOWS
class PollableSocket final : public TypedPollable {
	STORED_POLLABLE_TYPE(PollableSocket)
public:
	constexpr PollableSocket(SOCKET socket, Events const& events, void* user = nullptr) noexcept
		: TypedPollable(events, user)
		, socket(socket)
	{}

	virtual ~PollableSocket() override is_default

	SOCKET socket;
};

class PollableHandle final : public TypedPollable {
	STORED_POLLABLE_TYPE(PollableHandle)
public:
	constexpr PollableHandle(HANDLE handle, Events const& events, void* user = nullptr) noexcept
		: TypedPollable(events, user)
		, handle(handle)
	{}

	virtual ~PollableHandle() override is_default

	HANDLE handle;
};
#	endif // STORED_OS_WINDOWS

#	ifdef STORED_HAVE_ZMQ
class PollableZmqSocket : public TypedPollable {
	STORED_POLLABLE_TYPE(PollableZmqSocket)
public:
	constexpr PollableZmqSocket(
		void* socket, Events const& events, void* user = nullptr) noexcept
		: TypedPollable(events, user)
		, socket(socket)
	{}

	virtual ~PollableZmqSocket() override is_default

	void* socket;
};

class PollableZmqLayer : public TypedPollable {
	STORED_POLLABLE_TYPE(PollableZmqLayer)
public:
	constexpr PollableZmqLayer(
		ZmqLayer& layer, Events const& events, void* user = nullptr) noexcept
		: TypedPollable(events, user)
		, layer(&layer)
	{}

	virtual ~PollableZmqLayer() override is_default

	ZmqLayer* layer;
};
#	endif // STORED_HAVE_ZMQ



//////////////////////////////////////////////
// Poller base classes
//

#	ifndef STORED_HAVE_ZTH
/*!
 * \brief Abstract base class of a poller.
 */
template <typename PollItem_>
class PollerBase {
	STORED_CLASS_NOCOPY(PollerBase)
public:
	typedef PollItem_ PollItem;
	typedef typename Vector<PollItem>::type PollItemList;

protected:
	PollerBase() noexcept is_default
	virtual ~PollerBase() is_default

	virtual void event(Pollable::Events revents, size_t index) noexcept = 0;

protected:
	virtual int init(Pollable const& p, PollItem& item) noexcept = 0;
	virtual void deinit(Pollable const& UNUSED_PAR(p), PollItem& UNUSED_PAR(item)) noexcept {}
	virtual int doPoll(int timeout_ms, PollItemList& items) noexcept = 0;
};
#	endif // !STORED_HAVE_ZTH



//////////////////////////////////////////////
// Polling using WaitForMultipleObjects()
//

// TODO



//////////////////////////////////////////////
// Polling using zmq_poll()
//
// Without Zth:			With Zth:
//
// stored::PollerBase		zth::ZmqPoller
//	^				^
//	|				|
// stored::ZmqPoller		stored::ZmqPoller
// = stored::PollerImpl		= stored::PollerServer
//	^
//	|
// stored::Poller		zth::PollerClient
//				= stored::Poller

#	ifdef STORED_HAVE_ZMQ
#		ifdef STORED_HAVE_ZTH
typedef zth::ZmqPoller ZmqPollerBase;
#		else
typedef PollerBase<zmq_pollitem_t> ZmqPollerBase;
#		endif

class ZmqPoller : public ZmqPollerBase {
	STORED_CLASS_NOCOPY(ZmqPoller)
	STORED_CLASS_NEW_DELETE(ZmqPoller)
public:
	virtual ~ZmqPoller() override is_default

#		ifndef STORED_HAVE_ZTH
protected:
#		endif
	ZmqPoller() is_default

#		ifdef STORED_HAVE_ZTH
private:
#		else
protected:
#		endif
	virtual int init(Pollable const& p, zmq_pollitem_t& item) noexcept final
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

		if((tp.events.test(Pollable::PollInIndex)))
			item.events |= ZMQ_POLLIN;
		if((tp.events.test(Pollable::PollOutIndex)))
			item.events |= ZMQ_POLLOUT;

		return 0;
	}

#		ifndef STORED_POLL_ZTH_ZMQ
	virtual int doPoll(int timeout_ms, PollItemList& items) noexcept final
	{
		int res = ::zmq_poll(&items[0], (int)items.size(), (long)timeout_ms);

		if(res < 0)
			// Error.
			return errno;

		for(size_t i = 0; res > 0 && i < items.size(); i++) {
			zmq_pollitem_t& item = items[i];
			Pollable::Events revents = 0;

			if(!item.revents) {
				res--;

				if(item.revents & ZMQ_POLLIN)
					revents.set(Pollable::PollInIndex);
				if(item.revents & ZMQ_POLLOUT)
					revents.set(Pollable::PollOutIndex);
				if(item.revents & ZMQ_POLLERR)
					revents.set(Pollable::PollErr);
			}

			event(revents, i);
		}

		return 0;
	}
#		endif // !STORED_POLL_ZTH_ZMQ
};

#		ifdef STORED_POLL_ZMQ
typedef ZmqPoller PollerImpl;
#		elif defined(STORED_POLL_ZTH_ZMQ)
typedef ZmqPoller PollerServer;
#		endif
#	endif // STORED_HAVE_ZMQ



//////////////////////////////////////////////
// Polling using poll()
//
// Without Zth:			With Zth:
//
// stored::PollerBase		zth::PollPoller
//	^				^
//	|				|
// stored::PollPoller		stored::PollPoller
// = stored::PollerImpl		= stored::PollerServer
//	^
//	|
// stored::Poller		zth::PollerClient
//				= stored::Poller

#	if defined(STORED_OS_POSIX)
#		ifdef STORED_HAVE_ZTH
typedef zth::PollPoller PollPollerBase;
#		else
typedef PollerBase<struct pollfd> PollPollerBase;
#		endif

class PollPoller : public PollPollerBase {
	STORED_CLASS_NOCOPY(PollPoller)
	STORED_CLASS_NEW_DELETE(PollPoller)
public:
	virtual ~PollPoller() override is_default

#		ifndef STORED_HAVE_ZTH
protected:
#		endif
	PollPoller() is_default

#		ifdef STORED_HAVE_ZTH
private:
#		else
protected:
#		endif
	virtual int init(Pollable const& p, struct pollfd& item) noexcept final
	{
		// We only have TypedPollables.
		TypedPollable const& tp = static_cast<TypedPollable const&>(p);

		if(tp.type() == PollableFd::staticType())
			item.fd = static_cast<PollableFd const&>(tp).fd;
		else if(tp.type() == PollableFileLayer::staticType())
			item.fd = static_cast<PollableFileLayer const&>(tp).layer->fd();
		else
			return EINVAL;

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

#		ifndef STORED_POLL_ZTH_POLL
	virtual int doPoll(int timeout_ms, PollItemList& items) noexcept final
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
#		endif // !STORED_POLL_ZTH_POLL
};

#		ifdef STORED_POLL_POLL
typedef PollPoller PollerImpl;
#		elif defined(STORED_POLL_ZTH_POLL)
typedef PollPoller PollerServer;
#		endif
#	endif // STORED_OS_POSIX



//////////////////////////////////////////////
// Polling using poll_once()
//
// Without Zth:			With Zth:
//
// stored::PollerBase		zth::PollerServer
//	^				^
//	|				|
// stored::LoopPoller		stored::LoopPoller
// = stored::PollerImpl		= stored::PollerServer
//	^
//	|
// stored::Poller		zth::PollerClient
//				= stored::Poller

#	ifdef STORED_COMPILER_MSVC
extern "C"
#	endif
	int
	poll_once(TypedPollable const& p, Pollable::Events& revents) noexcept;

#	if defined(STORED_COMPILER_MSVC)
#		pragma comment(linker, "/alternatename:_poll_once=_poll_once_weak")
inline int poll_once_weak(TypedPollable const& p, Pollable::Events& revents) noexcept
#	elif defined(STORED_COMPILER_GCC) || defined(STORED_COMPILER_CLANG)
inline int poll_once(TypedPollable const& p, Pollable::Events& revents) noexcept
#	endif
{
	if(p.type() == PollableCallbackBase::staticType()) {
		revents = static_cast<PollableCallbackBase const&>(p)();
		return 0;
	} else {
		// Not supported by default poll_once().
		return EINVAL;
	}
}

#	ifdef STORED_HAVE_ZTH
typedef zth::PollerServer<Pollable const*> LoopPollerBase;
#	else
typedef PollerBase<Pollable const*> LoopPollerBase;
#	endif

class LoopPoller : public LoopPollerBase {
	STORED_CLASS_NOCOPY(LoopPoller)
	STORED_CLASS_NEW_DELETE(LoopPoller)
public:
	virtual ~LoopPoller() override is_default

#	ifndef STORED_HAVE_ZTH
protected:
#	endif
	LoopPoller() is_default

#	ifdef STORED_HAVE_ZTH
private:
#	else
protected:
#	endif
	virtual int init(Pollable const& p, Pollable const*& item) noexcept final
	{
		item = &p;
		return 0;
	}

	virtual int doPoll(int timeout_ms, PollItemList& items) noexcept final
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
};

#	ifdef STORED_POLL_LOOP
typedef LoopPoller PollerImpl;
#	elif defined(STORED_POLL_ZTH_LOOP)
typedef LoopPoller PollerServer;
#	endif



//////////////////////////////////////////////
// Poller
//
// Without Zth:			With Zth:
//
// stored::PollerImpl		zth::PollerClient
//	^			= stored::Poller
//	|
// stored::InheritablePoller
//	^
//	|
// stored::Poller
//
// The interface of stored::Poller is equivalent to zth::PollerClient.  User
// code can just use stored::Poller, regardless whether Zth is used.
//
// For this, stored::Poller (without Zth) must implement the interface based on
// the minimal function provided by the PollerServer-like interface classes.
//
// To inherit a Poller, use InheritablePoller as base class. Poller itself does
// not add any logic, and is final.
//

#	ifdef STORED_HAVE_ZTH
typedef zth::PollerClient Poller;
#	else // !STORED_HAVE_ZTH

template <typename PollerImpl = PollerImpl>
class InheritablePoller : public PollerImpl {
	STORED_CLASS_NOCOPY(InheritablePoller)
	STORED_CLASS_NEW_DELETE(InheritablePoller)
protected:
	using typename PollerImpl::PollItem;
	using typename PollerImpl::PollItemList;

#		ifdef STORED_POLL_OLD
	struct OldResult {
		unsigned long events;
		unsigned long revents;
		void* user_data;
		Pollable* p;

		Pollable* operator->() const
		{
			return p;
		}
		Pollable& operator*() const
		{
			return *p;
		}
	};

	typedef typename Vector<OldResult>::type Result;
#		else
	typedef typename Vector<Pollable*>::type Result;
#		endif

	InheritablePoller() is_default

	/*!
	 * \brief Dtor.
	 */
	virtual ~InheritablePoller() override
	{
		stored_assert(empty());
	}

	void throwing(int res)
	{
		errno = res;

#		ifdef __cpp_exceptions
		switch(errno) {
		case 0:
			break;
		case ENOMEM:
			throw std::bad_alloc();
		default:
			throw std::runtime_error("");
		}
#		endif
	}

public:
	/*!
	 * \brief Add a pollable object.
	 *
	 * Once a pollable is added, do not modify its properties, except for
	 * \c user_data.
	 *
	 * \return 0 on success, otherwise an errno
	 */
	virtual int add(Pollable& p) noexcept
	{
		try {
			reserve(1);
		} catch(std::bad_alloc const&) {
			return ENOMEM;
		} catch(...) {
			return EINVAL;
		}

		PollItem item;
		int res = this->init(p, item);
		if(res)
			return res;

		try {
#		if STORED_cplusplus >= 201103L
			m_items.emplace_back(std::move(item));
#		else
			m_items.push_back(item);
#		endif
		} catch(...) {
			this->deinit(p, item);
			return EINVAL;
		}

		try {
			m_pollables.push_back(&p);
		} catch(...) {
			this->deinit(p, m_items.back());
			m_items.pop_back();
			return EINVAL;
		}

		return 0;
	}

#		if STORED_cplusplus >= 201103L
	int add(std::initializer_list<std::reference_wrapper<Pollable>> l) noexcept
	{
		try {
			reserve(l.size());
		} catch(std::bad_alloc const&) {
			return ENOMEM;
		} catch(...) {
			return EINVAL;
		}

		int res = 0;
		size_t count = 0;

		for(auto& p : l) {
			if((res = add(p))) {
				// Rollback.
				for(auto const* it = l.begin(); it != l.end() && count > 0;
				    ++it, count--)
					remove(*it);
				break;
			}

			// Success.
			count++;
		}

		return res;
	}
#		endif

	/*!
	 * \brief Remove a pollable object.
	 * \return 0 on success, otherwise an errno
	 */
	virtual int remove(Pollable& p) noexcept
	{
		for(size_t i = 0; i < m_pollables.size(); i++)
			if(m_pollables[i] == &p) {
				this->deinit(p, m_items[i]);
#		if STORED_cplusplus >= 201103L
				m_items[i] = std::move(m_items.back());
#		else
				m_items[i] = m_items.back();
#		endif
				m_items.pop_back();
				m_pollables[i] = m_pollables.back();
				m_pollables.pop_back();
				return 0;
			}

		return ESRCH;
	}

	/*!
	 * \brief Reserve memory to add more pollables.
	 *
	 * \exception std::bad_alloc when allocation fails
	 */
	virtual void reserve(size_t more)
	{
		size_t capacity = m_pollables.size() + more;
		m_pollables.reserve(capacity);
		m_items.reserve(capacity);
		m_result.reserve(capacity);
	}

	/*!
	 * \brief Checks if there is any pollable registered.
	 */
	bool empty() const noexcept
	{
		return m_pollables.empty();
	}

	virtual void clear() noexcept
	{
		for(size_t i = 0; i < m_items.size(); i++)
			this->deinit(*m_pollables[i], m_items[i]);

		m_pollables.clear();
		m_items.clear();
	}

	virtual Result const& poll(int timeout_ms = -1) noexcept
	{
		stored_assert(m_pollables.size() == m_items.size());
		m_result.clear();

		if((errno = this->doPoll(timeout_ms, m_items)))
			m_result.clear();

		return m_result;
	}

protected:
	virtual void event(Pollable::Events revents, size_t index) noexcept override
	{
		if(revents.none())
			return;

		stored_assert(index < m_items.size());

		Pollable* p = m_pollables[index];
		p->revents = revents;

#		ifdef STORED_POLL_OLD
		OldResult r = {p->events.to_ulong(), revents.to_ulong(), p->user_data, p};
		m_result.push_back(r);
#		else
		m_result.push_back(p);
#		endif
	}

private:
	Vector<Pollable*>::type m_pollables;
	PollItemList m_items;
	Result m_result;
};

template <typename PollerImpl = PollerImpl>
class CustomPoller final : public InheritablePoller<PollerImpl> {
	STORED_CLASS_NOCOPY(CustomPoller)
	STORED_CLASS_NEW_DELETE(CustomPoller)
public:
	CustomPoller() is_default

	virtual ~CustomPoller() override
	{
		this->clear();
	}

#		if STORED_cplusplus >= 201103L
	CustomPoller(std::initializer_list<std::reference_wrapper<Pollable>> l)
	{
		this->throwing(this->add(l));
	}
#		endif // C++11
};

class Poller final : public InheritablePoller<> {
	STORED_CLASS_NOCOPY(Poller)
	STORED_CLASS_NEW_DELETE(Poller)
public:
	typedef InheritablePoller<> base;

	Poller() is_default

	virtual ~Poller() override
	{
		clear();
	}

#		if STORED_cplusplus >= 201103L
	Poller(std::initializer_list<std::reference_wrapper<Pollable>> l)
	{
		throwing(add(l));
	}
#		endif // C++11

#		ifdef STORED_POLL_OLD
	// Provide the old Poller interface, for a while.
public:
	using base::add;
	using base::poll;
	using base::remove;
	using base::Result;

	typedef unsigned long events_t;
	static unsigned long const PollIn = Pollable::PollIn;
	static unsigned long const PollOut = Pollable::PollOut;
	static unsigned long const PollErr = Pollable::PollErr;
	static unsigned long const PollPri = Pollable::PollPri;
	static unsigned long const PollHup = Pollable::PollHup;

	//	STORED_DEPRECATED("Use the new Pollables instead")
	Result const& poll(long timeout_us, bool UNUSED_PAR(suspend) = false)
	{
		return base::poll((int)(timeout_us / 1000L));
	}

	struct compare_fd {
		bool operator()(PollableFd const& p, int fd) const
		{
			return p.fd == fd;
		}
	};

	//	STORED_DEPRECATED("Use the new Pollables instead")
	int add(int fd, void* user_data, events_t events)
	{
		return add<PollableFd>(fd, user_data, events);
	}

	int modify(int fd, events_t events)
	{
		return modify<PollableFd>(fd, events, compare_fd());
	}

	int remove(int fd)
	{
		return remove<PollableFd>(fd, compare_fd());
	}

	struct compare_FileLayer {
		bool operator()(PollableFileLayer const& p, PolledFileLayer& l) const
		{
			return p.layer == &l;
		}
	};

	//	STORED_DEPRECATED("Use the new Pollables instead")
	int add(PolledFileLayer& layer, void* user_data, events_t events)
	{
		return add<PollableFileLayer, PolledFileLayer&>(layer, user_data, events);
	}

	int modify(PolledFileLayer& layer, events_t events)
	{
		return modify<PollableFileLayer, PolledFileLayer&>(
			layer, events, compare_FileLayer());
	}

	int remove(PolledFileLayer& layer)
	{
		return remove<PollableFileLayer, PolledFileLayer&>(layer, compare_FileLayer());
	}

#			if defined(STORED_OS_WINDOWS)
	struct compare_SOCKET {
		bool operator()(PollableSocket const& p, SOCKET s) const
		{
			return p.socket == socket;
		}
	};

	//	STORED_DEPRECATED("Use the new Pollables instead")
	int add(SOCKET socket, void* user_data, events_t events)
	{
		return add<PollableSocket>(socket, user_data, events);
	}

	int modify(SOCKET socket, events_t events)
	{
		return modify<PollableSocket>(socket, events, compare_SOCKET());
	}

	int remove(SOCKET socket)
	{
		return remove<PollableSocket>(socket, compare_SOCKET());
	}

	struct compare_HANDLE {
		bool operator()(PollableHandle const& p, HANDLE handle) const
		{
			return p.handle == handle;
		}
	};

	//	STORED_DEPRECATED("Use the new Pollables instead")
	int addh(HANDLE handle, void* user_data, events_t events)
	{
		return add<PollableHandle>(handle, user_data, events);
	}

	int modifyh(HANDLE handle, events_t events)
	{
		return modify<PollableHandle>(handle, events, compare_HANDLE());
	}

	int removeh(HANDLE handle)
	{
		return remove<PollableHandle>(handle, compare_HANDLE());
	}
#			endif // STORED_OS_WINDOWS

#			if defined(STORED_HAVE_ZMQ)
	struct compare_socket {
		bool operator()(PollableZmqSocket const& p, void* socket) const
		{
			return p.socket == &socket;
		}
	};

	//	STORED_DEPRECATED("Use the new Pollables instead")
	int add(void* socket, void* user_data, events_t events)
	{
		return add<PollableZmqSocket>(socket, user_data, events);
	}

	int modify(void* socket, events_t events)
	{
		return modify<PollableZmqSocket>(socket, events, compare_socket());
	}

	int remove(void* socket)
	{
		return remove<PollableZmqSocket>(socket, compare_socket());
	}

	struct compare_zmq {
		bool operator()(PollableZmqLayer const& p, ZmqLayer& layer) const
		{
			return p.layer == &layer;
		}
	};

	//	STORED_DEPRECATED("Use the new Pollables instead")
	int add(ZmqLayer& layer, void* user_data, events_t events)
	{
		return add<PollableZmqLayer, ZmqLayer&>(layer, user_data, events);
	}

	int modify(ZmqLayer& layer, events_t events)
	{
		return modify<PollableZmqLayer, ZmqLayer&>(layer, events, compare_zmq());
	}

	int remove(ZmqLayer& layer)
	{
		return remove<PollableZmqLayer, ZmqLayer&>(layer, compare_zmq());
	}
#			endif // STORED_HAVE_ZMQ

	virtual void clear() noexcept override
	{
		for(decltype(m_pollables.begin()) it = m_pollables.begin(); it != m_pollables.end();
		    ++it)
			delete *it;

		m_pollables.clear();
		base::clear();
	}

private:
	template <typename T, typename A>
	int add(A a, void* user_data, events_t events)
	{
		T* p = new T(a, events, user_data);
		int res = add(*p);

		if(res)
			delete p;
		else
			m_pollables.push_back(p);

		return res;
	}

	template <typename T, typename A, typename C>
	int modify(A a, events_t events, C const& c)
	{
		T* p = find<T, A, C>(a, c);
		if(!p)
			return ESRCH;

		void* user_data = p->user_data;
		remove(*p);
		return add<T, A>(a, user_data, events);
	}

	template <typename T, typename A, typename C>
	int remove(A a, C const& c)
	{
		T* p = find<T, A, C>(a, c);
		if(!p)
			return ESRCH;

		int res = remove(*p);
		delete p;
		return res;
	}

	template <typename T, typename A, typename C>
	T* find(A a, C const& cmp)
	{
		for(decltype(m_pollables.begin()) it = m_pollables.begin(); it != m_pollables.end();
		    ++it)
			if((*it)->type() == T::staticType()) {
				T* p = static_cast<T*>(*it);
				if(cmp(*p, a))
					return p;
			}

		return nullptr;
	}

private:
	Vector<TypedPollable*>::type m_pollables;
#		endif
};
#	endif // !STORED_HAVE_ZTH

} // namespace stored
#endif // __cplusplus

#endif // LIBSTORED_POLLER_H
