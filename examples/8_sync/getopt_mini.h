#ifndef GETOPT_MINI_H
#define GETOPT_MINI_H

#include <libstored/macros.h>

#ifdef STORED_OS_LINUX
// Just use glibc's one.
#  include <unistd.h>
#else // STORED_OS_LINUX

extern int opterr;
extern int optopt;
extern int optind;
extern char* optarg;
int getopt(int argc, char* const* argv, char const* options);

#endif // !STORED_OS_LINUX
#endif // GETOPT_MINI_H
