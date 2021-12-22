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

#include <libstored/macros.h>
#include <libstored/config.h>
#include <libstored/util.h>

#include <memory>
#include <stdexcept>
#include <vector>

#if STORED_cplusplus >= 201103L
#  include <initializer_list>
#endif

#ifdef STORED_OS_WINDOWS
#  include <winsock2.h>
#endif

#ifdef STORED_HAVE_ZTH
#  include <zth>
#endif

#if !defined(STORED_POLL_ZTH_WFMO) && \
    !defined(STORED_POLL_WFMO) && \
    !defined(STORED_POLL_ZTH_ZMQ) && \
    !defined(STORED_POLL_ZMQ) && \
    !defined(STORED_POLL_ZTH_POLL) && \
    !defined(STORED_POLL_POLL) && \
    !defined(STORED_POLL_ZTH_LOOP) && \
    !defined(STORED_POLL_LOOP)
// Do auto-detect

#  ifdef STORED_OS_WINDOWS
#    ifdef STORED_HAVE_ZTH
#      define STORED_POLL_ZTH_WFMO	// Use WaitForMultipleObjects via the zth::Waiter.
#    else
#      define STORED_POLL_WFMO		// Use WaitForMultipleObjects.
#    endif
#  else
#    if defined(STORED_HAVE_ZMQ)
#      ifdef STORED_HAVE_ZTH
#        define STORED_POLL_ZTH_ZMQ	// Use zmq_poll() via zth::Waiter.
#      else
#        define STORED_POLL_ZMQ		// Use zmq_poll().
#    elif define(STORED_OS_POSIX)
#      ifdef STORED_HAVE_ZTH
#        define STORED_POLL_ZTH_POLL	// Use poll() via zth::Waiter.
#      else
#        define STORED_POLL_POLL	// Use poll().
#      endif
#    else
#      ifdef STORED_HAVE_ZTH
#        define STORED_POLL_ZTH_LOOP	// Use poll_once() via zth::Waiter.
#      else
#        define STORED_POLL_LOOP	// Use poll_once() in a loop.
#      endif
#    endif
#  endif
#endif // auto-detect

#if defined(STORED_POLL_POLL) || defined(STORED_POLL_ZTH_POLL)
#  include <poll.h>
#endif
#if STORED_HAVE_ZMQ
#  include <zmq.h>
#endif

namespace stored {

	//////////////////////////////////////////////
	// Pollable
	//

#ifdef STORED_HAVE_ZTH
	// In case we have Zth, we will forward all poll requests to Zth
	// instead.  So, base our types on Zth's types.
	using zth::Pollable;
	typedef zth::PollerBase PollerInterface;

#else // !STORED_HAVE_ZTH
	// We don't have Zth, but we still want to have an equivalent interface.
	// Define it here.

	/*!
	 * \brief A pollable thing.
	 */
	struct Pollable {
		enum Events { PollIn = 1, PollOut = 2, PollErr = 4, PollPri = 8, PollHup = 16 };

		void* user_data;
		Events events;
		Events revents;
	};

	/*!
	 * \brief Abstract base class of a poller.
	 * \tparam Allocator the allocator type to use for dynamic memory allocations
	 */
	template <template <typename> typename Allocator = Config::Allocator>
	class PollerInterface {
	public:
		/*!
		 * \brief Result of #poll().
		 */
		typedef std::vector<Pollable*, Allocator<Pollable*>::allocator_type> Result;

		/*!
		 * \brief Dtor.
		 */
		virtual ~PollerInterface() is_default

		/*!
		 * \brief Add a pollable object.
		 *
		 * Once a pollable is added, do not modify its properties, except for
		 * \c user_data.
		 *
		 * \return 0 on success, otherwise an errno
		 */
		virtual int add(Pollable& p) = 0;

#if STORED_cplusplus >= 201103L
		/*!
		 * \brief Add pollable objects.
		 *
		 * \return 0 on success, otherwise an errno
		 */
		int add(std::initializer_list<Pollable&> l)
		{
			__try {
				reserve(l.size());
			} __catch(...) {
				return ENOMEM;
			}

			int res = 0;
			size_t count = 0;

			for(auto& p : l) {
				if((res = add(p))) {
					// Rollback.
					for(auto it = l.begin(); it != l.end() && count > 0; ++it, count--)
						remove(*it);
					break;
				}

				// Success.
				count++;
			}

			return res;
		}
#endif

