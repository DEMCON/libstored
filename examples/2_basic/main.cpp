#include "ExampleBasic.h"

#include <cstdio>

int main()
{
    stored::ExampleBasic e;

	// Initialized value.
	printf("test42=%" PRId8 "\n", e.test42.get());

	// The thing with the space around the =
	printf("_42_test42=%" PRId8 "\n", e._42_test42.get());

	// The initialized array element.
	printf("three ints[1]=%" PRId16 "\n", e.three_ints_1.get());

    return 0;
}

