#include "libstored/protocol.h"
#include "gtest/gtest.h"
#include "LoggingLayer.h"

namespace {

TEST(AsciiEscapeLayer, Encode) {
	stored::AsciiEscapeLayer l;
	LoggingLayer ll;
	ll.wrap(l);

	ll.encoded().clear();
	l.encode("123", 3);
	EXPECT_EQ(ll.encoded().size(), 1);
	EXPECT_EQ(ll.encoded().at(0), "123");

	ll.encoded().clear();
	l.encode("123\x00", 4);
	EXPECT_EQ(ll.encoded().size(), 1);
	EXPECT_EQ(ll.encoded().at(0), "123\x7f@");

	ll.encoded().clear();
	l.encode("123\r""4", 5);
	EXPECT_EQ(ll.encoded().size(), 1);
	EXPECT_EQ(ll.encoded().at(0), "123\x7f""M4");

	ll.encoded().clear();
	l.encode("123\x7f", 4);
	EXPECT_EQ(ll.encoded().size(), 1);
	EXPECT_EQ(ll.encoded().at(0), "123\x7f\x7f");

	ll.encoded().clear();
	l.encode("\x7f""123", 4);
	EXPECT_EQ(ll.encoded().size(), 1);
	EXPECT_EQ(ll.encoded().at(0), "\x7f\x7f""123");
}

TEST(AsciiEscapeLayer, Decode) {
	stored::AsciiEscapeLayer l;
	LoggingLayer ll;
	l.wrap(ll);

	ll.decoded().clear();
	{ char s[] = "123\x7f""F"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(ll.decoded().size(), 1);
	EXPECT_EQ(ll.decoded().at(0), "123\x06");

	ll.decoded().clear();
	{ char s[] = "123\x7f"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(ll.decoded().size(), 1);
	EXPECT_EQ(ll.decoded().at(0), "123\x7f");

	ll.decoded().clear();
	{ char s[] = "\x7f""A123"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(ll.decoded().size(), 1);
	EXPECT_EQ(ll.decoded().at(0), "\x01""123");
}

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
	{ char s[] = "\x01""123"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(top.decoded().size(), 1);
	EXPECT_EQ(top.decoded().at(0), "123");
	top.encode("abc", 3);
	EXPECT_EQ(bottom.encoded().size(), 1);
	EXPECT_EQ(bottom.encoded().at(0), std::string("\x81""abc", 4));

	top.decoded().clear();
	bottom.encoded().clear();
	{ char s[] = "\x02""123"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(top.decoded().size(), 1);
	EXPECT_EQ(top.decoded().at(0), "123");
	top.encode("abc", 3);
	EXPECT_EQ(bottom.encoded().size(), 1);
	EXPECT_EQ(bottom.encoded().at(0), std::string("\x02""abc", 4));

	top.decoded().clear();
	bottom.encoded().clear();
	{ char s[] = "\x80"; l.decode(s, sizeof(s) - 1); }
	{ char s[] = "\x01""123"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(top.decoded().size(), 1);
	EXPECT_EQ(top.decoded().at(0), "123");
	top.encode("abc", 3);
	EXPECT_EQ(bottom.encoded().size(), 2);
	EXPECT_EQ(bottom.encoded().at(0), std::string("\x80", 1));
	EXPECT_EQ(bottom.encoded().at(1), std::string("\x01""abc", 4));

	top.decoded().clear();
	bottom.encoded().clear();
	{ char s[] = "\xc0\x12"; l.decode(s, sizeof(s) - 1); }
	{ char s[] = "\x40\x13""123"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(top.decoded().size(), 1);
	EXPECT_EQ(top.decoded().at(0), "123");
	top.encode("abc", 3, false);
	top.encode("def", 3);
	EXPECT_EQ(bottom.encoded().size(), 2);
	EXPECT_EQ(bottom.encoded().at(0), "\x80");
	EXPECT_EQ(bottom.encoded().at(1), "\x01""abcdef");
}

TEST(ArqLayer, MultiChunk) {
	LoggingLayer top;
	stored::ArqLayer l;
	l.wrap(top);
	LoggingLayer bottom;
	bottom.wrap(l);

	top.decoded().clear();
	bottom.encoded().clear();
	{ char s[] = "\x82"; l.decode(s, sizeof(s) - 1); }
	{ char s[] = "\x03""123"; l.decode(s, sizeof(s) - 1); }
	{ char s[] = "\x04""456"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(top.decoded().size(), 2);
	EXPECT_EQ(top.decoded().at(0), "123");
	EXPECT_EQ(top.decoded().at(1), "456");
	top.encode("abc", 3);
	top.encode("defg", 4);
	EXPECT_EQ(bottom.encoded().size(), 3);
	EXPECT_EQ(bottom.encoded().at(0), "\x80");
	EXPECT_EQ(bottom.encoded().at(1), "\x01""abc");
	EXPECT_EQ(bottom.encoded().at(2), "\x02""defg");

	top.decoded().clear();
	bottom.encoded().clear();
	{ char s[] = "\x05""123"; l.decode(s, sizeof(s) - 1); }
	{ char s[] = "\x06""456"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(top.decoded().size(), 2);
	EXPECT_EQ(top.decoded().at(0), "123");
	EXPECT_EQ(top.decoded().at(1), "456");
	top.encode("abc", 3, false);
	top.encode("defg", 4);
	top.encode("hi", 2);
	EXPECT_EQ(bottom.encoded().size(), 2);
	EXPECT_EQ(bottom.encoded().at(0), std::string("\x03""abcdefg", 8));
	EXPECT_EQ(bottom.encoded().at(1), "\x04""hi");
}

TEST(ArqLayer, LostRequest) {
	LoggingLayer top;
	stored::ArqLayer l;
	l.wrap(top);
	LoggingLayer bottom;
	bottom.wrap(l);

	top.decoded().clear();
	bottom.encoded().clear();
	{ char s[] = "\x01""123"; l.decode(s, sizeof(s) - 1); }
	{ char s[] = "\x02""456"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(top.decoded().size(), 2);
	EXPECT_EQ(top.decoded().at(0), "123");
	EXPECT_EQ(top.decoded().at(1), "456");
	// Assume last part is lost.
	// Retransmit random packets.
	{ char s[] = "\x02""456"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(top.decoded().size(), 2);
	{ char s[] = "\x04""zzz"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(top.decoded().size(), 2);
	{ char s[] = "\x20""..."; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(top.decoded().size(), 2);
	// Reset and retransmit full request
	{ char s[] = "\x80"; l.decode(s, sizeof(s) - 1); }
	{ char s[] = "\x01""123"; l.decode(s, sizeof(s) - 1); }
	{ char s[] = "\x02""456"; l.decode(s, sizeof(s) - 1); }
	{ char s[] = "\x03""789"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(top.decoded().size(), 5);
	EXPECT_EQ(top.decoded().at(2), "123");
	EXPECT_EQ(top.decoded().at(3), "456");
	EXPECT_EQ(top.decoded().at(4), "789");

	top.encode("abc", 3);
	EXPECT_EQ(bottom.encoded().size(), 2);
	EXPECT_EQ(bottom.encoded().at(0), "\x80");
	EXPECT_EQ(bottom.encoded().at(1), std::string("\x01""abc", 4));

	top.decoded().clear();
	bottom.encoded().clear();
	{ char s[] = "\x80"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(bottom.encoded().size(), 1);
	EXPECT_EQ(bottom.encoded().at(0), "\x80");
	{ char s[] = "\x01""123"; l.decode(s, sizeof(s) - 1); }
	{ char s[] = "\x02""456"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(top.decoded().size(), 2);
	EXPECT_EQ(top.decoded().at(0), "123");
	EXPECT_EQ(top.decoded().at(1), "456");
	// Do some retransmit
	{ char s[] = "\x02""456"; l.decode(s, sizeof(s) - 1); }
	{ char s[] = "\x33""zzz"; l.decode(s, sizeof(s) - 1); }
	{ char s[] = "\x01""123"; l.decode(s, sizeof(s) - 1); }
	{ char s[] = "\x02""456"; l.decode(s, sizeof(s) - 1); }
	{ char s[] = "\x03""567"; l.decode(s, sizeof(s) - 1); }
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
	{ char s[] = "\x8F"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(bottom.encoded().size(), 1);
	EXPECT_EQ(bottom.encoded().at(0), "\x80");
	bottom.encoded().clear();
	{ char s[] = "\x10""123"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(top.decoded().size(), 1);
	EXPECT_EQ(top.decoded().at(0), "123");

	top.encode("abc", 3);
	EXPECT_EQ(bottom.encoded().size(), 1);
	EXPECT_EQ(bottom.encoded().at(0), std::string("\x01""abc", 4));

	// Assume response was lost. Retransmit request.
	{ char s[] = "\x10""123"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(top.decoded().size(), 1);
	EXPECT_EQ(bottom.encoded().size(), 2);
	EXPECT_EQ(bottom.encoded().at(1), std::string("\x01""abc", 4));

	{ char s[] = "\x11""456"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(top.decoded().size(), 2);
	EXPECT_EQ(top.decoded().at(1), "456");
	top.encode("def", 3, false);
	top.encode("g", 1);
	top.encode("hi", 2);
	EXPECT_EQ(bottom.encoded().size(), 4);
	EXPECT_EQ(bottom.encoded().at(2), std::string("\x02""defg", 5));
	EXPECT_EQ(bottom.encoded().at(3), "\x03""hi");
	{ char s[] = "\x11""456"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(bottom.encoded().size(), 6);
	EXPECT_EQ(bottom.encoded().at(4), std::string("\x02""defg", 5));
	EXPECT_EQ(bottom.encoded().at(5), "\x03""hi");
}

TEST(ArqLayer, Purgeable) {
	LoggingLayer top;
	stored::ArqLayer l;
	l.wrap(top);
	LoggingLayer bottom;
	bottom.wrap(l);

	top.decoded().clear();
	bottom.encoded().clear();
	{ char s[] = "\x80"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(bottom.encoded().size(), 1);
	EXPECT_EQ(bottom.encoded().at(0), "\x80");
	bottom.encoded().clear();
	{ char s[] = "\x01""123"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(top.decoded().size(), 1);
	EXPECT_EQ(top.decoded().at(0), "123");

	top.setPurgeableResponse();
	top.encode("abc", 3);
	EXPECT_EQ(bottom.encoded().size(), 1);
	EXPECT_EQ(bottom.encoded().at(0), std::string("\x01""abc", 4));

	// Retransmit response, expect decoded again.
	{ char s[] = "\x01""123"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(top.decoded().size(), 2);
	EXPECT_EQ(top.decoded().at(1), "123");
	top.setPurgeableResponse();
	top.encode("def", 3);
	EXPECT_EQ(bottom.encoded().size(), 2);
	EXPECT_EQ(bottom.encoded().at(1), std::string("\x82""def", 4));

	// Retransmit response, expect decoded again.
	{ char s[] = "\x01""123"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(top.decoded().size(), 3);
	EXPECT_EQ(top.decoded().at(2), "123");
	// Default to precious, but reset flag remains.
	top.encode("ghi", 3);
	EXPECT_EQ(bottom.encoded().size(), 3);
	EXPECT_EQ(bottom.encoded().at(2), std::string("\x83""ghi", 4));

	{ char s[] = "\x01""123"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(top.decoded().size(), 3);
	// Default to precious, but reset flag remains.
	EXPECT_EQ(bottom.encoded().size(), 4);
	EXPECT_EQ(bottom.encoded().at(3), std::string("\x83""ghi", 4));

	{ char s[] = "\x02""123"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(top.decoded().size(), 4);
	// Default to precious.
	top.encode("jkl", 3);
	EXPECT_EQ(bottom.encoded().size(), 5);
	EXPECT_EQ(bottom.encoded().at(4), std::string("\x04""jkl", 4));
}

TEST(ArqLayer, Overflow) {
	LoggingLayer top;
	stored::ArqLayer l(4);
	l.wrap(top);
	LoggingLayer bottom;
	bottom.wrap(l);

	top.decoded().clear();
	bottom.encoded().clear();
	{ char s[] = "\x80"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(bottom.encoded().size(), 1);
	EXPECT_EQ(bottom.encoded().at(0), "\x80");
	bottom.encoded().clear();
	{ char s[] = "\x01""123"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(top.decoded().size(), 1);
	top.encode("abcde", 5);
	EXPECT_EQ(bottom.encoded().size(), 1);
	EXPECT_EQ(bottom.encoded().at(0), std::string("\x01""abcde", 6));

	{ char s[] = "\x01""123"; l.decode(s, sizeof(s) - 1); }
	// Behave like purgeable.
	EXPECT_EQ(top.decoded().size(), 2);
	top.encode("fghij", 5);
	EXPECT_EQ(bottom.encoded().size(), 2);
	EXPECT_EQ(bottom.encoded().at(1), std::string("\x82""fghij", 6));

	{ char s[] = "\x02""456"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(top.decoded().size(), 3);
	top.encode("klm", 3);
	EXPECT_EQ(bottom.encoded().size(), 3);
	EXPECT_EQ(bottom.encoded().at(2), std::string("\x03""klm", 4));

	{ char s[] = "\x02""456"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(top.decoded().size(), 3);
	EXPECT_EQ(bottom.encoded().size(), 4);
	EXPECT_EQ(bottom.encoded().at(3), std::string("\x03""klm", 4));
}

TEST(Crc8Layer, Encode) {
	stored::Crc8Layer l;
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

TEST(Crc8Layer, Decode) {
	LoggingLayer ll;
	stored::Crc8Layer l;
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

TEST(Crc16Layer, Encode) {
	stored::Crc16Layer l;
	LoggingLayer ll;
	ll.wrap(l);

	ll.encoded().clear();
	l.encode();
	EXPECT_EQ(ll.encoded().size(), 1);
	EXPECT_EQ(ll.encoded().at(0), "\xff\xff");

	ll.encoded().clear();
	l.encode("1", 1);
	EXPECT_EQ(ll.encoded().size(), 1);
	EXPECT_EQ(ll.encoded().at(0), "1\x49\xD6");

	ll.encoded().clear();
	l.encode("12", 2);
	EXPECT_EQ(ll.encoded().size(), 1);
	EXPECT_EQ(ll.encoded().at(0), "12\x77\xA2");

	ll.encoded().clear();
	l.encode("123", 3);
	EXPECT_EQ(ll.encoded().size(), 1);
	EXPECT_EQ(ll.encoded().at(0), "123\x1C\x84");
}

TEST(Crc16Layer, Decode) {
	LoggingLayer ll;
	stored::Crc16Layer l;
	l.wrap(ll);

	ll.decoded().clear();
	{ char s[] = "\xff\xff"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(ll.decoded().size(), 1);
	EXPECT_EQ(ll.decoded().at(0), "");

	ll.decoded().clear();
	{ char s[] = "1\x49\xd6"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(ll.decoded().size(), 1);
	EXPECT_EQ(ll.decoded().at(0), "1");

	ll.decoded().clear();
	{ char s[] = "12\x77\xa2"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(ll.decoded().size(), 1);
	EXPECT_EQ(ll.decoded().at(0), "12");

	ll.decoded().clear();
	{ char s[] = "123\x1c\x84"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(ll.decoded().size(), 1);
	EXPECT_EQ(ll.decoded().at(0), "123");

	ll.decoded().clear();
	{ char s[] = "1234\x1c\x84"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(ll.decoded().size(), 0);

	ll.decoded().clear();
	{ char s[] = "\x00""123\x1c\x84"; l.decode(s, sizeof(s) - 1); }
	EXPECT_EQ(ll.decoded().size(), 0);
}

TEST(BufferLayer, Encode) {
	stored::BufferLayer l(4);
	LoggingLayer ll;
	ll.wrap(l);

	ll.encoded().clear();
	l.encode("123", 3);
	EXPECT_EQ(ll.encoded().size(), 1);
	EXPECT_EQ(ll.encoded().at(0), "123");

	ll.encoded().clear();
	l.encode("12", 2, false);
	l.encode("3", 1);
	EXPECT_EQ(ll.encoded().size(), 1);
	EXPECT_EQ(ll.encoded().at(0), "123");

	ll.encoded().clear();
	l.encode("12", 2, false);
	l.encode("3", 1, false);
	l.encode();
	EXPECT_EQ(ll.encoded().size(), 1);
	EXPECT_EQ(ll.encoded().at(0), "123");

	ll.encoded().clear();
	l.encode("1234", 4, true);
	EXPECT_EQ(ll.encoded().size(), 1);
	EXPECT_EQ(ll.encoded().at(0), "1234");

	ll.encoded().clear();
	l.encode("1234", 4, false);
	l.encode();
	EXPECT_EQ(ll.encoded().size(), 1);
	EXPECT_EQ(ll.encoded().at(0), "1234");

	ll.encoded().clear();
	l.encode("12345", 5, true);
	EXPECT_EQ(ll.encoded().size(), 1);
	EXPECT_EQ(ll.encoded().at(0), "12345");

	ll.encoded().clear();
	l.encode("12345", 5, false);
	l.encode("67", 2, true);
	EXPECT_EQ(ll.encoded().size(), 1);
	EXPECT_EQ(ll.encoded().at(0), "1234567");

	ll.encoded().clear();
	l.encode("1234567890", 10, true);
	EXPECT_EQ(ll.encoded().size(), 1);
	EXPECT_EQ(ll.encoded().at(0), "1234567890");
}



}