		/*!
		 * \brief Remove a pollable object.
		 * \return 0 on success, otherwise an errno
		 */
		virtual int remove(Pollable& p) noexcept = 0;

		/*!
		 * \brief Poll.
		 *
		 * When \p timeout_ms is -1, \c poll() blocks until at least one of the
		 * pollables got an event.  When \p timeout_ms is 0, \c poll() never
		 * blocks.  Otherwise, \c poll() blocks at most for \p timeout_ms and
		 * return with a timeout, or return earlier when a pollable got an
		 * event.
		 *
		 * \return The registered pollables that have an event set.
		 *         If the result is empty, \c errno is set to the error.
		 */
		virtual Result const& poll(int timeout_ms) noexcept = 0;

		/*!
		 * \brief Reserve memory to add more pollables.
		 *
		 * \except std::bad_alloc when allocation fails
		 */
		virtual void reserve(size_t more) = 0;
	};
#endif // !STORED_HAVE_ZTH

	/*!
	 * \brief A Pollable with run-time type information.
	 *
	 * A subclass must use \c STORED_POLLABLE_TYPE(subclass_type) in its
	 * definition.
	 */
	struct TypedPollable : public Pollable {
#ifdef __cpp_rtti
		typedef std::type_id const& Type;
		static Type staticType() noexcept { return typeid(Pollable); }
#else
		typedef void const* Type;
		static Type staticType() noexcept { return nullptr; }
#endif
		virtual Type type() const noexcept = 0;
	};

	/*!
	 * \def STORED_POLLABLE_TYPE
	 * \brief Helper macro to generate the TypedPollable::staticType() and TypedPollable::type() functions.
	 *
	 * The type functions will be final. So, the class cannot be subclassed
	 * any further with more specific (sub)types.
	 */
#ifdef __cpp_rtti
#  define STORED_POLLABLE_TYPE(T) \
	public: \
		constexpr static ::stored::Pollable::Type staticType() noexcept { return typeid(T); } \
		virtual ::stored::Pollable::Type type() const noexcept final { staticType(); } \
	private:
#else
#  define STORED_POLLABLE_TYPE(T) \
		constexpr static ::zth::Pollable::Type staticType() noexcept { static char const t = 0; return (::zth::Pollable::Type)&t; } \
		virtual ::zth::Pollable::Type type() const noexcept final { staticType(); } \
	private:
#endif

	class PollableFd final : public TypedPollable {
		STORED_POLLABLE_TYPE(PollableFd)
	public:
		explicit PollableFd(int fd) : fd(fd) {}
		int fd;
	};

	class PollableFileLayer final : public TypedPollable {
		STORED_POLLABLE_TYPE(PollableFileLayer)
	public:
		explicit PollableFileLayer(PolledFileLayer& f) : f(&f) {}
		PolledFileLayer* f;
	};

#ifdef STORED_OS_WINDOWS
	class PollableSocket final : public TypedPollable {
		STORED_POLLABLE_TYPE(PollableSocket)
	public:
		explicit PollableSocket(SOCKET s) : s(s) {}
		SOCKET s;
	};

	class PollableHandle final : public TypedPollable {
		STORED_POLLABLE_TYPE(PollableHandle)
	public:
		explicit PollableHandle(HANDLE h) : h(h) {}
		HANDLE h;
	};
#endif

#ifdef STORED_HAVE_ZMQ
	class PollableZmqSocket : public TypedPollable {
		STORED_POLLABLE_TYPE(PollableZmqSocket)
	public:
		explicit PollableZmqSocket(void* socket) : socket(socket) {}
		void* socket;
	};

	class PollableZmqLayer : public TypedPollable {
		STORED_POLLABLE_TYPE(PollableZmqLayer)
	public:
		explicit PollableZmqLayer(ZmqLayer& zmq) : zmq(&zmq) {}
		ZmqLayer* zmq;
	};
#endif

