#include "TestStore.h"
#include "gtest/gtest.h"

TEST(Types, Bool) {
	stored::TestStore store;
	EXPECT_FALSE(store.default_bool().get());
	store.default_bool() = true;
	EXPECT_TRUE(store.default_bool().get());
}

TEST(Types, Int8) {
	stored::TestStore store;
	EXPECT_EQ(store.default_int8().get(), 0);
	store.default_int8() = 42;
	EXPECT_EQ(store.default_int8().get(), 42);
}

