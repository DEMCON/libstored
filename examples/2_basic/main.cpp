#include "Example.h"

int main()
{
    stored::Example e;

	// Initialized value.
	printf("test42=%" PRId8 "\n", e.test42().get());

	// The thing with the space around the =
	printf("_42_test42=%" PRId8 "\n", e._42_test42().get());

    return 0;
}
