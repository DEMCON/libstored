// SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
//
// SPDX-License-Identifier: MPL-2.0

#include "TestStore.h"
#include "fuzz_common.h"

#include <cstring>
#include <vector>

char const* fuzz_name = "protocol";

#define FUZZ_LAYERS                                \
	EchoLayer echo;                            \
	stored::SegmentationLayer segmentation{8}; \
	segmentation.wrap(echo);                   \
	stored::DebugArqLayer arq;                 \
	arq.wrap(segmentation);                    \
	stored::Crc16Layer crc;                    \
	crc.wrap(arq);                             \
	stored::AsciiEscapeLayer escape;           \
	escape.wrap(crc);                          \
	stored::TerminalLayer terminal;            \
	terminal.wrap(escape);                     \
	[[maybe_unused]] auto& top = echo;         \
	[[maybe_unused]] auto& bottom = terminal;

void generate()
{
	FUZZ_LAYERS

	generate({"?", "i", "v", "l"}, top, bottom);
	generate({"r/default int8", "a1/default int8", "r1", "w101", "r1"}, top, bottom);
	generate({"w0123456789abcdef/f read/write", "r/f read-", "r/init float 1"}, top, bottom);
	generate({"mA|e0|e1|e2", "A", "mA"}, top, bottom);
	generate({"mt|r/default uint32", "ttt", "st", "t"}, top, bottom);
}

void test(Messages const& msgs)
{
	std::vector<uint8_t> buf;

	FUZZ_LAYERS

	for(auto const& msg : msgs) {
		buf.resize(msg.size());
		memcpy(buf.data(), msg.data(), msg.size());
		bottom.decode(buf.data(), msg.size());
	}
}