	template <typename T>
	class PollerBase : public PollerInterface<Config::Allocator> {
#if STORED_cplusplus >= 201103L
	public:
		PollerBase(PollerBase&& p) = default;
		PollerBase& operator=(PollerBase&& p) = default;
		PollerBase(PollerBase const&) = delete;
		void operator=(PollerBase const&) = delete;
#else
	private:
		PollerBase(PollerBase const&);
		void operator=(PollerBase const&);
#endif
	public:
		typedef PollerInterface<Config::Allocator> base;
		typedef T Item;
		typedef Vector<Item>::type PollItemList;

		Result m_result;
		using base::Result;

		virtual ~PollerBase() override
		{
			// You should call clear() first.
			zth_assert(m_pollables.empty());
		}

#if STORED_cplusplus >= 201103L
	protected:
		/*!
		 * \brief Helper for constructors with an initializer list.
		 */
		void throwing_add(std::initializer_list<Pollable&> l)
		{
			errno = add(l);
#  ifdef __cpp_exception
			switch(errno) {
			case ENOMEM: throw std::bad_alloc();
			case EINVAL: throw std::invalid_argument("");
			default: throw std::runtime_error("");
			}
#  else
			if(errno)
				std::abort();
#  endif
		}
	public:
#endif

		void reserve(size_t more) final
		{
			m_pollables.reserve(m_pollables.size() + more);
			m_items.reserve(m_pollables.capacity());
			m_result.reserve(m_pollables.capacity());
		}

		int migrateTo(PollerBase& p)
		{
			__try {
				p.reserve(m_pollables.size());
			} __catch(...) {
				return ENOMEM;
			}

			size_t added = 0;

			size_t i = 0;
			int res = 0;
			for(; i < m_pollables.size(); i++)
				if((res = p.add(*m_pollables[i])))
					goto rollback;

			clear();
			return 0;

		rollback:
			for(; i > 0; i--)
				p.remove(*m_pollables[i - 1u]);

			return res;
		}

		void clear() noexcept
		{
			for(size_t i = 0; i < m_pollables.size(); i++)
				deinit(*m_pollables[i], m_items[i]);

			m_pollables.clear();
			m_items.clear();
			m_result.clear();
		}

		int add(Pollable& p) final
		{
			Item item;
			int res = init(p, item);
			if(res)
				return res;

			__try {
#if STORED_cplusplus >= 201103L
				m_items.emplace_back(std::move(item));
#else
				m_items.push_back(item);
#endif
			} __catch(...) {
				return ENOMEM;
			}

			__try {
				m_pollables.push_back(&p);
			} __catch(...) {
				deinit(p, m_items.back());
				m_items.pop_back();
				return ENOMEM;
			}

			return 0;
		}

		int remove(Pollable& p) noexcept final
		{
			for(size_t i = m_pollables.size(); i > 0; i--) {
				if(m_pollables[i - 1u] == &p) {
					Item& item = m_items[i - 1u];
					if(i < m_pollables.size()) {
						deinit(p, item);
#if STORED_cplusplus >= 201103L
						m_pollables[i - 1u] = std::move(m_pollables.back());
						m_items[i - 1u] = std::move(m_fd.back());
#else
						m_pollables[i - 1u] = m_pollables.back();
						m_items[i - 1u] = m_items.back();
#endif
					}
					m_pollables.pop_back();
					m_items.pop_back();
					return 0;
				}
			}

			return ESRCH;
		}

		Result const& poll(int timeout_ms) noexcept final
		{
			m_result.clear();

			if(m_pollables.empty()) {
				errno = EINVAL;
				return m_result;
			}

			int res = doPoll(timeout_ms, m_items);
			if(m_result.empty() && !res)
				res = EAGAIN;

			errno = res;
			return m_result;
		}

	protected:
		void event(size_t index, Pollable::Event revents) noexcept
		{
			zth_assert(index < m_pollables.size());
			Pollable& p = m_pollables[index];
			if((p.revents = revents))
				m_result.push_back(&p);
		}

