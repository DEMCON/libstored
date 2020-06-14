#include "Example.h"

int main() {
	stored::Example e;

	// Notice how the scope separator gets replaced by two underscores.
	printf("/scope/an int by API: %" PRId8 "\n", e.scope__an_int().get());
	printf("/scope/an int by find(): %" PRId8 "\n", e.find("/scope/an int").variable<int8_t>().get());
	printf("G = %g\n", e.gravitational_constant_m__s_2().get());

	return 0;
}
