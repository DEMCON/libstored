// SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
//
// SPDX-License-Identifier: MPL-2.0

#include "TestStore.h"
#include "fuzz_common.h"

#include <cstring>
#include <vector>

char const* fuzz_name = "debug";

void generate()
{
	generate({"?", "i", "v", "l"});
	generate({"r/default int8", "a1/default int8", "r1", "w101", "r1"});
	generate({"w0123456789abcdef/f read/write", "r/f read-", "r/init float 1"});
	generate({"mA|e0|e1|e2", "A", "mA"});
	generate({"mt|r/default uint32", "ttt", "st", "t"});
}

void test(Messages const& msgs)
{
	std::vector<uint8_t> buf;

	stored::TestStore store;
	stored::Debugger debugger;
	debugger.map(store);

	for(auto const& msg : msgs) {
		buf.resize(msg.size());
		memcpy(buf.data(), msg.data(), msg.size());
		debugger.decode(buf.data(), msg.size());
	}
}
