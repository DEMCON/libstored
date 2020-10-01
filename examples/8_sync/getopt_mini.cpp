#include "getopt_mini.h"

#ifndef STORED_OS_POSIX

#include <stddef.h>

int opterr = 1;
int optopt = 0;
int optind = 1;
char* optarg = nullptr;

int getopt(int argc, char* const* argv, char const* options) {
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
	for(; options[i] && options[i] != optopt; i++);

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

#endif // STORED_OS_LINUX
