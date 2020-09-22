#ifndef TESTS_LOGGING_LAYER_H
#define TESTS_LOGGING_LAYER_H

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

#endif // __cplusplus
#endif // TESTS_LOGGING_LAYER_H
