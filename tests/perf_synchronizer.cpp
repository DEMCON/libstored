// SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
//
// SPDX-License-Identifier: MPL-2.0

#include "TestStore.h"

#include <cstdio>
#include <cstring>
#include <stored>

// Enable the following line to print messages exchanged.
//#define ENABLE_LOGGING

class SyncTestStore : public STORE_T(
			      SyncTestStore, stored::TestStoreDefaultFunctions,
			      stored::Synchronizable, stored::TestStoreBase) {
	STORE_CLASS(
		SyncTestStore, stored::TestStoreDefaultFunctions, stored::Synchronizable,
		stored::TestStoreBase)

public:
	SyncTestStore() = default;
};

static SyncTestStore store1;
static SyncTestStore store2;

static stored::Synchronizer s1;
static stored::Synchronizer s2;

#ifdef ENABLE_LOGGING
static stored::PrintLayer l1;
static stored::PrintLayer l2;
#endif

static stored::FifoLoopback<SyncTestStore::MaxMessageSize * 2, 16> loop;

static void help(char const* progname)
{
	printf("Usage: %s <iterations>\n", progname);
}

int main(int argc, char** argv)
{
	printf("%s\n\n", stored::banner());
	printf("Synchronizer performance tester\n\n");

	int iterations = 1000;

	switch(argc) {
	case 0:
	case 1:
		break;
	case 2:
		if(strcmp(argv[1], "-h") == 0) {
			help(argv[0]);
			return 0;
		}

		iterations = std::atoi(argv[1]);

		if(iterations <= 0) {
			help(argv[0]);
			return 1;
		}
		break;
	default:
		help(argv[0]);
		return 1;
	}

	loop.a2b().setOverflowHandler([&]() {
		printf("Buffer overflow\n");
		std::abort();
		return false;
	});

	loop.b2a().setOverflowHandler([&]() {
		printf("Buffer overflow\n");
		std::abort();
		return false;
	});

	printf("Running %d iterations...\n", iterations);
	fflush(nullptr);

	s1.map(store1);
	s2.map(store2);

#ifdef ENABLE_LOGGING
	stored::ProtocolLayer& p1 = l1;
	stored::ProtocolLayer& p2 = l2;
	loop.a().wrap(l1);
	loop.b().wrap(l2);
#else
	stored::ProtocolLayer& p1 = loop.a();
	stored::ProtocolLayer& p2 = loop.b();
#endif

	s1.connect(p1);
	s2.connect(p2);
	s2.syncFrom(store2, p2);

	for(int i = 0; i < iterations; i++) {
		// Change some data.
		store1.default_int8 = (int8_t)i;
		store1.default_int16 = (int16_t)i;
		store1.default_int32 = (int32_t)i;

		// Send updates.
		loop.b2a().recvAll();
		s1.process();
		loop.a2b().recvAll();
		s2.process();

		stored_assert(store2.default_int32.get() == (int32_t)i);
	}

	// Done.
}
