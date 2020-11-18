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
#include "libstored/compress.h"
#include "gtest/gtest.h"
#include "LoggingLayer.h"

#include <fcntl.h>

#define DECODE(stack, str)	do { char msg_[] = "" str; (stack).decode(msg_, sizeof(msg_) - 1); } while(0)

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
	l.encode("\x7f""123\r", 5);
	EXPECT_EQ(ll.encoded().size(), 1);
	EXPECT_EQ(ll.encoded().at(0), "\x7f\x7f""123\x7f\x4d");
}

TEST(AsciiEscapeLayer, Decode) {
	stored::AsciiEscapeLayer l;
	LoggingLayer ll;
	l.wrap(ll);

	ll.decoded().clear();
	DECODE(l, "123\x7f""F");
	EXPECT_EQ(ll.decoded().size(), 1);
	EXPECT_EQ(ll.decoded().at(0), "123\x06");

	ll.decoded().clear();
	DECODE(l, "123\x7f");
	EXPECT_EQ(ll.decoded().size(), 1);
	EXPECT_EQ(ll.decoded().at(0), "123\x7f");

	ll.decoded().clear();
	DECODE(l, "\x7f""A12\r3");
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
	DECODE(l, "123E");
	EXPECT_EQ(ll.decoded().size(), 1);
	EXPECT_EQ(ll.decoded().at(0), "123");

	ll.decoded().clear();
	DECODE(l, "E");
	EXPECT_EQ(ll.decoded().size(), 1);
	EXPECT_EQ(ll.decoded().at(0), "");

	ll.decoded().clear();
	DECODE(l, "");
	EXPECT_EQ(ll.decoded().size(), 0);
}

TEST(SegmentationLayer, MultiChunkDecode) {
	LoggingLayer ll;
	stored::SegmentationLayer l(4);
	l.wrap(ll);

	ll.decoded().clear();
	DECODE(l, "12345E");
	EXPECT_EQ(ll.decoded().size(), 1);
	EXPECT_EQ(ll.decoded().at(0), "12345");

	ll.decoded().clear();
	DECODE(l, "1234567890E");
	EXPECT_EQ(ll.decoded().size(), 1);
	EXPECT_EQ(ll.decoded().at(0), "1234567890");

	ll.decoded().clear();
	DECODE(l, "123C");
	DECODE(l, "45E");
	EXPECT_EQ(ll.decoded().size(), 1);
	EXPECT_EQ(ll.decoded().at(0), "12345");

	ll.decoded().clear();
	DECODE(l, "123C");
	DECODE(l, "456789E");
	EXPECT_EQ(ll.decoded().size(), 1);
	EXPECT_EQ(ll.decoded().at(0), "123456789");

	ll.decoded().clear();
	DECODE(l, "123C");
	DECODE(l, "456789C");
	DECODE(l, "E");
	EXPECT_EQ(ll.decoded().size(), 1);
	EXPECT_EQ(ll.decoded().at(0), "123456789");
}

TEST(DebugArqLayer, SingleChunk) {
	LoggingLayer top;
	stored::DebugArqLayer l;
	l.wrap(top);
	LoggingLayer bottom;
	bottom.wrap(l);

	top.decoded().clear();
	bottom.encoded().clear();
	DECODE(l, "\x01""123");
	EXPECT_EQ(top.decoded().size(), 1);
	EXPECT_EQ(top.decoded().at(0), "123");
	top.encode("abc", 3);
	EXPECT_EQ(bottom.encoded().size(), 1);
	EXPECT_EQ(bottom.encoded().at(0), std::string("\x81""abc", 4));

	top.decoded().clear();
	bottom.encoded().clear();
	DECODE(l, "\x02""123");
	EXPECT_EQ(top.decoded().size(), 1);
	EXPECT_EQ(top.decoded().at(0), "123");
	top.encode("abc", 3);
	EXPECT_EQ(bottom.encoded().size(), 1);
	EXPECT_EQ(bottom.encoded().at(0), std::string("\x02""abc", 4));

	top.decoded().clear();
	bottom.encoded().clear();
	DECODE(l, "\x80");
	DECODE(l, "\x01""123");
	EXPECT_EQ(top.decoded().size(), 1);
	EXPECT_EQ(top.decoded().at(0), "123");
	top.encode("abc", 3);
	EXPECT_EQ(bottom.encoded().size(), 2);
	EXPECT_EQ(bottom.encoded().at(0), std::string("\x80", 1));
	EXPECT_EQ(bottom.encoded().at(1), std::string("\x01""abc", 4));

	top.decoded().clear();
	bottom.encoded().clear();
	DECODE(l, "\xc0\x12");
	DECODE(l, "\x40\x13""123");
	EXPECT_EQ(top.decoded().size(), 1);
	EXPECT_EQ(top.decoded().at(0), "123");
	top.encode("abc", 3, false);
	top.encode("def", 3);
	EXPECT_EQ(bottom.encoded().size(), 2);
	EXPECT_EQ(bottom.encoded().at(0), "\x80");
	EXPECT_EQ(bottom.encoded().at(1), "\x01""abcdef");
}

