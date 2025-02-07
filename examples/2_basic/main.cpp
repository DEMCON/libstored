// SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
//
// SPDX-License-Identifier: CC0-1.0

/*!
 * \file
 * \brief Basic variable example.
 */

#include "ExampleBasic.h"

#include <cstdio>

int main()
{
	stored::ExampleBasic e;

	// Initialized value:
	printf("test42=%" PRId8 "\n", e.test42.get());

	// The thing with the space around the =
	printf("_42_test42=%" PRId8 "\n", e._42_test42.get());

	// The initialized array element:
	printf("three ints[0]=%" PRId16 "\n", e.three_ints_0.get());

	// Next element through array-like access:
	printf("three ints[1]=%" PRId16 "\n", e.three_ints_a(1).get<int16_t>());

	// An initialized string:
	char txt[8];
	e.string_hello.get(txt, 8);
	printf("string hello='%s'\n", txt);

	// Change a value by direct assignment:
	e.f = 3.14F;

	// Or using a setter:
	e.d.set(2.718);

	// Same for array elements (which are really just variables):
	e.four_ints_0 = 66;

	// Or through the array accessor:
	e.four_ints_a(1).set(67);

	// Write to array with:
	const char new_txt[] = "apples";
	e.string_hello.set(new_txt, sizeof(new_txt));

	return 0;
}
