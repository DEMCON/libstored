// SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
//
// SPDX-License-Identifier: CC0-1.0

// All previous examples had normal variables, which do not have side effects
// when read or written. If a side effect is required, one can define a
// function.  For this, surround the type with ( and ). Read it as a function
// that has an argument of the given type. Functions can be an array, but
// cannot have an initializer.

// The side effect is to be defined in your application. See main.cpp.

(uint64) time (s)   // The number of seconds since the Epoch.
(int32) rand		// A pseudo-random number.
(int32)[2] echo	    // Print every value written to it.

