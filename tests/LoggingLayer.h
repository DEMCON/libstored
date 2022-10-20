#ifndef TESTS_LOGGING_LAYER_H
#define TESTS_LOGGING_LAYER_H

/*
 * libstored, distributed debuggable data stores.
 * Copyright (C) 2020-2022  Jochem Rutgers
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "libstored/protocol.h"
#include "libstored/util.h"

#include <cinttypes>
#include <deque>

#ifdef __cplusplus

class LoggingLayer : public stored::ProtocolLayer {
	STORED_CLASS_NOCOPY(LoggingLayer)
public:
	typedef stored::ProtocolLayer base;

	LoggingLayer(ProtocolLayer* up = nullptr, ProtocolLayer* down = nullptr)
		: base(up, down)
		, m_partial()
	{}

	virtual ~LoggingLayer() override = default;

	virtual void decode(void* buffer, size_t len) override
	{
		m_decoded.emplace_back(static_cast<char const*>(buffer), len);
		base::decode(buffer, len);
	}

	std::deque<std::string>& decoded()
	{
		return m_decoded;
	}

	std::deque<std::string> const& decoded() const
	{
		return m_decoded;
	}

	std::string allDecoded() const
	{
		return join(decoded());
	}

	virtual void encode(void const* buffer, size_t len, bool last = true) override
	{
		if(m_partial && !m_encoded.empty())
			m_encoded.back().append(static_cast<char const*>(buffer), len);
		else
			m_encoded.emplace_back(static_cast<char const*>(buffer), len);
		m_partial = !last;

		base::encode(buffer, len, last);
	}

	std::deque<std::string>& encoded()
	{
		return m_encoded;
	}

	std::deque<std::string> const& encoded() const
	{
		return m_encoded;
	}

	std::string allEncoded() const
	{
		return join(encoded());
	}

	void clear()
	{
		encoded().clear();
		decoded().clear();
	}

	static std::string join(std::deque<std::string> const& list)
	{
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

void printBuffer(void const* buffer, size_t len, char const* prefix = nullptr, FILE* f = stdout)
{
	auto s = stored::string_literal(buffer, len, prefix);
	s += "\n";
	fputs(s.c_str(), f);
}

void printBuffer(std::string const& s, char const* prefix = nullptr, FILE* f = stdout)
{
	printBuffer(s.data(), s.size(), prefix, f);
}
#endif // __cplusplus
#endif // TESTS_LOGGING_LAYER_H
