// SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
//
// SPDX-License-Identifier: CC0-1.0

// This is a comment. The comment is always till the end of the line.

// Basically, a variable is defined as:
// <type> <name>
//
// Only built-in types can be used (although a blob can be cast to anything),
// and the name consists of all ASCII-printable characters (ASCII 0x20-0x7e).
// All variable are (default) initialized to zero or equivalent.
// More on that later on, but this creates two variables:

int32 hello
double world

// This file is named Hello.st, so a C++ object stored::Hello is generated,
// which can be included using Hello.h.

// Check out scripts/st.vim for syntax highlighting in Vim.

