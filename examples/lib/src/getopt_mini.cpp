// SPDX-FileCopyrightText: 2020-2025 Jochem Rutgers
//
// SPDX-License-Identifier: MIT

#include <getopt_mini.h>

#if !defined(__linux__) && !defined(__APPLE__) && !defined(__MINGW32__) && !defined(__MINGW64__)

#  include <stddef.h>

int opterr = 1;
int optopt = 0;
int optind = 1;
char* optarg = NULL;

// flawfinder: ignore
int getopt(int argc, char* const* argv, char const* options)
{
	if(optind >= argc || !argv || !options)
		return -1;

	char* a = argv[optind++];

	if(a[0] != '-')
		// Stop parsing.
		return -1;

	switch((optopt = a[1])) {
	case '\0':
	case ':':
		// Not an option.
	case '-':
		// Stop parsing.
		return -1;
	default:;
	}

	// Check if option exists.
	int i = 0;
	for(; options[i] && options[i] != optopt; i++)
		;

	if(!options[i])
		// Unknown.
		return '?';

	if(options[i + 1] != ':')
		// No argument, ok.
		return optopt;

	if(a[2] != '\0')
		// Argument is merged.
		optarg = &a[2];
	else if(optind < argc)
		// Argument is next arg.
		optarg = argv[optind++];
	else
		return options[0] == ':' ? ':' : '?';

	// Ok.
	return optopt;
}

#else  // POSIX
char dummy_char_to_make_getopt_mini_cpp_non_empty; // NOLINT
#endif // POSIX
