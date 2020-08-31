#include "libstored/protocol.h"
#include "gtest/gtest.h"

#include <deque>

namespace {

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
		if(m_partial)
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

TEST(SegmentationLayer, SingleChunkEncode) {
	stored::SegmentationLayer l(8);
	LoggingLayer ll;
	ll.wrap(l);

	ll.encoded().clear();
	l.encode("123", 3);
	EXPECT_EQ(ll.encoded().size(), 1);
	EXPECT_EQ(ll.encoded().at(0), "123\x17");

	ll.encoded().clear();
	l.encode("", 0);
	EXPECT_EQ(ll.encoded().size(), 1);
	EXPECT_EQ(ll.encoded().at(0), "\x17");

	ll.encoded().clear();
	l.encode("1234567", 7);
	EXPECT_EQ(ll.encoded().size(), 1);
	EXPECT_EQ(ll.encoded().at(0), "1234567\x17");

	ll.encoded().clear();
	l.encode("1234", 4, false);
	l.encode("567", 3, true);
	EXPECT_EQ(ll.encoded().size(), 1);
	EXPECT_EQ(ll.encoded().at(0), "1234567\x17");

	ll.encoded().clear();
	l.encode("1234", 4, false);
	l.encode("567", 3, false);
	l.encode();
	EXPECT_EQ(ll.encoded().size(), 1);
	EXPECT_EQ(ll.encoded().at(0), "1234567\x17");
}

TEST(SegmentationLayer, MultiChunkEncode) {
	stored::SegmentationLayer l(4);
	LoggingLayer ll;
	ll.wrap(l);

	ll.encoded().clear();
	l.encode("1234", 4);
	EXPECT_EQ(ll.encoded().size(), 2);
	EXPECT_EQ(ll.encoded().at(0), "1234");
	EXPECT_EQ(ll.encoded().at(1), "\x17");

	ll.encoded().clear();
	l.encode("12345", 5);
	EXPECT_EQ(ll.encoded().size(), 2);
	EXPECT_EQ(ll.encoded().at(0), "1234");
	EXPECT_EQ(ll.encoded().at(1), "5\x17");

	ll.encoded().clear();
	l.encode("1234567890", 10);
	EXPECT_EQ(ll.encoded().size(), 3);
	EXPECT_EQ(ll.encoded().at(0), "1234");
	EXPECT_EQ(ll.encoded().at(1), "5678");
	EXPECT_EQ(ll.encoded().at(2), "90\x17");

	ll.encoded().clear();
	l.encode("12345", 5, false);
	l.encode("67", 2, false);
	l.encode("890", 3, false);
	l.encode();
	EXPECT_EQ(ll.encoded().size(), 3);
	EXPECT_EQ(ll.encoded().at(0), "1234");
	EXPECT_EQ(ll.encoded().at(1), "5678");
	EXPECT_EQ(ll.encoded().at(2), "90\x17");
}

TEST(SegmentationLayer, SingleChunkDecode) {
	LoggingLayer ll;
	stored::SegmentationLayer l(8);
	l.wrap(ll);

	ll.decoded().clear();
	{ char s[] = "123\x17"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(ll.decoded().size(), 1);
	EXPECT_EQ(ll.decoded().at(0), "123");

	ll.decoded().clear();
	{ char s[] = "\x17"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(ll.decoded().size(), 1);
	EXPECT_EQ(ll.decoded().at(0), "");

	ll.decoded().clear();
	{ char s[] = ""; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(ll.decoded().size(), 0);
}

TEST(SegmentationLayer, MultiChunkDecode) {
	LoggingLayer ll;
	stored::SegmentationLayer l(4);
	l.wrap(ll);

	ll.decoded().clear();
	{ char s[] = "12345\x17"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(ll.decoded().size(), 1);
	EXPECT_EQ(ll.decoded().at(0), "12345");

	ll.decoded().clear();
	{ char s[] = "1234567890\x17"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(ll.decoded().size(), 1);
	EXPECT_EQ(ll.decoded().at(0), "1234567890");

	ll.decoded().clear();
	{ char s[] = "123"; l.decode(s, sizeof(s) - 1); }
	{ char s[] = "45\x17"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(ll.decoded().size(), 1);
	EXPECT_EQ(ll.decoded().at(0), "12345");

	ll.decoded().clear();
	{ char s[] = "123"; l.decode(s, sizeof(s) - 1); }
	{ char s[] = "456789\x17"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(ll.decoded().size(), 1);
	EXPECT_EQ(ll.decoded().at(0), "123456789");

	ll.decoded().clear();
	{ char s[] = "123"; l.decode(s, sizeof(s) - 1); }
	{ char s[] = "456789"; l.decode(s, sizeof(s) - 1); }
	{ char s[] = "\x17"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(ll.decoded().size(), 1);
	EXPECT_EQ(ll.decoded().at(0), "123456789");
}

TEST(ArqLayer, SingleChunk) {
	LoggingLayer top;
	stored::ArqLayer l(4);
	l.wrap(top);
	LoggingLayer bottom;
	bottom.wrap(l);

	top.decoded().clear();
	bottom.encoded().clear();
	{ char s[] = "\x00""123"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(top.decoded().size(), 1);
	EXPECT_EQ(top.decoded().at(0), "123");
	top.encode("abc", 3);
	EXPECT_EQ(bottom.encoded().size(), 1);
	EXPECT_TRUE(memcmp(bottom.encoded().at(0).data(), "\x00""abc", 4) == 0);

	top.decoded().clear();
	bottom.encoded().clear();
	{ char s[] = "\x01""123"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(top.decoded().size(), 1);
	EXPECT_EQ(top.decoded().at(0), "123");
	top.encode("abc", 3);
	EXPECT_EQ(bottom.encoded().size(), 1);
	EXPECT_EQ(bottom.encoded().at(0), "\x01""abc");

	top.decoded().clear();
	bottom.encoded().clear();
	{ char s[] = "\x00""123"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(top.decoded().size(), 1);
	EXPECT_EQ(top.decoded().at(0), "123");
	top.encode("abc", 3);
	EXPECT_EQ(bottom.encoded().size(), 1);
	EXPECT_TRUE(memcmp(bottom.encoded().at(0).data(), "\x00""abc", 4) == 0);

	top.decoded().clear();
	bottom.encoded().clear();
	{ char s[] = "\x00"; l.decode(s, sizeof(s) - 1); }
	{ char s[] = "\x01""123"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(top.decoded().size(), 1);
	EXPECT_EQ(top.decoded().at(0), "123");
	top.encode("abc", 3);
	EXPECT_EQ(bottom.encoded().size(), 1);
	EXPECT_TRUE(memcmp(bottom.encoded().at(0).data(), "\x00""abc", 4) == 0);
}

TEST(ArqLayer, MultiChunk) {
	LoggingLayer top;
	stored::ArqLayer l(4);
	l.wrap(top);
	LoggingLayer bottom;
	bottom.wrap(l);

	top.decoded().clear();
	bottom.encoded().clear();
	{ char s[] = "\x00""123"; l.decode(s, sizeof(s) - 1); }
	{ char s[] = "\x01""456"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(top.decoded().size(), 2);
	EXPECT_EQ(top.decoded().at(0), "123");
	EXPECT_EQ(top.decoded().at(1), "456");
	top.encode("abc", 3);
	top.encode("defg", 4);
	EXPECT_EQ(bottom.encoded().size(), 2);
	EXPECT_TRUE(memcmp(bottom.encoded().at(0).data(), "\x00""abc", 4) == 0);
	EXPECT_EQ(bottom.encoded().at(1), "\x01""defg");

	top.decoded().clear();
	bottom.encoded().clear();
	{ char s[] = "\x02""123"; l.decode(s, sizeof(s) - 1); }
	{ char s[] = "\x03""456"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(top.decoded().size(), 2);
	EXPECT_EQ(top.decoded().at(0), "123");
	EXPECT_EQ(top.decoded().at(1), "456");
	top.encode("abc", 3, false);
	top.encode("defg", 4);
	top.encode("hi", 2);
	EXPECT_EQ(bottom.encoded().size(), 3);
	EXPECT_EQ(bottom.encoded().at(0), "\x02""abc");
	EXPECT_EQ(bottom.encoded().at(1), "defg");
	EXPECT_EQ(bottom.encoded().at(2), "\x03""hi");
}

}
