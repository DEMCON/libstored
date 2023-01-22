/*
 * libstored, distributed debuggable data stores.
 * Copyright (C) 2020-2022  Jochem Rutgers
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "TestStore.h"
#include "gtest/gtest.h"

#include <libstored/synchronizer.h>

namespace {

class TestStore : public stored::TestStoreBase<TestStore> {
	STORE_CLASS(TestStore, stored::TestStoreBase)

	FRIEND_TEST(
		Hooks, // fmt
		Default);

public:
	TestStore() = default;
};

TEST(Hooks, Default)
{
	TestStore store;
	EXPECT_TRUE(store.__hookEntryRO__default());
	EXPECT_TRUE(store.__hookExitRO__default());
	EXPECT_TRUE(store.__hookEntryX__default());
	EXPECT_TRUE(store.__hookExitX__default());
	EXPECT_TRUE(store.__hookChanged__default());
}

class SyncTestStore : public stored::Synchronizable<stored::TestStoreBase<SyncTestStore>> {
	STORE_CLASS(SyncTestStore, stored::Synchronizable, stored::TestStoreBase)

	FRIEND_TEST(
		Hooks, // fmt
		Synchronizable);

public:
	SyncTestStore() = default;
};

TEST(Hooks, Synchronizable)
{
	SyncTestStore store;
	EXPECT_TRUE(store.__hookEntryRO__default());
	EXPECT_TRUE(store.__hookExitRO__default());
	EXPECT_TRUE(store.__hookEntryX__default());
	EXPECT_FALSE(store.__hookExitX__default());
	EXPECT_TRUE(store.__hookChanged__default());
}

class HookedTestStore : public stored::TestStoreBase<HookedTestStore> {
	STORE_CLASS(HookedTestStore, stored::TestStoreBase)

	FRIEND_TEST(
		Hooks, // fmt
		Changed);

public:
	HookedTestStore() = default;

protected:
	void __default_int32__changed()
	{
		default_int32_cnt++;
	}

	int default_int32_cnt = 0;
};

TEST(Hooks, Changed)
{
	HookedTestStore store;

	EXPECT_TRUE(store.__hookEntryRO__default());
	EXPECT_TRUE(store.__hookExitRO__default());
	EXPECT_TRUE(store.__hookEntryX__default());
	EXPECT_TRUE(store.__hookExitX__default());
	EXPECT_FALSE(store.__hookChanged__default());

	EXPECT_EQ(store.default_int32_cnt, 0);

	store.default_int32 = 1;
	EXPECT_EQ(store.default_int32_cnt, 1);
}

class HookedSyncTestStore
	: public stored::Synchronizable<stored::TestStoreBase<HookedSyncTestStore>> {
	STORE_CLASS(HookedSyncTestStore, stored::Synchronizable, stored::TestStoreBase)

	FRIEND_TEST(
		Hooks, // fmt
		SyncHook);

public:
	HookedSyncTestStore() = default;

protected:
	void __default_int32__changed()
	{
		default_int32_cnt++;
	}

	int default_int32_cnt = 0;
};

TEST(Hooks, SyncHook)
{
	HookedSyncTestStore store1;
	HookedSyncTestStore store2;

	EXPECT_TRUE(store1.__hookEntryRO__default());
	EXPECT_TRUE(store1.__hookExitRO__default());
	EXPECT_TRUE(store1.__hookEntryX__default());
	EXPECT_FALSE(store1.__hookExitX__default());
	EXPECT_FALSE(store1.__hookChanged__default());

	stored::Synchronizer s1;
	stored::Synchronizer s2;

	stored::ProtocolLayer p1;
	stored::ProtocolLayer p2;
	stored::Loopback loop(p1, p2);

	s1.map(store1);
	s2.map(store2);
	s1.connect(p1);
	s2.connect(p2);

	s2.syncFrom(store2, p2);

	// Should be synced now.
	EXPECT_EQ(store1.default_int32_cnt, 0);
	EXPECT_EQ(store2.default_int32_cnt, 1); // because of Welcome

	store1.default_int32 = 1;
	store1.default_int32 = 2;
	store1.default_int32 = 3;
	store1.default_int32 = 4;
	store1.default_int32 = 5;
	s1.process();

	EXPECT_EQ(store1.default_int32_cnt, 5); // local update
	EXPECT_EQ(store2.default_int32_cnt, 2); // because of Update
}

} // namespace
