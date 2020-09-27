#ifndef TESTS_LOGGING_LAYER_H
#define TESTS_LOGGING_LAYER_H

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

#include "libstored/protocol.h"

#include <deque>

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

private:
	std::deque<std::string> m_decoded;
	std::deque<std::string> m_encoded;
	bool m_partial;
};

void printBuffer(void const* buffer, size_t len, char const* prefix = nullptr, FILE* f = stdout) {
	std::string s;
	if(prefix)
		s += prefix;

	uint8_t const* b = static_cast<uint8_t const*>(buffer);
	char buf[16];
	for(size_t i = 0; i < len; i++) {
		switch(b[i]) {
		case '\0': s += "\\0"; break;
		case '\r': s += "\\r"; break;
		case '\n': s += "\\n"; break;
		case '\t': s += "\\t"; break;
		case '\\': s += "\\\\"; break;
		default:
			if(b[i] < 0x20 || b[i] >= 0x7f) {
				snprintf(buf, sizeof(buf), "\\x%02" PRIx8, b[i]);
				s += buf;
			} else {
				s += (char)b[i];
			}
		}
	}

	s += "\n";
	fputs(s.c_str(), f);
}

void printBuffer(std::string const& s, char const* prefix = nullptr, FILE* f = stdout) {
	printBuffer(s.data(), s.size(), prefix, f);
}

#endif // __cplusplus
#endif // TESTS_LOGGING_LAYER_H
