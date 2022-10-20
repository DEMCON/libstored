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
#include "LoggingLayer.h"

#include <chrono>

class SyncTestStore : public stored::Synchronizable<stored::TestStoreBase<SyncTestStore>> {
	friend class stored::TestStoreBase<SyncTestStore>;
};

namespace {

TEST(Synchronizer, Endianness)
{
	EXPECT_EQ(stored::swap_endian<uint8_t>(1), 1);
	EXPECT_EQ(stored::swap_endian<uint16_t>(0x1234), 0x3412);
	EXPECT_EQ(stored::swap_endian<uint32_t>(0x12345678), 0x78563412);

	uint8_t b[] = {1, 2, 3};
	stored::swap_endian_<3>(b);
	EXPECT_EQ(b[0], 3);
	EXPECT_EQ(b[1], 2);
	EXPECT_EQ(b[2], 1);

	stored::swap_endian(b, 2);
	EXPECT_EQ(b[0], 2);
	EXPECT_EQ(b[1], 3);
	EXPECT_EQ(b[2], 1);
}

TEST(Synchronizer, Instantiate)
{
	SyncTestStore store1;
	SyncTestStore store2;

	EXPECT_EQ(store1.hash(), store2.hash());
}

class TestJournal : public stored::StoreJournal {
	STORED_CLASS_NOCOPY(TestJournal)
public:
	template <typename... Arg>
	TestJournal(Arg&&... arg)
		: StoreJournal(std::forward<Arg>(arg)...)
	{}

