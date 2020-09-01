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
	EXPECT_EQ(ll.encoded().at(0), "123E");

	ll.encoded().clear();
	l.encode("", 0);
	EXPECT_EQ(ll.encoded().size(), 1);
	EXPECT_EQ(ll.encoded().at(0), "E");

	ll.encoded().clear();
	l.encode("1234567", 7);
	EXPECT_EQ(ll.encoded().size(), 1);
	EXPECT_EQ(ll.encoded().at(0), "1234567E");

	ll.encoded().clear();
	l.encode("1234", 4, false);
	l.encode("567", 3, true);
	EXPECT_EQ(ll.encoded().size(), 1);
	EXPECT_EQ(ll.encoded().at(0), "1234567E");

	ll.encoded().clear();
	l.encode("1234", 4, false);
	l.encode("567", 3, false);
	l.encode();
	EXPECT_EQ(ll.encoded().size(), 1);
	EXPECT_EQ(ll.encoded().at(0), "1234567E");
}

TEST(SegmentationLayer, MultiChunkEncode) {
	stored::SegmentationLayer l(4);
	LoggingLayer ll;
	ll.wrap(l);

	ll.encoded().clear();
	l.encode("1234", 4);
	EXPECT_EQ(ll.encoded().size(), 2);
	EXPECT_EQ(ll.encoded().at(0), "123C");
	EXPECT_EQ(ll.encoded().at(1), "4E");

	ll.encoded().clear();
	l.encode("12345", 5);
	EXPECT_EQ(ll.encoded().size(), 2);
	EXPECT_EQ(ll.encoded().at(0), "123C");
	EXPECT_EQ(ll.encoded().at(1), "45E");

	ll.encoded().clear();
	l.encode("1234567890", 10);
	EXPECT_EQ(ll.encoded().size(), 4);
	EXPECT_EQ(ll.encoded().at(0), "123C");
	EXPECT_EQ(ll.encoded().at(1), "456C");
	EXPECT_EQ(ll.encoded().at(2), "789C");
	EXPECT_EQ(ll.encoded().at(3), "0E");

	ll.encoded().clear();
	l.encode("12345", 5, false);
	l.encode("67", 2, false);
	l.encode("89", 2, false);
	l.encode();
	EXPECT_EQ(ll.encoded().size(), 3);
	EXPECT_EQ(ll.encoded().at(0), "123C");
	EXPECT_EQ(ll.encoded().at(1), "456C");
	EXPECT_EQ(ll.encoded().at(2), "789E");
}

