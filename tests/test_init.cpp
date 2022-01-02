/*
 * libstored, distributed debuggable data stores.
 * Copyright (C) 2020-2022  Jochem Rutgers
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

} // namespace

