/*
 * libstored, distributed debuggable data stores.
 * Copyright (C) 2020-2023  Jochem Rutgers
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "TestStore.h"
#include "gtest/gtest.h"

#include "libstored/debugger.h"

#include "LoggingLayer.h"

#define DECODE(stack, str)	do { char msg_[] = "" str; (stack).decode(msg_, sizeof(msg_) - 1); } while(0)

namespace {

TEST(Debugger, Capabilities)
{
	stored::Debugger d;
	LoggingLayer ll;
	ll.wrap(d);

	DECODE(d, "?");
	EXPECT_GT(ll.encoded().at(0).size(), 1);
}

TEST(Debugger, Identification)
{
	stored::Debugger d;
	LoggingLayer ll;
	ll.wrap(d);

	EXPECT_EQ(d.identification(), nullptr);
	DECODE(d, "i");
	EXPECT_EQ(ll.encoded().at(0), "?");

	d.setIdentification("asdf");
	EXPECT_EQ(strcmp(d.identification(), "asdf"), 0);

	DECODE(d, "i");
	EXPECT_EQ(ll.encoded().at(1), "asdf");
}

TEST(Debugger, Version)
{
	stored::Debugger d;
	LoggingLayer ll;

	EXPECT_TRUE(d.version(ll));

	if(stored::Config::Debug)
		EXPECT_EQ(ll.encoded().at(0), "2 debug");
	else
		EXPECT_EQ(ll.encoded().at(0), "2");

	ll.encoded().clear();
	d.setVersions("baab");
	EXPECT_TRUE(d.version(ll));

	if(stored::Config::Debug)
		EXPECT_EQ(ll.encoded().at(0), "2 baab debug");
	else
		EXPECT_EQ(ll.encoded().at(0), "2 baab");

	ll.encoded().clear();
	ll.wrap(d);
	DECODE(d, "v");

	if(stored::Config::Debug)
		EXPECT_EQ(ll.encoded().at(0), "2 baab debug");
	else
		EXPECT_EQ(ll.encoded().at(0), "2 baab");
}

TEST(Debugger, Find)
{
	stored::Debugger d;
	stored::TestStore store;
	d.map(store);

	EXPECT_TRUE(d.find("/default int8").valid());
	EXPECT_FALSE(d.find("/default int").valid());
	EXPECT_FALSE(d.find("/default int8", 6).valid());
	EXPECT_TRUE(d.find("/sc/inner b").valid());
}

TEST(Debugger, List)
{
	stored::Debugger d;
	stored::TestStore store;
	d.map(store);

	std::list<std::string> names;
	d.list([&](char const* name, stored::DebugVariant&) { names.push_back(name); });

	// We should find something.
	EXPECT_GT(names.size(), 10);

	// Check a few names.
	EXPECT_TRUE(std::find(names.begin(), names.end(), "/default int8") != names.end());
	EXPECT_TRUE(std::find(names.begin(), names.end(), "/f read/write") != names.end());
}

TEST(Debugger, FindMulti)
{
	stored::Debugger d;
	stored::TestStore store1;
	stored::TestStore store2;

	// Invalid mapping
	d.map(store1, "first");
	EXPECT_FALSE(d.find("/default int8").valid());
	d.map(store1, "/fir/st");
	EXPECT_FALSE(d.find("/default int8").valid());

	d.map(store1, "/first");
	auto v1 = d.find("/default int8");
	auto v2 = d.find("/first/default int8");
	EXPECT_TRUE(v1.valid());
	EXPECT_TRUE(v2.valid());
	EXPECT_EQ(v1, v2);
	EXPECT_TRUE(d.find("/f/sc/inner b").valid());
	EXPECT_TRUE(d.find("/asdf/default int8").valid());

	d.map(store2, "/second");
	EXPECT_FALSE(d.find("/default int8").valid());
	auto v3 = d.find("/first/default int8");
	auto v4 = d.find("/second/default int8");
	EXPECT_TRUE(v3.valid());
	EXPECT_TRUE(v4.valid());
	EXPECT_NE(v3, v4);
	v3 = d.find("/f/default int8");
	v4 = d.find("/s/default int8");
	EXPECT_TRUE(v3.valid());
	EXPECT_TRUE(v4.valid());
	EXPECT_NE(v3, v4);
	EXPECT_FALSE(d.find("/asdf/sc/inner b").valid());
}

TEST(Debugger, ListMulti)
{
	stored::Debugger d;
	stored::TestStore store1;
	stored::TestStore store2;
	d.map(store1, "/first");
	d.map(store2, "/second");

	std::list<std::string> names;
	d.list([&](char const* name, stored::DebugVariant&) { names.push_back(name); });

	// We should find something.
	EXPECT_GT(names.size(), 10);

	// Check a few names.
	EXPECT_TRUE(std::find(names.begin(), names.end(), "/first/default int8") != names.end());
	EXPECT_TRUE(std::find(names.begin(), names.end(), "/second/f read/write") != names.end());

	LoggingLayer ll;
	ll.wrap(d);
	DECODE(d, "l");
	// We are not going to check all encoded values...
	EXPECT_NE(ll.encoded().at(0), "?");
}

TEST(Debugger, Read)
{
	stored::Debugger d;
	stored::TestStore store;
	d.map(store);
	LoggingLayer ll;
	ll.wrap(d);

	DECODE(d, "r/default int8");
	EXPECT_EQ(ll.encoded().at(0), "0");
	DECODE(d, "r/init decimal");
	EXPECT_EQ(ll.encoded().at(1), "2a");
}

TEST(Debugger, Write)
{
	stored::Debugger d;
	stored::TestStore store;
	d.map(store);
	LoggingLayer ll;
	ll.wrap(d);

	DECODE(d, "w10/default int8");
	EXPECT_EQ(ll.encoded().at(0), "!");
	EXPECT_EQ(store.default_int8.get(), 0x10);
}

TEST(Debugger, Echo)
{
	stored::Debugger d;
	LoggingLayer ll;
	ll.wrap(d);

	DECODE(d, "e123");
	EXPECT_EQ(ll.encoded().at(0), "123");

	DECODE(d, "e");
	EXPECT_EQ(ll.encoded().at(1), "");
}

TEST(Debugger, Alias)
{
	stored::Debugger d;
	stored::TestStore store;
	d.map(store);
	LoggingLayer ll;
	ll.wrap(d);

	DECODE(d, "aa/default int8");
	EXPECT_EQ(ll.encoded().at(0), "!");
	DECODE(d, "w11a");
	EXPECT_EQ(ll.encoded().at(1), "!");
	EXPECT_EQ(store.default_int8.get(), 0x11);

	DECODE(d, "aa/default int16");
	EXPECT_EQ(ll.encoded().at(2), "!");
	DECODE(d, "w12a");
	EXPECT_EQ(ll.encoded().at(3), "!");
	EXPECT_EQ(store.default_int8.get(), 0x11);
	EXPECT_EQ(store.default_int16.get(), 0x12);

	DECODE(d, "ra");
	EXPECT_EQ(ll.encoded().at(4), "12");

	DECODE(d, "aa");
	EXPECT_EQ(ll.encoded().at(5), "!");
	DECODE(d, "ra");
	EXPECT_EQ(ll.encoded().at(6), "?");
}

TEST(Debugger, Macro)
{
	stored::Debugger d;
	stored::TestStore store;
	d.map(store);
	LoggingLayer ll;
	ll.wrap(d);

	DECODE(d, "m1;r/default uint8");
	EXPECT_EQ(ll.encoded().at(0), "!");
	DECODE(d, "1");
	EXPECT_EQ(ll.encoded().at(1), "0");
	store.default_uint8 = 2;
	DECODE(d, "1");
	EXPECT_EQ(ll.encoded().at(2), "2");

	DECODE(d, "m1|r/default uint8|e;|r/default uint16");
	EXPECT_EQ(ll.encoded().at(3), "!");
	DECODE(d, "1");
	EXPECT_EQ(ll.encoded().at(4), "2;0");

	DECODE(d, "m1");
	EXPECT_EQ(ll.encoded().at(5), "!");
	DECODE(d, "1");
	EXPECT_EQ(ll.encoded().at(6), "?");
}

TEST(Debugger, ReadMem)
{
	stored::Debugger d;
	stored::TestStore store;
	d.map(store);
	LoggingLayer ll;
	ll.wrap(d);

	uint32_t i = 0x12345678;
	char buf[32];
	snprintf(buf, sizeof(buf), "R%" PRIxPTR " 4", (uintptr_t)&i);
	d.decode(buf, strlen(buf));
#ifdef STORED_LITTLE_ENDIAN
	EXPECT_EQ(ll.encoded().at(0), "78563412");
#else
	EXPECT_EQ(ll.encoded().at(0), "12345678");
#endif
}

TEST(Debugger, WriteMem)
{
	stored::Debugger d;
	stored::TestStore store;
	d.map(store);
	LoggingLayer ll;
	ll.wrap(d);

	uint32_t i = 0x12345678;
	char buf[32];
	snprintf(buf, sizeof(buf), "W%" PRIxPTR " abcdef01", (uintptr_t)&i);
	d.decode(buf, strlen(buf));
	EXPECT_EQ(ll.encoded().at(0), "!");
#ifdef STORED_LITTLE_ENDIAN
	EXPECT_EQ(i, 0x01efcdab);
#else
	EXPECT_EQ(i, 0xabcdef01);
#endif
}

static std::string decompress(std::string const& s)
{
	LoggingLayer decompressed;
	stored::CompressLayer decompress;
	decompress.wrap(decompressed);
	std::vector<char> buf(s.begin(), s.end());
	decompress.decode(&buf[0], buf.size());
	return decompressed.decoded().at(0);
}

TEST(Debugger, Stream)
{
	stored::Debugger d;
	stored::TestStore store;
	d.map(store);
	LoggingLayer ll;
	ll.wrap(d);

	ASSERT_TRUE(stored::Config::DebuggerStreams == 2);
	// Use a stream, such that we have only room for one left.
	d.stream('z', "oh gosh");

	DECODE(d, "s1");
	EXPECT_EQ(ll.encoded().at(0), "?");

	d.stream('1', "it's ");
	d.stream('2', "a "); // no room for this stream
	d.stream('1', "small ");

	d.stream('1')->flush();
	DECODE(d, "s1");
	EXPECT_EQ(decompress(ll.encoded().at(1)), "it's small ");
	DECODE(d, "s1");
	EXPECT_EQ(ll.encoded().at(2), "");

	d.stream('1', "world ");
	d.stream('1')->flush();
	DECODE(d, "s1");
	EXPECT_EQ(decompress(ll.encoded().at(3)), "world ");

	d.stream('3', "after "); // stream 1 is empty, so 3 is using 1's space
	d.stream('3', "all");
	d.stream('3')->flush();
	d.stream('1', "world "); // no room for this stream
	EXPECT_EQ(d.stream('1'), nullptr);
	DECODE(d, "s3");
	EXPECT_EQ(decompress(ll.encoded().at(4)), "after all");

	DECODE(d, "s2");
	EXPECT_EQ(ll.encoded().at(5), "?");
	DECODE(d, "s1");
	EXPECT_EQ(ll.encoded().at(6), "?");
}

TEST(Debugger, Trace)
{
	stored::Debugger d;
	stored::TestStore store;
	d.map(store);
	LoggingLayer ll;
	ll.wrap(d);

	DECODE(d, "mt|r/default uint8|e;");
	EXPECT_EQ(ll.encoded().at(0), "!");

	DECODE(d, "ttT");
	EXPECT_EQ(ll.encoded().at(1), "!");

	d.trace();
	DECODE(d, "f");
	DECODE(d, "sT");
	EXPECT_EQ(decompress(ll.encoded().at(3)), "0;");

	store.default_uint8 = 1;
	d.trace();
	store.default_uint8 = 2;
	d.trace();

	DECODE(d, "fT");
	DECODE(d, "sT");
	EXPECT_EQ(decompress(ll.encoded().at(5)), "1;2;");

	// Set decimate
	DECODE(d, "ttT3");
	EXPECT_EQ(ll.encoded().at(6), "!");

	for(int i = 4; i < 10; i++) {
		store.default_uint8 = (uint8_t)i;
		d.trace();
	}

	DECODE(d, "fT");
	DECODE(d, "sT");
	EXPECT_EQ(decompress(ll.encoded().at(8)), "6;9;");

	// Disable
	DECODE(d, "t");
	EXPECT_EQ(ll.encoded().at(9), "!");

	d.trace();
	d.trace();
	d.trace();

	DECODE(d, "f");
	DECODE(d, "sT");
	EXPECT_EQ(ll.encoded().at(11), "");
}


} // namespace

