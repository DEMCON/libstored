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
