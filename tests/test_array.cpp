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

namespace {

TEST(Array, Initialized)
{
	stored::TestStore store;
	EXPECT_EQ(store.array_bool_0.get(), true);
	EXPECT_EQ(store.array_bool_1.get(), true);
	EXPECT_FLOAT_EQ(store.array_single.get(), 3.0f);
}

TEST(Array, Assign)
{
	stored::TestStore store;
	EXPECT_EQ(store.array_bool_0.get(), true);
	store.array_bool_1 = false;
	EXPECT_EQ(store.array_bool_0.get(), true);
	EXPECT_EQ(store.array_bool_1.get(), false);
}

TEST(Array, Dynamic)
{
	stored::TestStore store;
	EXPECT_TRUE(store.array_bool_a(0).template get<bool>());
	EXPECT_TRUE(store.array_bool_a(1).template get<bool>());
	EXPECT_FALSE(store.array_bool_a(2).template get<bool>());
	EXPECT_FALSE(store.array_bool_a(3).valid());
	store.array_bool_a(1).set<bool>(false);
	EXPECT_FALSE(store.array_bool_1.get());
}

} // namespace