TEST(SegmentationLayer, SingleChunkDecode) {
	LoggingLayer ll;
	stored::SegmentationLayer l(8);
	l.wrap(ll);

	ll.decoded().clear();
	{ char s[] = "123E"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(ll.decoded().size(), 1);
	EXPECT_EQ(ll.decoded().at(0), "123");

	ll.decoded().clear();
	{ char s[] = "E"; l.decode(s, sizeof(s) - 1); }
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
	{ char s[] = "12345E"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(ll.decoded().size(), 1);
	EXPECT_EQ(ll.decoded().at(0), "12345");

	ll.decoded().clear();
	{ char s[] = "1234567890E"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(ll.decoded().size(), 1);
	EXPECT_EQ(ll.decoded().at(0), "1234567890");

	ll.decoded().clear();
	{ char s[] = "123C"; l.decode(s, sizeof(s) - 1); }
	{ char s[] = "45E"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(ll.decoded().size(), 1);
	EXPECT_EQ(ll.decoded().at(0), "12345");

	ll.decoded().clear();
	{ char s[] = "123C"; l.decode(s, sizeof(s) - 1); }
	{ char s[] = "456789E"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(ll.decoded().size(), 1);
	EXPECT_EQ(ll.decoded().at(0), "123456789");

	ll.decoded().clear();
	{ char s[] = "123C"; l.decode(s, sizeof(s) - 1); }
	{ char s[] = "456789C"; l.decode(s, sizeof(s) - 1); }
	{ char s[] = "E"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(ll.decoded().size(), 1);
	EXPECT_EQ(ll.decoded().at(0), "123456789");
}

TEST(ArqLayer, SingleChunk) {
	LoggingLayer top;
	stored::ArqLayer l;
	l.wrap(top);
	LoggingLayer bottom;
	bottom.wrap(l);

	top.decoded().clear();
	bottom.encoded().clear();
	{ char s[] = "\x20""123"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(top.decoded().size(), 1);
	EXPECT_EQ(top.decoded().at(0), "123");
	top.encode("abc", 3);
	EXPECT_EQ(bottom.encoded().size(), 1);
	EXPECT_EQ(bottom.encoded().at(0), std::string("\x20""abc", 4));

	top.decoded().clear();
	bottom.encoded().clear();
	{ char s[] = "\x21""123"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(top.decoded().size(), 1);
	EXPECT_EQ(top.decoded().at(0), "123");
	top.encode("abc", 3);
	EXPECT_EQ(bottom.encoded().size(), 1);
	EXPECT_EQ(bottom.encoded().at(0), std::string("\x20""abc", 4));

	top.decoded().clear();
	bottom.encoded().clear();
	{ char s[] = "\x20""123"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(top.decoded().size(), 1);
	EXPECT_EQ(top.decoded().at(0), "123");
	top.encode("abc", 3);
	EXPECT_EQ(bottom.encoded().size(), 1);
	EXPECT_EQ(bottom.encoded().at(0), std::string("\x20""abc", 4));

	top.decoded().clear();
	bottom.encoded().clear();
	{ char s[] = "\x20"; l.decode(s, sizeof(s) - 1); }
	{ char s[] = "\x21""123"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(top.decoded().size(), 1);
	EXPECT_EQ(top.decoded().at(0), "123");
	top.encode("abc", 3, false);
	top.encode("def", 3);
	EXPECT_EQ(bottom.encoded().size(), 1);
	EXPECT_EQ(bottom.encoded().at(0), std::string("\x20""abcdef", 7));
}

TEST(ArqLayer, MultiChunk) {
	LoggingLayer top;
	stored::ArqLayer l;
	l.wrap(top);
	LoggingLayer bottom;
	bottom.wrap(l);

	top.decoded().clear();
	bottom.encoded().clear();
	{ char s[] = "\x20""123"; l.decode(s, sizeof(s) - 1); }
	{ char s[] = "\x21""456"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(top.decoded().size(), 2);
	EXPECT_EQ(top.decoded().at(0), "123");
	EXPECT_EQ(top.decoded().at(1), "456");
	top.encode("abc", 3);
	top.encode("defg", 4);
	EXPECT_EQ(bottom.encoded().size(), 2);
	EXPECT_TRUE(memcmp(bottom.encoded().at(0).data(), "\x20""abc", 4) == 0);
	EXPECT_EQ(bottom.encoded().at(1), "\x21""defg");

	top.decoded().clear();
	bottom.encoded().clear();
	{ char s[] = "\x22""123"; l.decode(s, sizeof(s) - 1); }
	{ char s[] = "\x23""456"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(top.decoded().size(), 2);
	EXPECT_EQ(top.decoded().at(0), "123");
	EXPECT_EQ(top.decoded().at(1), "456");
	top.encode("abc", 3, false);
	top.encode("defg", 4);
	top.encode("hi", 2);
	EXPECT_EQ(bottom.encoded().size(), 2);
	EXPECT_EQ(bottom.encoded().at(0), std::string("\x20""abcdefg", 8));
	EXPECT_EQ(bottom.encoded().at(1), "\x21""hi");
}

TEST(ArqLayer, LostRequest) {
	LoggingLayer top;
	stored::ArqLayer l;
	l.wrap(top);
	LoggingLayer bottom;
	bottom.wrap(l);

	top.decoded().clear();
	bottom.encoded().clear();
	{ char s[] = "\x20""123"; l.decode(s, sizeof(s) - 1); }
	{ char s[] = "\x21""456"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(top.decoded().size(), 2);
	EXPECT_EQ(top.decoded().at(0), "123");
	EXPECT_EQ(top.decoded().at(1), "456");
	// Assume last part is lost.
	// Retransmit random packets.
	{ char s[] = "\x21""456"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(top.decoded().size(), 2);
	{ char s[] = "\x23""zzz"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(top.decoded().size(), 2);
	{ char s[] = "\x42""..."; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(top.decoded().size(), 2);
	// Reset and retransmit full request
	{ char s[] = "\x20""123"; l.decode(s, sizeof(s) - 1); }
	{ char s[] = "\x21""456"; l.decode(s, sizeof(s) - 1); }
	{ char s[] = "\x22""567"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(top.decoded().size(), 5);
	EXPECT_EQ(top.decoded().at(2), "123");
	EXPECT_EQ(top.decoded().at(3), "456");
	EXPECT_EQ(top.decoded().at(4), "567");

	top.encode("abc", 3);
	EXPECT_EQ(bottom.encoded().size(), 1);
	EXPECT_EQ(bottom.encoded().at(0), std::string("\x20""abc", 4));

	top.decoded().clear();
	bottom.encoded().clear();
	{ char s[] = "\x20"; l.decode(s, sizeof(s) - 1); }
	{ char s[] = "\x21""123"; l.decode(s, sizeof(s) - 1); }
	{ char s[] = "\x22""456"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(top.decoded().size(), 2);
	EXPECT_EQ(top.decoded().at(0), "123");
	EXPECT_EQ(top.decoded().at(1), "456");
	// Do some retransmit
	{ char s[] = "\x22""456"; l.decode(s, sizeof(s) - 1); }
	{ char s[] = "\x32""zzz"; l.decode(s, sizeof(s) - 1); }
	{ char s[] = "\x21""123"; l.decode(s, sizeof(s) - 1); }
	{ char s[] = "\x22""456"; l.decode(s, sizeof(s) - 1); }
	{ char s[] = "\x23""567"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(top.decoded().size(), 3);
	EXPECT_EQ(top.decoded().at(2), "567");
}

TEST(ArqLayer, LostResponse) {
	LoggingLayer top;
	stored::ArqLayer l;
	l.wrap(top);
	LoggingLayer bottom;
	bottom.wrap(l);

	top.decoded().clear();
	bottom.encoded().clear();
	{ char s[] = "\x20"; l.decode(s, sizeof(s) - 1); }
	{ char s[] = "\x21""123"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(top.decoded().size(), 1);
	EXPECT_EQ(top.decoded().at(0), "123");

	top.encode("abc", 3);
	EXPECT_EQ(bottom.encoded().size(), 1);
	EXPECT_EQ(bottom.encoded().at(0), std::string("\x20""abc", 4));

	// Assume response was lost. Retransmit request.
	{ char s[] = "\x21""123"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(top.decoded().size(), 1);
	EXPECT_EQ(bottom.encoded().size(), 2);
	EXPECT_EQ(bottom.encoded().at(1), std::string("\x20""abc", 4));

	{ char s[] = "\x22""456"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(top.decoded().size(), 2);
	EXPECT_EQ(top.decoded().at(1), "456");
	top.encode("def", 3, false);
	top.encode("g", 1);
	top.encode("hi", 2);
	EXPECT_EQ(bottom.encoded().size(), 4);
	EXPECT_EQ(bottom.encoded().at(2), std::string("\x20""defg", 5));
	EXPECT_EQ(bottom.encoded().at(3), "\x21""hi");
	{ char s[] = "\x22""456"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(bottom.encoded().size(), 6);
	EXPECT_EQ(bottom.encoded().at(4), std::string("\x20""defg", 5));
	EXPECT_EQ(bottom.encoded().at(5), "\x21""hi");
}

TEST(ArqLayer, Purgeable) {
	LoggingLayer top;
	stored::ArqLayer l;
	l.wrap(top);
	LoggingLayer bottom;
	bottom.wrap(l);

	top.decoded().clear();
	bottom.encoded().clear();
	{ char s[] = "\x20"; l.decode(s, sizeof(s) - 1); }
	{ char s[] = "\x21""123"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(top.decoded().size(), 1);
	EXPECT_EQ(top.decoded().at(0), "123");

	top.setPurgeableResponse();
	top.encode("abc", 3);
	EXPECT_EQ(bottom.encoded().size(), 1);
	EXPECT_EQ(bottom.encoded().at(0), std::string("\x20""abc", 4));

	// Retransmit response, expect decoded again.
	{ char s[] = "\x21""123"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(top.decoded().size(), 2);
	EXPECT_EQ(top.decoded().at(1), "123");
	// Default to precious.
	top.encode("def", 3);
	EXPECT_EQ(bottom.encoded().size(), 2);
	EXPECT_EQ(bottom.encoded().at(1), std::string("\x20""def", 4));

	{ char s[] = "\x21""123"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(top.decoded().size(), 2);
	// Default to precious.
	EXPECT_EQ(bottom.encoded().size(), 3);
	EXPECT_EQ(bottom.encoded().at(2), std::string("\x20""def", 4));
}

TEST(ArqLayer, Overflow) {
	LoggingLayer top;
	stored::ArqLayer l(4);
	l.wrap(top);
	LoggingLayer bottom;
	bottom.wrap(l);

	top.decoded().clear();
	bottom.encoded().clear();
	{ char s[] = "\x20"; l.decode(s, sizeof(s) - 1); }
	{ char s[] = "\x21""123"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(top.decoded().size(), 1);
	top.encode("abcde", 5);
	EXPECT_EQ(bottom.encoded().size(), 1);
	EXPECT_EQ(bottom.encoded().at(0), std::string("\x20""abcde", 6));

	{ char s[] = "\x21""123"; l.decode(s, sizeof(s) - 1); }
	// Behave like purgeable.
	EXPECT_EQ(top.decoded().size(), 2);
	top.encode("fghij", 5);
	EXPECT_EQ(bottom.encoded().size(), 2);
	EXPECT_EQ(bottom.encoded().at(1), std::string("\x20""fghij", 6));

	{ char s[] = "\x22""456"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(top.decoded().size(), 3);
	top.encode("klm", 3);
	EXPECT_EQ(bottom.encoded().size(), 3);
	EXPECT_EQ(bottom.encoded().at(2), std::string("\x20""klm", 4));

	{ char s[] = "\x22""456"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(top.decoded().size(), 3);
	EXPECT_EQ(bottom.encoded().size(), 4);
	EXPECT_EQ(bottom.encoded().at(3), std::string("\x20""klm", 4));
}

TEST(CrcLayer, Encode) {
	stored::CrcLayer l;
	LoggingLayer ll;
	ll.wrap(l);

	ll.encoded().clear();
	l.encode();
	EXPECT_EQ(ll.encoded().size(), 1);
	EXPECT_EQ(ll.encoded().at(0), "\xff");

	ll.encoded().clear();
	l.encode("1", 1);
	EXPECT_EQ(ll.encoded().size(), 1);
	EXPECT_EQ(ll.encoded().at(0), "1\x5e");

	ll.encoded().clear();
	l.encode("12", 2);
	EXPECT_EQ(ll.encoded().size(), 1);
	EXPECT_EQ(ll.encoded().at(0), "12\x54");

	ll.encoded().clear();
	l.encode("123", 3);
	EXPECT_EQ(ll.encoded().size(), 1);
	EXPECT_EQ(ll.encoded().at(0), "123\xfc");
}

TEST(CrcLayer, Decode) {
	LoggingLayer ll;
	stored::CrcLayer l;
	l.wrap(ll);

	ll.decoded().clear();
	{ char s[] = "\xff"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(ll.decoded().size(), 1);
	EXPECT_EQ(ll.decoded().at(0), "");

	ll.decoded().clear();
	{ char s[] = "1\x5e"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(ll.decoded().size(), 1);
	EXPECT_EQ(ll.decoded().at(0), "1");

	ll.decoded().clear();
	{ char s[] = "12\x54"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(ll.decoded().size(), 1);
	EXPECT_EQ(ll.decoded().at(0), "12");

	ll.decoded().clear();
	{ char s[] = "123\xfc"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(ll.decoded().size(), 1);
	EXPECT_EQ(ll.decoded().at(0), "123");

	ll.decoded().clear();
	{ char s[] = "1234\xfc"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(ll.decoded().size(), 0);

	ll.decoded().clear();
	{ char s[] = "\x00""123\xfc"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(ll.decoded().size(), 0);
}




}
