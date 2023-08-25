/*
 * libstored, distributed debuggable data stores.
 * Copyright (C) 2020-2023  Jochem Rutgers
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "LoggingLayer.h"
#include "TestStore.h"
#include "fuzz_common.h"

#include <cstring>
#include <vector>

#include <cassert>

char const* fuzz_name = "synchronizer";

class SyncTestStore : public STORE_T(
			      SyncTestStore, stored::TestStoreDefaultFunctions,
			      stored::Synchronizable, stored::TestStoreBase) {
	STORE_CLASS(
		SyncTestStore, stored::TestStoreDefaultFunctions, stored::Synchronizable,
		stored::TestStoreBase)

public:
	SyncTestStore() = default;
};

void generate()
{
	// Generate traffic.

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

	// s1 -> s2: hello
	// s2 -> s1: welcome
	s1.syncFrom(store1, ll1);

	store1.default_uint8 = 1;
	// s1 -> s2: update
	s1.process();
	store1.default_uint16 = 2;
	// s1 -> s2: update
	s1.process();

	store1.default_uint32 = 3;
	store1.default_bool = true;
	store1.default_float = 3.14F;
	// s1 -> s2: update 3x
	s1.process();

	// s1 -> s2: bye
	s1.disconnect(ll1);

#if 0
	for(auto& s : ll2.encoded())
		printBuffer(s, "> ");
	for(auto& s : ll2.decoded())
		printBuffer(s, "< ");
#endif

	// Now, while fuzzing, let the fuzzing target be s2, which gets the hello.  Next, send the
	// updates to the fuzzing target.  So, replay the messages as recorded in ll2.decoded().
	auto const& msgs = ll2.decoded();
	assert(msgs.size() == 5);
	generate({msgs[0], msgs[1], msgs[2], msgs[3], msgs[4]});
}

void test(Messages const& msgs)
{
	std::vector<uint8_t> buf;

	SyncTestStore store;
	stored::Synchronizer sync;
	sync.map(store);
	stored::ProtocolLayer p;
	sync.connect(p);

	for(auto const& msg : msgs) {
		buf.resize(msg.size());
		memcpy(buf.data(), msg.data(), msg.size());
		p.decode(buf.data(), msg.size());
	}
}
