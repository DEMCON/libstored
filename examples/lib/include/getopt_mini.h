#ifndef GETOPT_MINI_H
#define GETOPT_MINI_H
// SPDX-FileCopyrightText: 2020-2025 Jochem Rutgers
//
// SPDX-License-Identifier: MIT

#if defined(__linux__) || defined(__APPLE__)
// Just use glibc's one.
#  include <unistd.h>
#else // !POSIX

extern int opterr;
extern int optopt;
extern int optind;
extern char* optarg;

// flawfinder: ignore
int getopt(int argc, char* const* argv, char const* options);

#endif // !POSIX
#endif // GETOPT_MINI_H
