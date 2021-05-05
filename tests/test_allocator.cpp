/*
 * libstored, distributed debuggable data stores.
 * Copyright (C) 2020-2021  Jochem Rutgers
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "libstored/allocator.h"
#include "gtest/gtest.h"

namespace {

static bool callable_flag;
static void callable() {
	callable_flag = true;
}

TEST(Callable, FunctionPointer) {
	stored::Callable<void()>::type f;
	EXPECT_FALSE((bool)f);

	f = &callable;
	EXPECT_TRUE((bool)f);

	f();
	EXPECT_TRUE(callable_flag);

	callable_flag = false;
	f = nullptr;
	EXPECT_FALSE((bool)f);
	f();
	EXPECT_FALSE(callable_flag);

	stored::Callable<void()>::type g{&callable};
	EXPECT_TRUE((bool)g);
	g();
	EXPECT_TRUE(callable_flag);
}

} // namespace