	private:
		virtual int init(Pollable& p, Item& i) noexcept = 0;
		virtual void deinit(Pollable& UNUSED_PAR(p), Item& UNUSED_PAR(i)) noexcept = 0;
		virtual int doPoll(int timeout_ms, PollItemList& items) noexcept = 0;

	private:
		Vector<Pollable*>::type m_pollables;
		PollItemList m_items;
	};



	//////////////////////////////////////////////
	// Polling using zmq_poll()
	//

#if defined(STORED_POLL_ZMQ) || defined(STORED_POLL_ZTH_ZMQ)
	template <typename Base>
	class ZmqPollerBase : public Base {
	private:
		virtual ~PollPollerBase() override
		{
			clear();
		}

		int init(Pollable& p, zmq_pollitem_t& item) noexcept final
		{
			item.socket = 0;
			item.fd = -1;

			if(p.type() == PollableFd::staticType())
				item.fd = static_cast<PollableFd&>(p).fd;
			else if(p.type() == PollableFileLayer::staticType())
				item.fd = static_cast<PollableFileLayer&>(p).f->fd();
			else if(p.type() == PollableZmqSocket::staticType())
				item.socket = static_cast<PollableZmqSocket&>(p).socket;
			else if(p.type() == PollableZmqLayer::staticType())
				item.socket = static_cast<PollableZmqLayer&>(p).zmq->socket();
			else
				return EINVAL;

			if((p.events & Pollable::PollIn))
				item.events |= ZMQ_POLLIN;
			if((p.events & Pollable::PollOut))
				item.events |= ZMQ_POLLOUT;

			return 0;
		}

		void deinit(Pollable& UNUSED_PAR(p), struct pollfd& UNUSED_PAR(i)) noexcept final {}
	};
#endif

#ifdef STORED_POLL_POLL
	class ZmqPoller final : public ZmqPollerBase<PollerBase<zmq_pollitem_t> > {
	public:
		typedef ZmqPollerBase<PollerBase<zmq_pollitem_t> > base;

		~ZmqPoller() final is_default

#  if STORED_cplusplus >= 201103L
		ZmqPoller(std::initializer_list<Pollable&> l)
		{
			throwing_add(l);
		}
#  endif // C++11

	private:
		int doPoll(int timeout_ms, base::PollItemList& items) noexcept final
		{
			int res = ::zmq_poll(&m_items[0], (int)m_items.size(), (long)timeout_ms);

			if(res < 0)
				// Error.
				return errno;

			for(size_t i = 0; res > 0 && i < m_items.size(); i++) {
				zmq_pollitem_t& item = m_items[i];

				if(!item.revents)
					continue;

				res--;
				Pllable::Event revents = 0;

				if(item.revents & ZMQ_POLLIN)
					revents |= Pollable::PollIn;
				if(item.revents & ZMQ_POLLOUT)
					revents |= Pollable::PollOut;
				if(item.revents & ZMQ_POLLERR)
					revents |= Pollable::PollErr;

				if(!revents)
					continue;

				event(i, revents);
			}

			return 0;
		}
	};

	typedef ZmqPoller Poller;
#endif // STORED_POLL_ZMQ

#ifdef STORED_POLL_ZTH_ZMQ
	class ZmqPollerServer final : public ZmqPollerBase<zth::ZmqPoller<Config::Allocator> > {
	public:
		~ZmqPollerServer() final is_default
	};

	typedef ZmqPollerServer PollServer;
	using zth::Poller;
#endif // STORED_POLL_ZTH_ZMQ




	//////////////////////////////////////////////
	// Polling using poll()
	//

#if defined(STORED_POLL_POLL) || defined(STORED_POLL_ZTH_POLL)
	template <typename Base>
	class PollPollerBase : public Base {
	private:
		virtual ~PollPollerBase() override
		{
			clear();
		}

