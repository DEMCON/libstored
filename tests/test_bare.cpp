// SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
//
// SPDX-License-Identifier: MPL-2.0

#include "TestStore.h"

#include <stored>

#include "gtest/gtest.h"

namespace {

// It is easy to forget to handle all throw statements correctly in case we build without exception
// support. This test just makes sure that everything compiles in this non-common case.

TEST(Bare, Exceptions)
{
#if defined(STORED_COMPILER_GCC) && defined(__cpp_exceptions)
	FAIL() << "For this test, disable exception support.";
#endif

#ifdef STORED_cpp_exceptions
	FAIL() << "libstored has exceptions enabled anyway.";
#else
	SUCCEED();
#endif
}

TEST(Bare, RTTI)
{
#if defined(STORED_COMPILER_GCC) && defined(__cpp_rtti)
	FAIL() << "For this test, disable run-time type information support.";
#endif

#ifdef STORED_cpp_rtti
	FAIL() << "libstored has RTTI enabled anyway.";
#else
	SUCCEED();
#endif
}

} // namespace
