/*
 * libstored, distributed debuggable data stores.
 * Copyright (C) 2020-2022  Jochem Rutgers
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

#include "libstored/pipes.h"
#include "TestStore.h"

#include "gtest/gtest.h"

namespace {

TEST(Pipes, Size)
{
	using namespace stored::pipes;
	auto p0 = Entry<int>{} >> Cap{};
	auto p1 = Entry<int>{} >> Identity<int>{} >> Cap{};
	auto p2 = Entry<int>{} >> Identity<int>{} >> Identity<int>{} >> Identity<int>{} >> Cap{};
	auto p3 = Entry<int>{} >> Identity<int>{} >> Identity<int>{} >> Identity<int>{}
		  >> Identity<int>{} >> Cap{};

	// Identity does not have data. It should not increase the total size.
	// Only two vptrs are expected (one for Entry, and one for Cap).
	EXPECT_EQ(sizeof(p0), sizeof(p1));
	EXPECT_EQ(sizeof(p0), sizeof(p2));
	EXPECT_EQ(sizeof(p0), sizeof(p3));
}

TEST(Pipes, Copy)
{
	using namespace stored::pipes;

	auto p0 = Entry<int>{} >> Buffer<int>{} >> Exit{};
	p0.inject(1);
	EXPECT_EQ(p0.extract(), 1);

	// Pipes are copyable.
	auto p1 = p0;
	EXPECT_EQ(p1.extract(), 1);

	p0.inject(2);
	EXPECT_EQ(p0.extract(), 2);
	EXPECT_EQ(p1.extract(), 1);
}

TEST(Pipes, Move)
{
	using namespace stored::pipes;

	auto p0 = Entry<int>{} >> Buffer<int>{} >> Exit{};
	p0.inject(1);

	// Pipes are movable.
	auto p1 = std::move(p0);
	EXPECT_EQ(p1.extract(), 1);
}

TEST(Pipes, Connect)
{
	using namespace stored::pipes;

	// A pipe is solid. They cannot be split or combined dynamically.
	//
	// There are two pipe ends: a Cap and an Exit. A Capped pipe does not
	// allow dynamic connections (and is therefor a bit smaller in memory
	// footprint).  An open pipe (by using Exit) allows different pipes to
	// be connected and disconnected dynamically.
	//
	// Injected values flow through the (connected) pipes. But note that
	// value extraction stops at the entry of the extraction pipe.
	auto p1 = Entry<int>{} >> Log<int>("p1") >> Buffer<int>{1} >> Exit{};
	auto p2 = Entry<int>{} >> Log<int>("p2") >> Buffer<int>{2} >> Exit{};
	auto p3 = Entry<int>{} >> Log<int>("p3") >> Buffer<int>{3} >> Exit{};

	// Upon connection, the upstream pipe injects its extraction value into
	// the downstream pipe.
	p1 >> p2 >> p3;

	// This accesses p3's buffer.
	EXPECT_EQ(p3.extract(), 1);

	p1.inject(4);
	EXPECT_EQ(p3.extract(), 4);

	p1 >> p3;
	p1.inject(5);
	EXPECT_EQ(p2.extract(), 4);
	EXPECT_EQ(p3.extract(), 5);

	// A pipe can only connect to one downstream pipe, but one downstream pipe can
	// receive from multiple upstream pipes.
	p1 >> p3;
	p2 >> p3;

	p1.inject(10);
	EXPECT_EQ(p3.extract(), 10);

	p2.inject(11);
	EXPECT_EQ(p3.extract(), 11);

	// Data does not flow back into the other upstream pipe.
	EXPECT_EQ(p1.extract(), 10);
}

TEST(Pipes, Operators)
{
	using namespace stored::pipes;

	auto p = Entry<int>{} >> Buffer<int>{} >> Cap{};

	int i = 1;

	// Inject
	i >> p; // returns the result of inject()
	EXPECT_EQ(p.extract(), 1);

	2 >> p;
	EXPECT_EQ(p.extract(), 2);

	i = 3;
	p << i;
	EXPECT_EQ(p.extract(), 3);

	p << 4;
	EXPECT_EQ(p.extract(), 4);

	// Extract
	p >> i; // returns i as int&
	EXPECT_EQ(i, 4);

	p << 5;
	i << p;
	EXPECT_EQ(i, 5);
}

TEST(Pipes, Tee)
{
	using namespace stored::pipes;

	auto a = Entry<int>{} >> Buffer<int>{} >> Cap{};
	auto b = Entry<int>{} >> Tee{a} >> Cap{};

	1 >> b;
	EXPECT_EQ(a.extract(), 1);

	auto c = a;
	auto d = Entry<int>{} >> Tee{b, c} >> Cap{};

	2 >> d;
	EXPECT_EQ(a.extract(), 2);
	EXPECT_EQ(c.extract(), 2);
}

TEST(Pipes, Cast)
{
	using namespace stored::pipes;

	auto p = Entry<double>{} >> Cast<double, unsigned int>{} >> Buffer<unsigned int>{} >> Cap{};

	2.4 >> p;
	EXPECT_EQ(p.extract(), 2);

	5.8 >> p; // saturated_cast rounds instead of static_cast's truncate.
	EXPECT_EQ(p.extract(), 6);

	-3.1 >> p;
	EXPECT_EQ(p.extract(), 0);
}

TEST(Pipes, Types)
{
	using namespace stored::pipes;

	auto a = Entry<int>{} >> Exit{};
	static_assert(std::is_base_of_v<Pipe<int, int>, decltype(a)>);

	auto b = Entry<int>{} >> Buffer<int>{} >> Exit{};
	static_assert(std::is_base_of_v<Pipe<int, int>, decltype(b)>);
}

TEST(Pipes, Transistor)
{
	using namespace stored::pipes;

	auto AND = [](PipeExit<bool>& a, PipeExit<bool>& b) {
		return Entry<bool>{} >> Transistor<bool>{a} >> Transistor<bool>{b} >> Buffer<bool>{}
		       >> Exit{};
	};

	auto NOT = [](PipeExit<bool>& i) {
		return Entry<bool>{} >> Transistor<bool, true>{i} >> Buffer<bool>{} >> Exit{};
	};

	auto i0 = Entry<bool>{} >> Buffer<bool>{} >> Exit{};
	auto i1 = Entry<bool>{} >> Buffer<bool>{} >> Exit{};

	auto and0 = AND(i0, i1);
	auto not0 = NOT(and0);

	auto and1 = AND(i0, not0);
	auto not1 = NOT(and1);

	auto and2 = AND(i1, not0);
	auto not2 = NOT(and2);

	auto and3 = AND(not1, not2);
	auto o0 = NOT(and3);
	auto& o1 = and0;

	// Create active circuit. Input a 1 to evaluate the output.
	auto half_adder =
		Entry<bool>{} >> Tee{and0, not0, and1, not1, and2, not2, and3, o0} >> Exit{};

	// Set input.
	true >> i0;
	false >> i1;

	// Evaluate half-adder.
	true >> half_adder;
	EXPECT_TRUE(o0.extract());
	EXPECT_FALSE(o1.extract());

	// Set another input.
	true >> i0;
	true >> i1;

	// Evaluate.
	true >> half_adder;
	EXPECT_FALSE(o0.extract());
	EXPECT_TRUE(o1.extract());

	// QED, pipes are functionally complete.
}

TEST(Pipes, Call)
{
	using namespace stored::pipes;

	// Callback by value.
	int sum = 0;
	auto p0 = Entry<int>{} >> Call{[&](int x) { sum += x; }} >> Exit{};

	1 >> p0;
	2 >> p0;

	EXPECT_EQ(sum, 3);


	// Callback by const reference.
	sum = 0;
	auto p1 = Entry<int>{} >> Call{[&](int const& x) { sum += x; }} >> Exit{};

	1 >> p1;
	2 >> p1;

	EXPECT_EQ(sum, 3);


	// Callback by reference.
	auto p2 = Entry<int>{} >> Call{[](int& x) { x++; }} >> Buffer<int>{} >> Exit{};

	1 >> p2;

	EXPECT_EQ(p2.extract(), 2);


	// Callback as filter.
	auto p3 = Entry<int>{} >> Call{[](int x) { return x + 1; }} >> Buffer<int>{} >> Exit{};

	2 >> p3;

	EXPECT_EQ(p3.extract(), 3);
}

TEST(Pipes, Extend)
{
	using namespace stored::pipes;

	int injects = 0;
	auto p0 = Entry<int>{} >> Buffer<int>{} >> Log<int>{"p0"} >> Exit{};
	auto p1 = Entry<int>{} >> Buffer<int>{} >> Log<int>{"p1"} >> Call{[&](int) { injects++; }}
		  >> Exit{};
	p0 >> p1;
	1 >> p0;

	auto p2 = Entry<int>{} >> Buffer<int>{} >> Log<int>{"p2"} >> Exit{};
	2 >> p2;

	// This will actually inject both 2 and 1 into p1.
	injects = 0;
	p0.extend(p2);
	EXPECT_EQ(p2.extract(), 1);
	EXPECT_EQ(p1.extract(), 1);
	EXPECT_EQ(injects, 2);

	auto p3 = Entry<int>{} >> Log<int>{"p3"} >> Exit{};
	// Now, only 1 is injected (again).
	injects = 0;
	p2.extend(p3);
	EXPECT_EQ(p1.extract(), 1);
	EXPECT_EQ(injects, 1);
}

TEST(Pipes, Get)
{
	using namespace stored::pipes;

	stored::TestStore store;

	auto p0 = Entry<bool>{}
		  >> Get<int32_t, stored::Variant<stored::TestStore>>{store.init_decimal.variant()}
		  >> Buffer<int32_t>{} >> Exit{};

	true >> p0;
	EXPECT_EQ(p0.extract(), 42);

	auto p1 = Entry<bool>{} >> Get<int32_t, decltype(store.init_decimal)&>{store.init_decimal}
		  >> Exit{};

	EXPECT_EQ(p1.extract(), 42);

	// Auto-deduct StoreVariable
	auto p2 = Entry<bool>{} >> Get{store.init_decimal} >> Exit{};
	store.init_decimal = 43;
	EXPECT_EQ(p2.extract(), 43);

	// Auto-deduct StoreFunction
	auto p3 = Entry<bool>{} >> Get{store.f_read_only} >> Exit{};
	EXPECT_EQ(p3.extract(), 0);
}

TEST(Pipes, Set)
{
	using namespace stored::pipes;

	stored::TestStore store;

	auto p0 = Entry<int32_t>{}
		  >> Set<int32_t, stored::Variant<stored::TestStore>>{store.init_decimal.variant()}
		  >> Cap{};

	1 >> p0;
	EXPECT_EQ(p0.extract(), 1);
	EXPECT_EQ(store.init_decimal.get(), 1);

	auto p1 = Entry<int32_t>{} >> Set<int32_t, decltype(store.init_decimal)&>{store.init_decimal}
		  >> Exit{};

	2 >> p1;
	EXPECT_EQ(p1.extract(), 2);
	EXPECT_EQ(store.init_decimal.get(), 2);

	// Auto-deduct StoreVariable
	auto p2 = Entry<int32_t>{} >> Set{store.init_decimal} >> Exit{};
	3 >> p2;
	EXPECT_EQ(p2.extract(), 3);
	EXPECT_EQ(store.init_decimal.get(), 3);

	// Auto-deduct StoreFunction
	auto p3 = Entry<int32_t>{} >> Set{store.f_read_only} >> Exit{};
	4 >> p3;
	EXPECT_EQ(p3.extract(), 0);
}

} // namespace
