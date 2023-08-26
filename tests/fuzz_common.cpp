/*
 * libstored, distributed debuggable data stores.
 * Copyright (C) 2020-2023  Jochem Rutgers
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "fuzz_common.h"
#include "LoggingLayer.h"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

#ifdef STORED_COVERAGE
#	include <csignal>
#endif

#ifdef HAVE_AFL
__AFL_FUZZ_INIT();
#endif

void help(char const* exe)
{
	printf("Usage: %s [-h|-i|<input file>]\n", exe ? exe : fuzz_name);

	printf("\nwhere\n");
	printf("   -h   Show this help and exit.\n");
	printf("   -i   Generate input files in the current directory and exit.\n");
	printf("   <input file>\n");
	printf("        The input file to run (without using AFL++).\n");

	printf("\nWithout parameters, the program expects to be controlled by AFL++ for "
	       "fuzzing.\n");
}

void generate(
	std::initializer_list<std::string> msgs, stored::ProtocolLayer& top,
	stored::ProtocolLayer& bottom)
{
	LoggingLayer l;
	l.wrap(bottom);

	for(auto const& msg : msgs) {
		char len = (char)std::min<size_t>(0xff, msg.size());
		top.encode(msg.data(), (size_t)(uint8_t)len, true);
		top.flush();
		l.encoded().insert(
			l.encoded().end() - 1, std::string{(char)l.encoded().back().size()});
	}
	auto buf = l.allEncoded();

	static size_t count;
	std::array<char, 64> filename{};
	snprintf(filename.data(), filename.size(), "fuzz_%s_%03zu.bin", fuzz_name, count++);

	FILE* f = fopen(filename.data(), "wb");
	if(!f) {
		printf("Cannot open %s for writing; %s\n", filename.data(), strerror(errno));
		exit(1);
	}

	auto written = fwrite(buf.data(), buf.size(), 1, f);
	fclose(f);

	if(written != 1) {
		printf("Cannot write %s\n", filename.data());
		exit(1);
	}

	printf("Generated %s\n", filename.data());
}

void generate(std::initializer_list<std::string> msgs)
{
	stored::ProtocolLayer p;
	generate(msgs, p, p);
}

void test(void const* buf, size_t len)
{
	test(Messages{buf, len});
}

int test(char const* file)
{
	int res = 1;
	int fd = -1;
	void* buf = nullptr;
	size_t len = 0;
	struct stat s {};

	printf("Reading %s...\n", file);

	fd = open(file, O_RDONLY);

	if(fd == -1) {
		printf("Cannot open file; %s\n", strerror(errno));
		goto done;
	}

	if(fstat(fd, &s) == -1) {
		printf("Cannot stat file; %s\n", strerror(errno));
		goto close;
	}

	len = (size_t)s.st_size;
	buf = mmap(nullptr, len, PROT_READ, MAP_SHARED, fd, 0);

	if(buf == MAP_FAILED) {
		printf("Cannot mmap; %s\n", strerror(errno));
		goto close;
	}

	test({buf, len});
	res = 0;

	// unmap:
	munmap(buf, len);
close:
	close(fd);
done:
	return res;
}

#ifdef STORED_COVERAGE
// Coverage statistics are written upon proper exit. However, the program may not exit gracefully
// while fuzzing. Trigger writing when we get a signal.

extern "C" void __gcov_dump(void);

static void coverage_sig(int sig)
{
	__gcov_dump();

	switch(sig) {
	case SIGINT:
	case SIGTERM:
		exit(sig);
		break;
	default:;
	}
}

static void coverage_setup()
{
	for(auto sig : {SIGHUP, SIGINT, SIGTERM}) {
		if(signal(sig, coverage_sig) == SIG_ERR)
			printf("Cannot register signal %d handler; %s (ignored)\n", sig,
			       strerror(errno));
	}
}
#else
static void coverage_setup() {}
#endif

#ifdef HAVE_AFL
#	ifdef STORED_COMPILER_CLANG
#		pragma clang optimize off
#	endif
#	ifdef STORED_COMPILER_GCC
#		pragma GCC optimize("O0")
#	endif
#endif
int main(int argc, char** argv)
{
	printf("%s\n\n", stored::banner());
	printf("Fuzzing %s\n\n", fuzz_name);

	switch(argc) {
	case 0:
	case 1:
		break;
	case 2:
		if(strcmp(argv[1], "-h") == 0) {
			help(argv[0]);
			return 0;
		} else if(strcmp(argv[1], "-i") == 0) {
			generate();
			return 0;
		}

		return test(argv[1]);
	default:
		help(argv[0]);
		return 1;
	}

#ifndef HAVE_AFL
	printf("Compile this program with afl-clang-fast++ to do fuzzing.\n");
	return 1;
#else

	coverage_setup();

	printf("Ready. Waiting for afl-fuzz for instructions...\n");

	__AFL_INIT();

	unsigned char* buf = __AFL_FUZZ_TESTCASE_BUF;

	while(__AFL_LOOP(100000)) {
		auto len = __AFL_FUZZ_TESTCASE_LEN;
		test(buf, (size_t)len);
	}

	return 0;
#endif
}
