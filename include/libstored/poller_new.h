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

	class PolledFileLayer final : public TypedPollable {
		STORED_POLLABLE_TYPE(PolledFileLayer)
	public:
		explicit PolledFileLayer(PolledFileLayer& f) : f(&f) {}
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

	class PollerBase : public PollerInterface<Config::Allocator> {
		CLASS_NOCOPY(PollerBase)
	public:
		typedef PollerInterface<Config::Allocator> base;
		using base::Result;

		virtual ~PollerBase() override is_default

		virtual void reserve(size_t more) {}
		virtual int migrateTo(PollerBase& b) {}
		virtual void clear() noexcept {}
	};



	//////////////////////////////////////////////
	// Polling using poll()
	//

#ifdef STORED_POLL_POLL
	class PollPoller : public PollerBase {
	public:
		typedef PollerBase base;
		using base::Result;

		virtual ~PollPoller() override is_default

#  if STORED_cplusplus >= 201103L
		PollPoller(PollPoller&& p) noexcept
		{
			*this = std::move(p);
		}

		PollPoller& operator=(PollPoller&& p) noexcept
		{
			m_pollables = std::move(p.m_pollables);
			m_fds = std::move(m_fds);
			m_result = std::move(m_result);
		}

		PollPoller(std::initializer_list<Pollable&> l)
		{
			reserve(l.size());

			for(auto& p : l)
				if((errno = add(p))) {
#    ifdef __cpp_exception
					switch(errno) {
					case ENOMEM: throw std::bad_alloc();
					case EINVAL: throw std::invalid_argument("");
					default: throw std::runtime_error("");
					}
#    else
					return;
#    endif
				}

			errno = 0;
		}
#  endif // C++11

		virtual int migrateTo(PollerBase& p) override
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

		virtual void reserve(size_t more) override
		{
			m_pollables.reserve(m_pollables.size() + more);
			m_fds.reserve(m_pollables.capacity());
			m_result.reserve(m_pollables.capacity());
		}

		virtual int add(Pollable& p) override
		{
			__try {
				if(p.type() == PollableFd::staticType())
					m_fds.push_back(static_cast<PollableFd&>(p).fd);
				if(p.type() == PolledFileLayer::staticType())
					m_fds.push_back(static_cast<PollableFd&>(p).fd());
				else
					return EINVAL;
			} __catch(...) {
				return ENOMEM;
			}

			__try {
				m_pollables.push_back(&p);
			} __catch(...) {
				m_fds.pop_back();
				return ENOMEM;
			}

			return 0;
		}

		virtual int remove(Pollable& p) noexcept override
		{
			for(size_t i = m_pollables.size(); i > 0; i--)
				if(m_pollables[i - 1u] == &p) {
					m_pollables[i - 1u] = m_pollables.back();
					m_pollables.pop_back();
					m_fds[i - 1u] = m_fd.back();
					m_fds.pop_back();
					return 0;
				}

			return ESRCH;
		}

		virtual void clear() noexcept override
		{
			m_pollables.clear();
			m_fds.clear();
			m_result.clear();
		}

		virtual Result const& poll(int timeout_ms) noexcept override
		{
			m_result.clear();

			int res = ::poll(&m_fds[0], m_fds.size(), timeout_ms);

			if(res < 0)
				// Error.
				return m_result;

			if(res == 0) {
				// Timeout.
				errno = 0;
				return m_result;
			}

			for(size_t i = 0; res > 0 && i < m_fds.size(); i++) {
				struct pollfd const& fd = m_fds[i];

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

				m_pollables[i].revents = revents;
				m_result.push_back(m_pollables[i]);
			}

			errno = m_result.empty() ? EAGAIN : 0;
			return m_result;
		}

	private:
		Vector<Pollable*>::type m_pollables;
		Vector<struct pollfd>::type m_fds;
		Result m_result;
	};
#endif // STORED_POLL_POLL

#ifdef STORED_POLL_ZTH_POLL
	class PollPollerServer : public zth::PollPoller<Config::Allocator> {
	public:
		typedef zth::PollerPoller<Config::Allocator> base;
		virtual ~PollPollerServer() override is_default

	private:
		virtual int init(Pollable const& p, struct pollfd& item) noexcept override
		{
			if(p.type() == PollableFd::staticType())
				item.fd = static_cast<PollableFd&>(p).fd;
			if(p.type() == PolledFileLayer::staticType())
				item.fd = static_cast<PollableFd&>(p).fd();
			else
				return EINVAL;

			item.events = p.events;
			return 0;

		}
	};
#endif // STORED_POLL_ZTH_POLL


	//////////////////////////////////////////////
	// Polling using poll_once()
	//
#ifdef STORED_POLL_LOOP
	int poll_once(Pollable& p) noexcept;

	class LoopPoller : public PollerBase {
	public:
		typedef PollerBase base;
		using base::Result;

		virtual ~LoopPoller() override is_default

#  if STORED_cplusplus >= 201103L
		LoopPoller(LoopPoller&& p) noexcept
		{
			*this = std::move(p);
		}

		LoopPoller& operator=(LoopPoller&& p) noexcept
		{
			m_pollables = std::move(p.m_pollables);
			m_result = std::move(m_result);
		}

		LoopPoller(std::initializer_list<Pollable&> l)
		{
			reserve(l.size());

			for(auto& p : l)
				if((errno = add(p))) {
#    ifdef __cpp_exception
					switch(errno) {
					case ENOMEM: throw std::bad_alloc();
					default: throw std::runtime_error("");
					}
#    else
					return;
#    endif
				}

			errno = 0;
		}
#  endif // C++11

		virtual int migrateTo(PollerBase& p) override
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

		virtual void reserve(size_t more) override
		{
			m_pollables.reserve(m_pollables.size() + more);
			m_result.reserve(m_pollables.capacity());
		}

		virtual int add(Pollable& p) override
		{
			__try {
				m_pollables.push_back(&p);
				return 0;
			} __catch(...) {
				return ENOMEM;
			}
		}

		virtual int remove(Pollable& p) noexcept override
		{
			for(size_t i = m_pollables.size(); i > 0; i--)
				if(m_pollables[i - 1u] == &p) {
					m_pollables[i - 1u] = m_pollables.back();
					m_pollables.pop_back();
					return 0;
				}

			return ESRCH;
		}

		virtual void clear() noexcept override
		{
			m_pollables.clear();
			m_result.clear();
		}

		virtual Result const& poll(int timeout_ms) noexcept override
		{
			m_result.clear();

			do {
				int e = 0;
				for(size_t i = 0; i < m_pollables.size(); i++) {
					Pollable* p = m_pollables[i];

					p->revents = 0;
					int res = poll_once(*p);
					if(res && res != EAGAIN && !e)
						e = res;
					else if(p->revents)
						m_result.push_back(p);
				}

				if(!m_result.empty() || e) {
					errno = e;
					return m_result;
				}
			} while(timeout_ms < 0);

			errno = EAGAIN;
			return m_result;
		}

	private:
		Vector<Pollable*>::type m_pollables;
		Result m_result;
	};
#endif // STORED_POLL_POLL

#ifdef STORED_POLL_ZTH_POLL
	class LoopPollerServer : public zth::PollerServer<Pollable const*, Config::Allocator> {
	public:
		typedef zth::PollerServer<Pollable const*, Config::Allocator> base;
		virtual ~LoopPollerServer() override is_default

	private:
		virtual int init(Pollable const& p, Pollable const*& item) noexcept override
		{
			item = &p;
			return 0;
		}

		virtual int doPoll(int timeout_ms, PollItemList& items) noexcept override
		{
			do {
				bool gotSomething = false;
				for(size_t i = 0; i < items.size(); i++) {
					Pollable* p = items[i];

					p->revents = 0;
					int res = poll_once(*p);
					if(res)
						return res;

					if(p->revents) {
						event(p->revents, i);
						gotSometing = true;
					}
				}

				if(gotSomething)
					return 0;
			} while(timeout_ms < 0);
		}
	};
#endif // STORED_POLL_LOOP

} // namespace
#endif // __cplusplus
#endif // LIBSTORED_POLLER_H
