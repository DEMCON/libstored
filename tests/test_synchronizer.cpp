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

TEST(Synchronizer, Endianness) {
	EXPECT_EQ(stored::swap_endian<uint8_t>(1), 1);
	EXPECT_EQ(stored::swap_endian<uint16_t>(0x1234), 0x3412);
	EXPECT_EQ(stored::swap_endian<uint32_t>(0x12345678), 0x78563412);

	uint8_t b[] = {1,2,3};
	stored::swap_endian_<3>(b);
	EXPECT_EQ(b[0], 3);
	EXPECT_EQ(b[1], 2);
	EXPECT_EQ(b[2], 1);

	stored::swap_endian(b, 2);
	EXPECT_EQ(b[0], 2);
	EXPECT_EQ(b[1], 3);
	EXPECT_EQ(b[2], 1);
}

TEST(Synchronizer, Instantiate) {
	SyncTestStore store1;
	SyncTestStore store2;

	EXPECT_EQ(store1.hash(), store2.hash());
}

TEST(Synchronizer, Changes) {
	SyncTestStore store;

	auto now = store.journal().seq();
	auto u8 = store.default_uint8.variable();

	size_t c = 0;
	store.journal().iterateChanged(0, [&](stored::StoreJournal::Key){ c++; });
	EXPECT_EQ(c, 0);

	stored::StoreJournal::Key key_u8 = (stored::StoreJournal::Key)u8.key();
	EXPECT_FALSE(store.journal().hasChanged(key_u8, now));

	u8 = 1;
	EXPECT_TRUE(store.journal().hasChanged(key_u8, now));

	c = 0;
	store.journal().iterateChanged(0, [&](stored::StoreJournal::Key){ c++; });
	EXPECT_EQ(c, 1);

	now = store.journal().seq();
	store.default_uint8 = 2;
	EXPECT_TRUE(store.journal().hasChanged(key_u8, now));
	EXPECT_FALSE(store.journal().hasChanged(key_u8, now + 1));

	now = store.journal().bumpSeq();
	EXPECT_FALSE(store.journal().hasChanged(key_u8, now));

	auto u16 = store.default_uint16.variable();
	stored::StoreJournal::Key key_u16 = (stored::StoreJournal::Key)u16.key();
	EXPECT_FALSE(store.journal().hasChanged(key_u16, now));
	store.default_uint16 = 3;
	EXPECT_TRUE(store.journal().hasChanged(key_u16, now));

	EXPECT_FALSE(store.journal().hasChanged(key_u8, now));
	EXPECT_TRUE(store.journal().hasChanged(now));

	c = 0;
	store.journal().iterateChanged(0, [&](stored::StoreJournal::Key){ c++; });
	EXPECT_EQ(c, 2);
}

#define EXPECT_SYNCED(store1, store2) \
	do { \
		auto _map1 = (store1).map(); \
		auto _map2 = (store2).map(); \
		for(auto& _o : _map1) \
			EXPECT_EQ(_o.second.get(), _map2[_o.first].get()); \
	} while(0)

#define EXPECT_NOT_SYNCED(store1, store2) \
	do { \
		auto _map1 = (store1).map(); \
		auto _map2 = (store2).map(); \
		bool _synced = true; \
		for(auto& _o : _map1) \
			if(_o.second.get() != _map2[_o.first].get()) { \
				_synced = false; \
				break; \
			} \
		EXPECT_FALSE(_synced); \
	} while(0)

TEST(Synchronizer, Sync1) {
	SyncTestStore store1;
	SyncTestStore store2;

	stored::Synchronizer s1;
	stored::Synchronizer s2;

	LoggingLayer ll1;
	LoggingLayer ll2;
	stored::Loopback loop(ll1, ll2);

	s1.map(store1);
	s2.map(store2);
	s1.connect(ll1);
	s2.connect(ll2);

	// Equal at initialization.
	EXPECT_SYNCED(store1, store2);

	s2.syncFrom(store2, ll2);

	store1.default_uint8 = 1;
	// Not synced yet.
	EXPECT_NOT_SYNCED(store1, store2);
	s1.process();
	EXPECT_EQ(store2.default_uint8.get(), 1);

	// Equal after sync.
	EXPECT_SYNCED(store1, store2);

	store2.default_uint16 = 2;
	s2.process();
	EXPECT_EQ(store1.map()["/default uint16"].get<uint16_t>(), 2);
	EXPECT_SYNCED(store1, store2);

	store1.default_uint8 = 3;
	store2.default_uint16 = 4;
	EXPECT_NOT_SYNCED(store1, store2);
	s1.process();
	s2.process();
	EXPECT_SYNCED(store1, store2);

	for(auto& s : ll2.encoded())
		printBuffer(s, "> ");
	for(auto& s : ll2.decoded())
		printBuffer(s, "< ");

}

} // namespace