	FRIEND_TEST(
		Synchronizer, // fmt
		ShortSeq);
};

TEST(Synchronizer, ShortSeq)
{
	TestJournal j("123", nullptr, 0u);

	EXPECT_EQ(j.seq(), 1);

	j.changed(1, 0);
	EXPECT_TRUE(j.hasChanged(1, 1));

	for(int i = 1; i < 50; i++)
		j.bumpSeq(true);

	EXPECT_EQ(j.seq(), 50);
	EXPECT_FALSE(j.hasChanged(1, 2));

	EXPECT_EQ(j.toShort(50), 50);
	EXPECT_EQ(j.toShort(49), 49);
	EXPECT_EQ(j.toShort(1), 1);

	EXPECT_EQ(j.toLong(50), 50);
	EXPECT_EQ(j.toLong(49), 49);
	EXPECT_EQ(j.toLong(1), 1);

	for(int i = 0; i < 0x10000; i++)
		j.bumpSeq(true);

	EXPECT_EQ(j.toShort(0x10032), 50);
	EXPECT_EQ(j.toShort(0x10031), 49);
	EXPECT_EQ(j.toShort(0x10001), 1);
	EXPECT_EQ(j.toShort(51), 51);

	EXPECT_EQ(j.toLong(51), 51);
	EXPECT_EQ(j.toLong(50), 0x10032);
	EXPECT_EQ(j.toLong(49), 0x10031);
	EXPECT_EQ(j.toLong(1), 0x10001);

	EXPECT_TRUE(j.hasChanged(
		1, j.seq() - TestJournal::ShortSeqWindow + TestJournal::SeqLowerMargin));
	EXPECT_FALSE(j.hasChanged(
		1, j.seq() - TestJournal::ShortSeqWindow + TestJournal::SeqLowerMargin * 2u));
}

TEST(Synchronizer, Changes)
{
	SyncTestStore store;

	auto now = store.journal().seq();
	auto u8 = store.default_uint8.variable();

	size_t c = 0;
	store.journal().iterateChanged(0, [&](stored::StoreJournal::Key) { c++; });
	EXPECT_EQ(c, 0);

	stored::StoreJournal::Key key_u8 = (stored::StoreJournal::Key)u8.key();
	EXPECT_FALSE(store.journal().hasChanged(key_u8, now));

	u8 = 1;
	EXPECT_TRUE(store.journal().hasChanged(key_u8, now));

	c = 0;
	store.journal().iterateChanged(0, [&](stored::StoreJournal::Key) { c++; });
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
	store.journal().iterateChanged(0, [&](stored::StoreJournal::Key) { c++; });
	EXPECT_EQ(c, 2);
}

#define EXPECT_SYNCED(store1, store2)                                      \
	do {                                                               \
		auto _map1 = (store1).map();                               \
		auto _map2 = (store2).map();                               \
		for(auto& _o : _map1)                                      \
			EXPECT_EQ(_o.second.get(), _map2[_o.first].get()); \
	} while(0)

#define EXPECT_NOT_SYNCED(store1, store2)                              \
	do {                                                           \
		auto _map1 = (store1).map();                           \
		auto _map2 = (store2).map();                           \
		bool _synced = true;                                   \
		for(auto& _o : _map1)                                  \
			if(_o.second.get() != _map2[_o.first].get()) { \
				_synced = false;                       \
				break;                                 \
			}                                              \
		EXPECT_FALSE(_synced);                                 \
	} while(0)

TEST(Synchronizer, Sync2)
{
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

TEST(Synchronizer, Sync5)
{
	SyncTestStore store[5];
	stored::Synchronizer s[5];

	for(size_t i = 0; i < 5; i++)
		s[i].map(store[i]);

	/*
	 * Topology: higher in tree is source.
	 *
	 *     0
	 *    /  \
	 *   1    2
	 *       /  \
	 *      3    4
	 */

	LoggingLayer ll01;
	LoggingLayer ll10;
	LoggingLayer ll02;
	LoggingLayer ll20;
	LoggingLayer ll23;
	LoggingLayer ll32;
	LoggingLayer ll24;
	LoggingLayer ll42;
	stored::Loopback loop01(ll01, ll10);
	stored::Loopback loop02(ll02, ll20);
	stored::Loopback loop23(ll23, ll32);
	stored::Loopback loop24(ll24, ll42);

	s[0].connect(ll01);
	s[0].connect(ll02);
	s[1].connect(ll10);
	s[2].connect(ll20);
	s[2].connect(ll23);
	s[2].connect(ll24);
	s[3].connect(ll32);
	s[4].connect(ll42);

	s[1].syncFrom(store[1], ll10);
	s[2].syncFrom(store[2], ll20);
	s[3].syncFrom(store[3], ll32);
	s[4].syncFrom(store[4], ll42);

	for(size_t i = 1; i < 5; i++)
		EXPECT_SYNCED(store[0], store[1]);

	store[0].default_uint8 = 1;
	s[0].process();
	EXPECT_EQ(store[4].default_uint8.get(), 0);
	s[2].process();

	EXPECT_EQ(store[1].default_uint8.get(), 1);
	EXPECT_EQ(store[2].default_uint8.get(), 1);
	EXPECT_EQ(store[3].default_uint8.get(), 1);
	EXPECT_EQ(store[4].default_uint8.get(), 1);

	for(size_t i = 1; i < 5; i++)
		EXPECT_SYNCED(store[0], store[i]);

	store[3].default_int16 = 2;
	store[2].default_int32 = 3;
	store[4].default_uint8 = 4;
	store[1].default_uint16 = 5;
	store[0].default_uint32 = 6;

	for(size_t j = 0; j < 3; j++)
		for(size_t i = 0; i < 5; i++)
			s[i].process();

	for(size_t i = 1; i < 5; i++)
		EXPECT_SYNCED(store[0], store[i]);

	SyncTestStore::ObjectMap map[5];
	std::vector<SyncTestStore::ObjectMap::mapped_type> list[5];

	for(size_t i = 0; i < 5; i++) {
		map[i] = store[i].map();
		for(auto& x : map[i])
			list[i].push_back(x.second);
	}

	auto start = std::chrono::steady_clock::now();
	int count = 0;
	do {
		for(size_t batch = 0; batch < 10; batch++) {
			// Pick a random store
			int i = rand() % 5;
			auto& l = list[i];

			// Pick a random object from that store (but only one in five can be written
			// by us).
			auto& o = l[((size_t)rand() % (l.size() / 5u)) * 5u + (size_t)i];

			// Flip a bit of that object.
			auto data = o.get();
			data[0] = (char)(data[0] + 1);
			o.set(data);
			count++;
		}

		// Do a full sync and check.
		for(size_t j = 0; j < 3; j++)
			for(size_t i = 0; i < 5; i++)
				s[i].process();

		for(size_t i = 1; i < 5; i++)
			EXPECT_SYNCED(store[0], store[i]);
	} while(std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count()
		< 1);

	EXPECT_GT(count, 100);
}

} // namespace
