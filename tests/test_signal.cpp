/*
 * libstored, distributed debuggable data stores.
 * Copyright (C) 2020-2023  Jochem Rutgers
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
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

TEST(Signal, NoKey)
{
	stored::Signal<> s;

	int sig = 0;
	s.connect([&]() { sig++; });
	s();
	EXPECT_EQ(sig, 1);

	// Another connection, with a 'different' lambda.
	s.connect([&]() { sig++; });
	s();
	EXPECT_EQ(sig, 3);

	s.disconnect();
	s();
	EXPECT_EQ(sig, 3);
}

TEST(Signal, NoToken)
{
	stored::Signal<int> s;

	int sig = 0;
	s.connect(1, [&]() { sig++; });
	s();
	EXPECT_EQ(sig, 1);

	s.connect(2, [&]() { sig++; });
	s.connect(2, [&]() { sig++; });
	s();
	EXPECT_EQ(sig, 4);

	s.disconnect(2);
	s();
	EXPECT_EQ(sig, 5);
}

TEST(Signal, Token)
{
	stored::Signal<int, int> s;

	int sig = 0;
	s.connect(
		0, [&]() { sig++; }, 1);
	s.connect(
		0, [&]() { sig++; }, 2);
	s();
	EXPECT_EQ(sig, 2);

	s.disconnect(0, 2);
	s();
	EXPECT_EQ(sig, 3);
}

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
