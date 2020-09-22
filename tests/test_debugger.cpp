#include "TestStore.h"
#include "gtest/gtest.h"

#include "libstored/debugger.h"

#include "LoggingLayer.h"

#define DECODE(stack, str)	do { char msg_[] = "" str; (stack).decode(msg_, sizeof(msg_) - 1); } while(0)

namespace {

TEST(Debugger, Capabilities) {
	stored::Debugger d;
	LoggingLayer ll;
	ll.wrap(d);

	DECODE(d, "?");
	EXPECT_GT(ll.encoded().at(0).size(), 1);
}

TEST(Debugger, Identification) {
	stored::Debugger d;
	LoggingLayer ll;
	ll.wrap(d);

	EXPECT_EQ(d.identification(), nullptr);
	DECODE(d, "i");
	EXPECT_EQ(ll.encoded().at(0), "?");

	d.setIdentification("asdf");
	EXPECT_EQ(d.identification(), "asdf");

	DECODE(d, "i");
	EXPECT_EQ(ll.encoded().at(1), "asdf");
}

TEST(Debugger, Version) {
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

TEST(Debugger, Find) {
	stored::Debugger d;
	stored::TestStore store;
	d.map(store);

	EXPECT_TRUE(d.find("/default int8").valid());
	EXPECT_FALSE(d.find("/default int").valid());
	EXPECT_FALSE(d.find("/default int8", 6).valid());
	EXPECT_TRUE(d.find("/sc/inner b").valid());
}

TEST(Debugger, List) {
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

} // namespace

