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

	callable_flag = false;
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

	stored::Callable<void()>::type h;
	h = callable;
	EXPECT_TRUE((bool)h);
	h = (void(*)())nullptr;
	EXPECT_FALSE((bool)h);
}

TEST(Callable, Lambda) {
	// Decays to normal function pointer.
	callable_flag = false;
	stored::Callable<void()>::type f{[](){ callable_flag = true; }};

	EXPECT_TRUE((bool)f);
	f();
	EXPECT_TRUE(callable_flag);

	// Lambda with one capture.
	bool flag = false;
	f = [&](){ flag = true; };
	EXPECT_TRUE((bool)f);
	f();
	EXPECT_TRUE(flag);

	bool flag0 = false, flag1 = false, flag2 = false, flag3 = false,
		 flag4 = false, flag5 = false, flag6 = false, flag7 = false;

	// This lamba is larger than what fits in the Callable.
	auto lambda = [&]() {
		flag0 = true; flag1 = true; flag2 = true; flag3 = true;
		flag4 = true; flag5 = true; flag6 = true; flag7 = true;
	};
	EXPECT_GE(sizeof(lambda), sizeof(bool*) * 8U);
	EXPECT_GT(sizeof(lambda), sizeof(f));
	f = lambda;

	f();
	EXPECT_TRUE(flag0);
	EXPECT_TRUE(flag1);
	EXPECT_TRUE(flag2);
	EXPECT_TRUE(flag3);
	EXPECT_TRUE(flag4);
	EXPECT_TRUE(flag5);
	EXPECT_TRUE(flag6);
	EXPECT_TRUE(flag7);
}

TEST(Callable, Functor) {
	struct C {
		void operator()() { count++; }
		int count = 0;
	};

	C c;
	c();

	stored::Callable<void()>::type f{c};
	f();
	EXPECT_EQ(c.count, 2);
}

TEST(Callable, Move) {
	bool flag = false;
	stored::Callable<void()>::type f{[&](){ flag = true; }};
	f();
	EXPECT_TRUE(flag);

	stored::Callable<void()>::type g{std::move(f)};
	flag = false;
	f();
	EXPECT_FALSE(flag);
	g();
	EXPECT_TRUE(flag);

	flag = false;
	f = std::move(g);
	g();
	EXPECT_FALSE(flag);
	f();
	EXPECT_TRUE(flag);

	bool flag0 = false, flag1 = false, flag2 = false, flag3 = false,
		 flag4 = false, flag5 = false, flag6 = false, flag7 = false;

	auto lambda = [&]() {
		flag0 = true; flag1 = true; flag2 = true; flag3 = true;
		flag4 = true; flag5 = true; flag6 = true; flag7 = true;
	};
	EXPECT_GT(sizeof(lambda), sizeof(f));
	f = lambda;
	g = std::move(f);

	f();
	EXPECT_FALSE(flag0);
	g();
	EXPECT_TRUE(flag0);
	EXPECT_TRUE(flag1);
	EXPECT_TRUE(flag2);
	EXPECT_TRUE(flag3);
	EXPECT_TRUE(flag4);
	EXPECT_TRUE(flag5);
	EXPECT_TRUE(flag6);
	EXPECT_TRUE(flag7);
}

TEST(Callable, Copy) {
	bool flag = false;
	stored::Callable<void()>::type f{[&](){ flag = true; }};
	f();
	EXPECT_TRUE(flag);

	stored::Callable<void()>::type g{f};
	flag = false;
	f();
	EXPECT_TRUE(flag);
	flag = false;
	g();
	EXPECT_TRUE(flag);

	flag = false;
	f = nullptr;
	f = g;
	g();
	EXPECT_TRUE(flag);
	flag = false;
	f();
	EXPECT_TRUE(flag);
	// There shouldn't be a relation between g and f.
	f = nullptr;
	flag = false;
	g();
	EXPECT_TRUE(flag);

	bool flag0 = false, flag1 = false, flag2 = false, flag3 = false,
		 flag4 = false, flag5 = false, flag6 = false, flag7 = false;

	auto lambda = [&]() {
		flag0 = true; flag1 = true; flag2 = true; flag3 = true;
		flag4 = true; flag5 = true; flag6 = true; flag7 = true;
	};
	EXPECT_GT(sizeof(lambda), sizeof(f));
	f = lambda;
	g = f;

	g();
	EXPECT_TRUE(flag0);
	EXPECT_TRUE(flag1);
	EXPECT_TRUE(flag2);
	EXPECT_TRUE(flag3);
	EXPECT_TRUE(flag4);
	EXPECT_TRUE(flag5);
	EXPECT_TRUE(flag6);
	EXPECT_TRUE(flag7);

	flag0 = false;
	f();
	EXPECT_TRUE(flag0);
}

TEST(Callable, Args) {
	int v = 0;
	stored::Callable<void(int)>::type f{[&](int x){ v = x; }};
	f(1);
	EXPECT_EQ(v, 1);

	stored::Callable<void(int,int)>::type g{[&](int a, int b){ v = a + b; }};
	g(2, 3);
	EXPECT_EQ(v, 5);

	static int copies = 0;
	struct C {
		C() = default;
		C(C const&) { copies++; }
	};

	stored::Callable<void(C)>::type h{[](C){}};
	copies = 0;
	C c;
	h(c);
	EXPECT_EQ(copies, 1);
	h(C());
	EXPECT_EQ(copies, 2);

	stored::Callable<void(C&)>::type i{[](C&){}};
	copies = 0;
	i(c);
	EXPECT_EQ(copies, 0);

	stored::Callable<void(C&&)>::type j{[](C&&){}};
	copies = 0;
	j(std::move(c));
	EXPECT_EQ(copies, 0);
}

TEST(Callable, Return) {
	stored::Callable<int(int*)>::type f{[](int* x){ return *x; }};

	int i = 4;
	EXPECT_EQ(f(&i), 4);
}

} // namespace

