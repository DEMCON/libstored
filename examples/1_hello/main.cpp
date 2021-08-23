/*!
 * \file
 * \brief Hello world example.
 */

// Include the generated model, based on ExampleHello.st.
#include "ExampleHello.h"

#include <cstdio>

int main()
{
	// Construct the store.
	stored::ExampleHello h;

	// Print defaults.
	printf("hello=%" PRIi32 " world=%g\n", h.hello.get(), h.world.get());

	// Set some values.
	h.hello = 42;
	h.world = 3.14;

	// Check if it worked.
	printf("hello=%d world=%g\n", h.hello.as<int>(), h.world.get());

	// All variables can be accessed by name too. This is encoded in a
	// directory with scopes (see later examples), but this requires to add the
	// root /.
	// cppcheck-suppress unreadVariable
	h.find("/hello").variable<int32_t>() = 43;

	// Check if it changed.
	printf("hello=%d world=%g\n", h.hello.as<int>(), h.world.get());

	return 0;
}

