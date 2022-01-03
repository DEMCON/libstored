/*!
 * \file
 * \brief Zth integration example.
 */

#include "ExampleFibered.h"

#include <cstdio>
#include <zth>

// TODO: fiber-aware poller example.

int main_fiber(int UNUSED_PAR(argc), char** UNUSED_PAR(argv))
{
	stored::ExampleFibered s;
	printf("i=%d\n", s.i.as<int>());
	return 0;
}