TEST(DebugArqLayer, MultiChunk) {
	LoggingLayer top;
	stored::DebugArqLayer l;
	l.wrap(top);
	LoggingLayer bottom;
	bottom.wrap(l);

	top.decoded().clear();
	bottom.encoded().clear();
	DECODE(l, "\x82");
	DECODE(l, "\x03""123");
	DECODE(l, "\x04""456");
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
	DECODE(l, "\x05""123");
	DECODE(l, "\x06""456");
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

TEST(DebugArqLayer, LostRequest) {
	LoggingLayer top;
	stored::DebugArqLayer l;
	l.wrap(top);
	LoggingLayer bottom;
	bottom.wrap(l);

	top.decoded().clear();
	bottom.encoded().clear();
	DECODE(l, "\x01""123");
	DECODE(l, "\x02""456");
	EXPECT_EQ(top.decoded().size(), 2);
	EXPECT_EQ(top.decoded().at(0), "123");
	EXPECT_EQ(top.decoded().at(1), "456");
	// Assume last part is lost.
	// Retransmit random packets.
	DECODE(l, "\x02""456");
	EXPECT_EQ(top.decoded().size(), 2);
	DECODE(l, "\x04""zzz");
	EXPECT_EQ(top.decoded().size(), 2);
	DECODE(l, "\x20""...");
	EXPECT_EQ(top.decoded().size(), 2);
	// Reset and retransmit full request
	DECODE(l, "\x80");
	DECODE(l, "\x01""123");
	DECODE(l, "\x02""456");
	DECODE(l, "\x03""789");
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
	DECODE(l, "\x80");
	EXPECT_EQ(bottom.encoded().size(), 1);
	EXPECT_EQ(bottom.encoded().at(0), "\x80");
	DECODE(l, "\x01""123");
	DECODE(l, "\x02""456");
	EXPECT_EQ(top.decoded().size(), 2);
	EXPECT_EQ(top.decoded().at(0), "123");
	EXPECT_EQ(top.decoded().at(1), "456");
	// Do some retransmit
	DECODE(l, "\x02""456");
	DECODE(l, "\x33""zzz");
	DECODE(l, "\x01""123");
	DECODE(l, "\x02""456");
	DECODE(l, "\x03""567");
	EXPECT_EQ(top.decoded().size(), 3);
	EXPECT_EQ(top.decoded().at(2), "567");
}

TEST(DebugArqLayer, LostResponse) {
	LoggingLayer top;
	stored::DebugArqLayer l;
	l.wrap(top);
	LoggingLayer bottom;
	bottom.wrap(l);

	top.decoded().clear();
	bottom.encoded().clear();
	DECODE(l, "\x8F");
	EXPECT_EQ(bottom.encoded().size(), 1);
	EXPECT_EQ(bottom.encoded().at(0), "\x80");
	bottom.encoded().clear();
	DECODE(l, "\x10""123");
	EXPECT_EQ(top.decoded().size(), 1);
	EXPECT_EQ(top.decoded().at(0), "123");

	top.encode("abc", 3);
	EXPECT_EQ(bottom.encoded().size(), 1);
	EXPECT_EQ(bottom.encoded().at(0), std::string("\x01""abc", 4));

	// Assume response was lost. Retransmit request.
	DECODE(l, "\x10""123");
	EXPECT_EQ(top.decoded().size(), 1);
	EXPECT_EQ(bottom.encoded().size(), 2);
	EXPECT_EQ(bottom.encoded().at(1), std::string("\x01""abc", 4));

	DECODE(l, "\x11""456");
	EXPECT_EQ(top.decoded().size(), 2);
	EXPECT_EQ(top.decoded().at(1), "456");
	top.encode("def", 3, false);
	top.encode("g", 1);
	top.encode("hi", 2);
	EXPECT_EQ(bottom.encoded().size(), 4);
	EXPECT_EQ(bottom.encoded().at(2), std::string("\x02""defg", 5));
	EXPECT_EQ(bottom.encoded().at(3), "\x03""hi");
	DECODE(l, "\x11""456");
	EXPECT_EQ(bottom.encoded().size(), 6);
	EXPECT_EQ(bottom.encoded().at(4), std::string("\x02""defg", 5));
	EXPECT_EQ(bottom.encoded().at(5), "\x03""hi");
}

TEST(DebugArqLayer, Purgeable) {
	LoggingLayer top;
	stored::DebugArqLayer l;
	l.wrap(top);
	LoggingLayer bottom;
	bottom.wrap(l);

	top.decoded().clear();
	bottom.encoded().clear();
	DECODE(l, "\x80");
	EXPECT_EQ(bottom.encoded().size(), 1);
	EXPECT_EQ(bottom.encoded().at(0), "\x80");
	bottom.encoded().clear();
	DECODE(l, "\x01""123");
	EXPECT_EQ(top.decoded().size(), 1);
	EXPECT_EQ(top.decoded().at(0), "123");

	top.setPurgeableResponse();
	top.encode("abc", 3);
	EXPECT_EQ(bottom.encoded().size(), 1);
	EXPECT_EQ(bottom.encoded().at(0), std::string("\x01""abc", 4));

	// Retransmit response, expect decoded again.
	DECODE(l, "\x01""123");
	EXPECT_EQ(top.decoded().size(), 2);
	EXPECT_EQ(top.decoded().at(1), "123");
	top.setPurgeableResponse();
	top.encode("def", 3);
	EXPECT_EQ(bottom.encoded().size(), 2);
	EXPECT_EQ(bottom.encoded().at(1), std::string("\x82""def", 4));

	// Retransmit response, expect decoded again.
	DECODE(l, "\x01""123");
	EXPECT_EQ(top.decoded().size(), 3);
	EXPECT_EQ(top.decoded().at(2), "123");
	// Default to precious, but reset flag remains.
	top.encode("ghi", 3);
	EXPECT_EQ(bottom.encoded().size(), 3);
	EXPECT_EQ(bottom.encoded().at(2), std::string("\x83""ghi", 4));

	DECODE(l, "\x01""123");
	EXPECT_EQ(top.decoded().size(), 3);
	// Default to precious, but reset flag remains.
	EXPECT_EQ(bottom.encoded().size(), 4);
	EXPECT_EQ(bottom.encoded().at(3), std::string("\x83""ghi", 4));

	DECODE(l, "\x02""123");
	EXPECT_EQ(top.decoded().size(), 4);
	// Default to precious.
	top.encode("jkl", 3);
	EXPECT_EQ(bottom.encoded().size(), 5);
	EXPECT_EQ(bottom.encoded().at(4), std::string("\x04""jkl", 4));
}

TEST(DebugArqLayer, Overflow) {
	LoggingLayer top;
	stored::DebugArqLayer l(4);
	l.wrap(top);
	LoggingLayer bottom;
	bottom.wrap(l);

	top.decoded().clear();
	bottom.encoded().clear();
	DECODE(l, "\x80");
	EXPECT_EQ(bottom.encoded().size(), 1);
	EXPECT_EQ(bottom.encoded().at(0), "\x80");
	bottom.encoded().clear();
	DECODE(l, "\x01""123");
	EXPECT_EQ(top.decoded().size(), 1);
	top.encode("abcde", 5);
	EXPECT_EQ(bottom.encoded().size(), 1);
	EXPECT_EQ(bottom.encoded().at(0), std::string("\x01""abcde", 6));

	DECODE(l, "\x01""123");
	// Behave like purgeable.
	EXPECT_EQ(top.decoded().size(), 2);
	top.encode("fghij", 5);
	EXPECT_EQ(bottom.encoded().size(), 2);
	EXPECT_EQ(bottom.encoded().at(1), std::string("\x82""fghij", 6));

	DECODE(l, "\x02""456");
	EXPECT_EQ(top.decoded().size(), 3);
	top.encode("klm", 3);
	EXPECT_EQ(bottom.encoded().size(), 3);
	EXPECT_EQ(bottom.encoded().at(2), std::string("\x03""klm", 4));

	DECODE(l, "\x02""456");
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
	DECODE(l, "\xff");
	EXPECT_EQ(ll.decoded().size(), 1);
	EXPECT_EQ(ll.decoded().at(0), "");

	ll.decoded().clear();
	DECODE(l, "1\x5e");
	EXPECT_EQ(ll.decoded().size(), 1);
	EXPECT_EQ(ll.decoded().at(0), "1");

	ll.decoded().clear();
	DECODE(l, "12\x54");
	EXPECT_EQ(ll.decoded().size(), 1);
	EXPECT_EQ(ll.decoded().at(0), "12");

	ll.decoded().clear();
	DECODE(l, "123\xfc");
	EXPECT_EQ(ll.decoded().size(), 1);
	EXPECT_EQ(ll.decoded().at(0), "123");

	ll.decoded().clear();
	DECODE(l, "1234\xfc");
	EXPECT_EQ(ll.decoded().size(), 0);

	ll.decoded().clear();
	DECODE(l, "\x00""123\xfc");
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
	DECODE(l, "\xff\xff");
	EXPECT_EQ(ll.decoded().size(), 1);
	EXPECT_EQ(ll.decoded().at(0), "");

	ll.decoded().clear();
	DECODE(l, "1\x49\xd6");
	EXPECT_EQ(ll.decoded().size(), 1);
	EXPECT_EQ(ll.decoded().at(0), "1");

	ll.decoded().clear();
	DECODE(l, "12\x77\xa2");
	EXPECT_EQ(ll.decoded().size(), 1);
	EXPECT_EQ(ll.decoded().at(0), "12");

	ll.decoded().clear();
	DECODE(l, "123\x1c\x84");
	EXPECT_EQ(ll.decoded().size(), 1);
	EXPECT_EQ(ll.decoded().at(0), "123");

	ll.decoded().clear();
	DECODE(l, "1234\x1c\x84");
	EXPECT_EQ(ll.decoded().size(), 0);

	ll.decoded().clear();
	DECODE(l, "\x00""123\x1c\x84");
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

TEST(ArqLayer, Normal) {
	LoggingLayer top;
	stored::ArqLayer l;
	l.wrap(top);
	LoggingLayer bottom;
	bottom.wrap(l);

	top.flush();

	EXPECT_EQ(bottom.encoded().at(0), "\x40");
	DECODE(bottom, "\x80\x40");
	EXPECT_EQ(bottom.encoded().at(1), "\x80");

	DECODE(bottom, "\x01 1");
	EXPECT_EQ(top.decoded().at(0), " 1");
	EXPECT_EQ(bottom.encoded().at(2), "\x81");

	DECODE(bottom, "\x02 2");
	EXPECT_EQ(top.decoded().at(1), " 2");
	EXPECT_EQ(bottom.encoded().at(3), "\x82");

	top.encode(" 3", 2);
	EXPECT_EQ(bottom.encoded().at(4), "\x01 3");

	DECODE(bottom, "\x81\x03 5");
	EXPECT_EQ(top.decoded().at(2), " 5");
	EXPECT_EQ(bottom.encoded().at(5), "\x83");

	top.encode(" 6", 2);
	EXPECT_EQ(bottom.encoded().at(6), "\x02 6");
}

TEST(ArqLayer, Retransmit) {
	LoggingLayer top;
	stored::ArqLayer l;
	l.wrap(top);
	LoggingLayer bottom;
	bottom.wrap(l);

	DECODE(bottom, "\xff");
	// 0xff is ignored
	//
	DECODE(bottom, "\x40");
	EXPECT_EQ(bottom.encoded().at(0), "\x80\x40"); // ack 0x40, and retransmit

	// retransmit
	DECODE(bottom, "\x40");
	EXPECT_EQ(bottom.encoded().at(1), "\x80"); // ack 0x40
	// No auto-retransmit

	top.flush();
	// retransmit
	EXPECT_EQ(bottom.encoded().at(2), "\x40");

	DECODE(bottom, "\x80");
	top.flush();
	// no retransmit
	EXPECT_EQ(bottom.encoded().size(), 3);

	top.clear();
	bottom.clear();

	top.encode(" 1", 2);
	EXPECT_EQ(bottom.encoded().at(0), "\x01 1");

	top.encode(" 2", 2); // triggers retransmit of 1
	EXPECT_EQ(bottom.encoded().at(1), "\x01 1");

	top.flush();
	// retransmit
	EXPECT_EQ(bottom.encoded().at(2), "\x01 1");

	DECODE(bottom, "\x81");
	EXPECT_EQ(bottom.encoded().at(3), "\x02 2");

	// Wrong ack
	DECODE(bottom, "\x83"); // ignored
	EXPECT_EQ(bottom.encoded().size(), 4);
	DECODE(bottom, "\x82");

	top.clear();
	bottom.clear();

	DECODE(bottom, "\x01 3");
	EXPECT_EQ(bottom.encoded().at(0), "\x81"); // assume lost

	DECODE(bottom, "\x01 3");
	EXPECT_EQ(bottom.encoded().at(1), "\x81");

	DECODE(bottom, "\x02 4");
	EXPECT_EQ(bottom.encoded().at(2), "\x82");
}

TEST(ArqLayer, KeepAlive) {
	LoggingLayer top;
	stored::ArqLayer l;
	l.wrap(top);
	LoggingLayer bottom;
	bottom.wrap(l);

	DECODE(bottom, "\x80\x40");
	l.flush();
	bottom.clear();

	// No queue, empty message
	l.keepAlive();
	EXPECT_EQ(bottom.encoded().at(0), "\x41");
	DECODE(bottom, "\x81");

	top.encode(" 1", 2);
	EXPECT_EQ(bottom.encoded().at(1), "\x02 1");

	l.keepAlive();
	EXPECT_EQ(bottom.encoded().at(2), "\x02 1"); // retransmit instead of empty message
	DECODE(bottom, "\x82");

	DECODE(bottom, "\x41");
	EXPECT_EQ(top.decoded().size(), 0);
	EXPECT_EQ(bottom.encoded().at(3), "\x81");
}

TEST(ArqLayer, Callback) {
	LoggingLayer top;
	stored::ArqLayer l(100);
	l.wrap(top);
	LoggingLayer bottom;
	bottom.wrap(l);

	stored::ArqLayer::Event event = stored::ArqLayer::EventNone;
	l.setEventCallback([&](stored::ArqLayer&, stored::ArqLayer::Event e) { event = e; });

	DECODE(bottom, "\x80\x40");
	top.encode(" 1", 2);
	DECODE(bottom, "\x01");
	bottom.clear();

	EXPECT_EQ(event, stored::ArqLayer::EventNone);
	DECODE(bottom, "\x40");
	EXPECT_EQ(event, stored::ArqLayer::EventReconnect);

	for(int i = 0; i <= 5; i++)
		top.encode("01234567890123456789", 20);

	EXPECT_EQ(event, stored::ArqLayer::EventEncodeBufferOverflow);

	for(int i = 0; i <= stored::ArqLayer::RetransmitCallbackThreshold; i++)
		top.flush();

	EXPECT_EQ(event, stored::ArqLayer::EventRetransmit);
}

TEST(CompressLayer, Compress) {
	LoggingLayer top;
	stored::CompressLayer l;
	l.wrap(top);
	LoggingLayer bottom;
	bottom.wrap(l);

	top.encode("Hello World! Nice World!", 24u);
	EXPECT_EQ(bottom.encoded().size(), 1u);
	std::string& msg = bottom.encoded().at(0);
	printBuffer(msg);
	EXPECT_LE(msg.size(), 24u);

	// std::string cannot be modified in place; copy needed.
	std::vector<char> buf(msg.begin(), msg.end());
	bottom.decode(&buf[0], buf.size());
	EXPECT_EQ(top.decoded().at(0), "Hello World! Nice World!");
}

#ifdef STORED_OS_WINDOWS
TEST(FileLayer, Pipe) {
	LoggingLayer top;
	stored::NamedPipeLayer l("test");
	l.wrap(top);

	EXPECT_EQ(l.lastError(), EAGAIN);

	int fd = open("\\\\.\\pipe\\test", O_RDWR);
	ASSERT_NE(fd, -1);
	write(fd, "hello", 5);
	EXPECT_EQ(l.recv(), 0);
	EXPECT_EQ(top.decoded().at(0), "hello");

	write(fd, " world", 6);
	EXPECT_EQ(l.recv(), 0);
	EXPECT_EQ(top.decoded().at(1), " "); // It blocked on one char.
	EXPECT_EQ(top.decoded().at(2), "world"); // The rest will follow.

	// Nothing to receive.
	EXPECT_EQ(l.recv(), EAGAIN);

	l.encode("Zip-a-Dee-Doo-Dah", 17);
	char buf[32] = {};
	EXPECT_EQ(read(fd, buf, sizeof(buf)), 17);
	EXPECT_EQ(std::string(buf), "Zip-a-Dee-Doo-Dah");

	close(fd);
	EXPECT_EQ(l.recv(), EIO);

	LoggingLayer ftop;
	stored::FileLayer f("\\\\.\\pipe\\test");
	f.wrap(ftop);
	f.encode("When You Wish", 13);
	l.recv();
	EXPECT_EQ(top.decoded().at(3), "When You Wish");

	l.encode(" Upon a Star", 12);
	f.recv();
	EXPECT_EQ(ftop.decoded().at(0), " ");
	EXPECT_EQ(ftop.decoded().at(1), "Upon a Star");
}
#endif

} // namespace
