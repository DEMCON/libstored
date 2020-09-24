/*
 * libstored, a Store for Embedded Debugger.
 * Copyright (C) 2020  Jochem Rutgers
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

#include <libstored/synchronizer.h>
#include "LoggingLayer.h"

namespace {

class SyncTestStore : public stored::Synchronizable<stored::TestStoreBase<SyncTestStore>> {
	friend class stored::TestStoreBase<SyncTestStore>;
};

TEST(Synchronizer, Instantiate) {
	SyncTestStore store1;
	SyncTestStore store2;

	// The store is the same...
	EXPECT_EQ(store1.hash(), store2.hash());
	// ...but the instance id isn't.
	EXPECT_NE(store1.journal().id(), store2.journal().id());
}

TEST(Synchronizer, Changes) {
	SyncTestStore store;

	auto now = store.journal().seq();
	auto v = store.default_uint8.variable();

	stored::StoreJournal::Key key = (stored::StoreJournal::Key)v.key();
	EXPECT_FALSE(store.journal().hasChanged(key, now));

	v = 1;
	EXPECT_TRUE(store.journal().hasChanged(key, now));

	now = store.journal().seq();
	store.default_uint8 = 2;
	EXPECT_TRUE(store.journal().hasChanged(key, now));
	EXPECT_FALSE(store.journal().hasChanged(key, now + 1));
}

} // namespace

