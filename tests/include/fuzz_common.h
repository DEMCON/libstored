#ifndef FUZZ_COMMON_H
#define FUZZ_COMMON_H

/*
 * libstored, distributed debuggable data stores.
 * Copyright (C) 2020-2023  Jochem Rutgers
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <stored>

#include <algorithm>
#include <string>
#include <utility>

#include <cassert>

#ifdef __AFL_FUZZ_TESTCASE_LEN
#	define HAVE_AFL
#	pragma clang diagnostic ignored "-Wunused-but-set-variable"
#	pragma clang diagnostic ignored "-Wshorten-64-to-32"
#endif

class EchoLayer : public stored::ProtocolLayer {
	STORED_CLASS_NOCOPY(EchoLayer)
public:
	EchoLayer() = default;

	virtual void decode(void* buffer, size_t len) override
	{
		encode(buffer, len);
	}
};

/*!
 * \brief A wrapper for a set of messages.
 *
 * The buffer contains messages, which are a byte with the length and then one message of that
 * length.
 */
class Messages {
public:
	Messages(void const* buf, size_t len)
		: m_buf{static_cast<char const*>(buf)}
		, m_len{len}
	{}

	class Iterator {
	protected:
		Iterator() = default;

		Iterator(char const* buf, size_t len)
			: m_buf{buf}
			, m_len{len}
		{
			assert(m_len == 0 || m_buf);
		}

	public:
		auto operator*() const
		{
			size_t l = std::min<size_t>(m_len - 1, (size_t)(uint8_t)*m_buf);
			return std::string_view{m_buf + 1, l};
		}

		bool operator==(Iterator const& rhs) const
		{
			return (m_len == 0 && rhs.m_len == 0)
			       || (m_len == rhs.m_len && m_buf == rhs.m_buf);
		}

		bool operator!=(Iterator const& rhs) const
		{
			return !this->operator==(rhs);
		}

		Iterator& operator++()
		{
			assert(m_len > 0);
			auto l = std::min<size_t>(m_len - 1, (size_t)(uint8_t)*m_buf) + 1;
			m_len -= l;
			m_buf += l;
			return *this;
		}

	private:
		char const* m_buf{};
		size_t m_len{};
		friend class Messages;
	};

	Iterator begin() const
	{
		return {m_buf, m_len};
	}

	Iterator end() const
	{
		return {};
	}

public:
	char const* m_buf{};
	size_t m_len{};
};

void help(char const* exe = nullptr);
void generate(
	std::initializer_list<std::string> msgs, stored::ProtocolLayer& top,
	stored::ProtocolLayer& bottom);
void generate(std::initializer_list<std::string> msgs);
void test(void const* buf, size_t len);
int test(char const* file);

// To be provided by the application.
void generate();
void test(Messages const& msgs);
extern char const* fuzz_name;

#endif // FUZZ_COMMON_H
