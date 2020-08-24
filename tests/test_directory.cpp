#include "TestStore.h"
#include "gtest/gtest.h"

namespace {

TEST(Directory, FullMatch) {
	stored::TestStore store;
	EXPECT_TRUE(store.find("/default int8").valid());
	EXPECT_TRUE(store.find("/f read/write").valid());
	EXPECT_TRUE(store.find("/f read-only").valid());
	EXPECT_TRUE(store.find("/array bool[1]").valid());
	EXPECT_TRUE(store.find("/scope/inner bool").valid());
}

TEST(Directory, ShortMatch) {
	stored::TestStore store;
	EXPECT_TRUE(store.find("/d.......f").valid());
	EXPECT_TRUE(store.find("/f.r.../").valid());
	EXPECT_TRUE(store.find("/f.r...-").valid());
	EXPECT_TRUE(store.find("/init float 3").valid());
	EXPECT_TRUE(store.find("/sc/i.....b").valid());
	EXPECT_TRUE(store.find("/so/s").valid());
}

TEST(Directory, Ambiguous) {
	stored::TestStore store;
	EXPECT_FALSE(store.find("/default int").valid());
	EXPECT_FALSE(store.find("/s/inner bool").valid());
}

TEST(Directory, Bogus) {
	stored::TestStore store;
	EXPECT_FALSE(store.find("").valid());
	EXPECT_FALSE(store.find("/").valid());
	EXPECT_FALSE(store.find("asdf").valid());
	EXPECT_FALSE(store.find("/zzz").valid());
}

} // namespace

