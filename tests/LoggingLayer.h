#ifndef TESTS_LOGGING_LAYER_H
#define TESTS_LOGGING_LAYER_H

/*
 * libstored, a Store for Embedded Debugger.
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

#include "libstored/protocol.h"
#include "libstored/util.h"

#include <deque>
#include <cinttypes>

#ifdef __cplusplus

class LoggingLayer : public stored::ProtocolLayer {
	CLASS_NOCOPY(LoggingLayer)
public:
	typedef stored::ProtocolLayer base;

	LoggingLayer(ProtocolLayer* up = nullptr, ProtocolLayer* down = nullptr)
		: base(up, down)
		, m_partial()
	{}

	virtual ~LoggingLayer() override = default;

	virtual void decode(void* buffer, size_t len) override {
		m_decoded.emplace_back(static_cast<char const*>(buffer), len);
		base::decode(buffer, len);
	}

	std::deque<std::string>& decoded() { return m_decoded; }
	std::deque<std::string> const & decoded() const { return m_decoded; }
	std::string allDecoded() const {
		return join(decoded());
	}

	virtual void encode(void const* buffer, size_t len, bool last = true) override {
		if(m_partial && !m_encoded.empty())
			m_encoded.back().append(static_cast<char const*>(buffer), len);
		else
			m_encoded.emplace_back(static_cast<char const*>(buffer), len);
		m_partial = !last;

		base::encode(buffer, len, last);
	}

	std::deque<std::string>& encoded() { return m_encoded; }
	std::deque<std::string> const & encoded() const { return m_encoded; }
	std::string allEncoded() const {
		return join(encoded());
	}

	void clear() {
		encoded().clear();
		decoded().clear();
	}

	static std::string join(std::deque<std::string> const& list) {
		std::string res;
		for(auto const& s : list)
			res += s;
		return res;
	}

private:
	std::deque<std::string> m_decoded;
	std::deque<std::string> m_encoded;
	bool m_partial;
};

void printBuffer(void const* buffer, size_t len, char const* prefix = nullptr, FILE* f = stdout) {
	std::string s = stored::string_literal(buffer, len, prefix);
	s += "\n";
	fputs(s.c_str(), f);
}

void printBuffer(std::string const& s, char const* prefix = nullptr, FILE* f = stdout) {
	printBuffer(s.data(), s.size(), prefix, f);
}

#if defined(STORED_POLL_LOOP) || defined(STORED_POLL_ZTH_LOOP)
#  include "libstored/poller.h"
#  include <poll.h>
namespace stored {
	int poll_once(Poller::Event const& e, Poller::events_t& revents) {
		switch(e.type) {
		case Poller::Event::TypeFd: {
			struct pollfd fd = {};
			fd.fd = e.fd;
			if(e.events & Poller::PollIn)
				fd.events |= POLLIN;
			if(e.events & Poller::PollOut)
				fd.events |= POLLOUT;
			if(e.events & Poller::PollErr)
				fd.events |= POLLERR;

do_poll:
			switch(poll(&fd, 1, 0)) {
			case 0:
				// Timeout
				return 0;
			case 1:
				// Got it.
				revents = 0;
				if(fd.revents & POLLIN)
					revents |= (short)Poller::PollIn;
				if(fd.revents & POLLOUT)
					revents |= (short)Poller::PollOut;
				if(fd.revents & POLLERR)
					revents |= (short)Poller::PollErr;
				return 0;
			case -1:
				switch(errno) {
				case EINTR:
				case EAGAIN:
					goto do_poll;
				default:
					return errno;
				}
			default:
				return EINVAL;
			}
		}
		default:
			return EINVAL;
		}
	}
} // namespace
#endif

#endif // __cplusplus
#endif // TESTS_LOGGING_LAYER_H
