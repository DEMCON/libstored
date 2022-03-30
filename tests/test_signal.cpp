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

#include "libstored/signal.h"
#include "TestStore.h"

#include "gtest/gtest.h"

class TestStore : public STORE_T(TestStore, stored::Signalling, stored::TestStoreBase) {
	STORE_CLASS(TestStore, stored::Signalling, stored::TestStoreBase)
public:
	TestStore() = default;
};

namespace {

TEST(Signal, Var)
{
	TestStore store;

	int sig = 0;
	store.connect(store.default_int8, [&]() { sig++; });

	store.default_int8 = 1;
	EXPECT_EQ(sig, 1);

	store.default_int8 = 1;
	EXPECT_EQ(sig, 1);

	store.default_int8 = 10;
	EXPECT_EQ(sig, 2);

	store.disconnect(store.default_int8);
	store.default_int8 = 11;
	EXPECT_EQ(sig, 2);
}

TEST(Signal, Variant)
{
	TestStore store;

	int sig = 0;
	store.connect(store.init_string, [&]() { sig++; });

	store.init_string.set("a");
	EXPECT_EQ(sig, 1);

	store.init_string.set("a");
	EXPECT_EQ(sig, 1);

	store.init_string.set("b");
	EXPECT_EQ(sig, 2);

	store.disconnect(store.init_string);
	store.init_string.set("c");
	EXPECT_EQ(sig, 2);
}

} // namespace
