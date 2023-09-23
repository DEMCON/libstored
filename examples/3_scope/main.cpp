// SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
//
// SPDX-License-Identifier: CC0-1.0

/*!
 * \file
 * \brief Example to show scoping in the store's objects.
 */

#include "ExampleScope.h"

#include <cstdio>

int main()
{
	stored::ExampleScope e;

	// Notice how the scope separator gets replaced by two underscores.
	printf("/scope/an int by API: %" PRId8 "\n", e.scope__an_int.get());
	printf("/scope/an int by find(): %" PRId8 "\n",
	       e.find("/scope/an int").variable<int8_t>().get());
	printf("G = %g\n", e.gravitational_constant_m__s_2.get());

	return 0;
}
