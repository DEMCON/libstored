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

TEST(Init, Decimal)
{
	stored::TestStore store;
	EXPECT_EQ(store.init_decimal.get(), 42);
	EXPECT_EQ(store.init_negative.get(), -42);
}

TEST(Init, Hex)
{
	stored::TestStore store;
	EXPECT_EQ(store.init_hex.get(), 0x54);
}

TEST(Init, Bin)
{
	stored::TestStore store;
	EXPECT_EQ(store.init_bin.get(), 5);
}

TEST(Init, Bool)
{
	stored::TestStore store;
	EXPECT_EQ(store.init_true.get(), true);
	EXPECT_EQ(store.init_false.get(), false);
	EXPECT_EQ(store.init_bool_0.get(), false);
	EXPECT_EQ(store.init_bool_10.get(), true);
}

TEST(Init, Float)
{
	stored::TestStore store;
	EXPECT_FLOAT_EQ(store.init_float_1.get(), 1.0f);
	EXPECT_FLOAT_EQ(store.init_float_3_14.get(), 3.14f);
	EXPECT_FLOAT_EQ(store.init_float_4000.get(), -4000.0f);
	EXPECT_FALSE(store.init_float_nan.get() == store.init_float_nan.get()); // NaN
	EXPECT_FLOAT_EQ(store.init_float_inf.get(), std::numeric_limits<float>::infinity());
	EXPECT_FLOAT_EQ(store.init_float_neg_inf.get(), -std::numeric_limits<float>::infinity());
}

TEST(Init, String)
{
	stored::TestStore store;
	char buf[16] = {};
	store.init_string.get(buf, sizeof(buf) - 1);
	EXPECT_TRUE(strcmp(buf, "a b\"c") == 0);
}

} // namespace