		int init(Pollable& p, struct pollfd& item) noexcept final
		{
			if(p.type() == PollableFd::staticType())
				item.fd = static_cast<PollableFd&>(p).fd;
			else if(p.type() == PollableFileLayer::staticType())
				item.fd = static_cast<PollableFileLayer&>(p).f->fd();
			else
				return EINVAL;

			if((p.events & Pollable::PollIn))
				item.events |= POLLIN;
			if((p.events & Pollable::PollOut))
				item.events |= POLLOUT;
			if((p.events & Pollable::PollPri))
				item.events |= POLLPRI;
			if((p.events & Pollable::PollHup))
				item.events |= POLLHUP;

			return 0;
		}

		void deinit(Pollable& UNUSED_PAR(p), struct pollfd& UNUSED_PAR(i)) noexcept final {}
	};
#endif

#ifdef STORED_POLL_POLL
	class PollPoller final : public PollPollerBase<PollerBase<struct pollfd> > {
	public:
		typedef PollerBase base;
		using base::Result;

		~PollPoller() final is_default

#  if STORED_cplusplus >= 201103L
		PollPoller(std::initializer_list<Pollable&> l)
		{
			throwing_add(l);
		}
#  endif // C++11

	private:
		int doPoll(int timeout_ms, base::PollItemList& items) noexcept final
		{
			int res = ::poll(&m_items[0], m_items.size(), timeout_ms);

			if(res < 0)
				// Error.
				return errno;

			for(size_t i = 0; res > 0 && i < m_items.size(); i++) {
				struct pollfd& fd = m_items[i];

				if(!item.revents)
					continue;

				res--;

				if(item.revents & POLLIN)
					revents |= Pollable::PollIn;
				if(item.revents & POLLOUT)
					revents |= Pollable::PollOut;
				if(item.revents & POLLERR)
					revents |= Pollable::PollErr;
				if(item.revents & POLLPRI)
					revents |= Pollable::PollPri;
				if(item.revents & POLLHUP)
					revents |= Pollable::PollHup;

				if(!revents)
					continue;

				event(i, revents);
			}

			return 0;
		}
	};

	typedef PollPoller Poller;
#endif // STORED_POLL_POLL

#ifdef STORED_POLL_ZTH_POLL
	class PollPollerServer final : public PollPollerBase<zth::PollPoller<Config::Allocator> > {
	public:
		~PollPollerServer() final is_default
	};

	typedef PollPollerServer PollServer;
	using zth::Poller;
#endif // STORED_POLL_ZTH_POLL


	//////////////////////////////////////////////
	// Polling using poll_once()
	//
#if defined(STORED_POLL_LOOP) || defined(STORED_POLL_ZTH_LOOP)
	int poll_once(Pollable& p) noexcept;

	template <typename Base>
	class LoopPollerBase : public Base {
	public:
		virtual ~LoopPollerBase() override is_default;
		{
			clear();
		}

	private:
		int init(Pollable& p, Pollable*& item) noexcept final
		{
			item = &p;
			return 0;
		}

		void deinit(Pollable& UNUSED_PAR(p), Pollable*& UNUSED_PAR(i)) noexcept final {}

		int doPoll(int timeout_ms, PollItemList& items) noexcept final
		{
			do {
				int res = 0;
				bool gotSomething = false;

				for(size_t i = 0; i < items.size(); i++) {
					Pollable& p = *items[i];
					p.revents = 0;
					int e = poll_once(p);
					switch(e) {
					case 0:
						if(p->revents) {
							gotSomething = true;
							event(i, p->revents);
						}
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
#endif

#ifdef STORED_POLL_LOOP
	class LoopPoller final : public LoopPollerBase<PollerBase<Pollable*> > {
	public:
		~LoopPoller() final is_default

#  if STORED_cplusplus >= 201103L
		LoopPoller(std::initializer_list<Pollable&> l)
		{
			throwing_add(l);
		}
#  endif // C++11
	};
#endif // STORED_POLL_POLL

#ifdef STORED_POLL_ZTH_POLL
	class LoopPollerServer final : public LoopPollerBase<zth::PollerServer<Pollable*, Config::Allocator> > {
	public:
		~LoopPollerServer() final is_default
	};
#endif // STORED_POLL_LOOP

} // namespace
#endif // __cplusplus
#endif // LIBSTORED_POLLER_H
