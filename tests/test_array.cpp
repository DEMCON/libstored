#include "TestStore.h"
#include "gtest/gtest.h"

namespace {

TEST(Array, Initialized) {
	stored::TestStore store;
	EXPECT_EQ(store.array_bool_0().get(), true);
	EXPECT_EQ(store.array_bool_1().get(), true);
	EXPECT_FLOAT_EQ(store.array_single().get(), 3.0f);
}

TEST(Array, Assign) {
	stored::TestStore store;
	EXPECT_EQ(store.array_bool_0().get(), true);
	store.array_bool_1() = false;
	EXPECT_EQ(store.array_bool_0().get(), true);
	EXPECT_EQ(store.array_bool_1().get(), false);
}

} // namespace

