// This is a comment. The comment must start at the beginning of the line.
// It is not possible to add a comment as a suffix of a non-comment line.

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

