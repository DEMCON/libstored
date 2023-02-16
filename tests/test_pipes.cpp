/*
 * libstored, distributed debuggable data stores.
 * Copyright (C) 2020-2022  Jochem Rutgers
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "libstored/pipes.h"
#include "TestStore.h"

#include <thread>

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
	EXPECT_EQ(p.extract(), 2U);

	5.8 >> p; // saturated_cast rounds instead of static_cast's truncate.
	EXPECT_EQ(p.extract(), 6U);

	-3.1 >> p;
	EXPECT_EQ(p.extract(), 0U);
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

	store.init_decimal = 41;
	bool triggered = false;
	p1.trigger(&triggered);
	EXPECT_EQ(p1.extract(), 41);
	EXPECT_TRUE(triggered);

	// Auto-deduct StoreVariable
	auto p2 = Entry<bool>{} >> Get{store.init_decimal} >> Exit{};
	store.init_decimal = 43;
	EXPECT_EQ(p2.extract(), 43);

	// Auto-deduct StoreFunction
	auto p3 = Entry<bool>{} >> Get{store.f_read_only} >> Exit{};
	EXPECT_EQ(p3.extract(), (unsigned short)0U);
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

	auto p1 = Entry<int32_t>{}
		  >> Set<int32_t, decltype(store.init_decimal)&>{store.init_decimal} >> Exit{};

	2 >> p1;
	EXPECT_EQ(p1.extract(), 2);
	EXPECT_EQ(store.init_decimal.get(), 2);

	// Auto-deduct StoreVariable
	auto p2 = Entry<int32_t>{} >> Set{store.init_decimal} >> Exit{};
	3 >> p2;
	EXPECT_EQ(p2.extract(), 3);
	EXPECT_EQ(store.init_decimal.get(), 3);

	// Auto-deduct StoreFunction
	auto p3 = Entry<int16_t>{} >> Set{store.f_read_only} >> Exit{};
	4 >> p3;
	EXPECT_EQ(p3.extract(), (uint16_t)0);
}

TEST(Pipes, Mux)
{
	using namespace stored::pipes;

	auto p0 = Entry<short>{} >> Buffer<short>{(short)10} >> Exit{};
	auto p1 = Entry<short>{} >> Buffer<short>{(short)11} >> Exit{};
	auto p2 = Entry<short>{} >> Buffer<short>{(short)12} >> Exit{};
	auto mux = Entry<size_t>{} >> Mux{p0, p1, p2} >> Exit{};

	EXPECT_EQ(mux.extract(), (short)10);

	1 >> mux;
	EXPECT_EQ(mux.extract(), (short)11);

	2 >> mux;
	EXPECT_EQ(mux.extract(), (short)12);

	3 >> mux;
	EXPECT_EQ(mux.extract(), (short)0);

	// The one-input mux is optimized by ignoring the index.
	auto mux1 = Entry<size_t>{} >> Mux{p0} >> Exit{};
	EXPECT_EQ(mux1.extract(), (short)10);
	1 >> mux1;
	EXPECT_EQ(mux1.extract(), (short)10);
}

TEST(Pipes, Cache)
{
	using namespace stored::pipes;

	auto candidate = Entry<int>{} >> Log<int>{"candidate"} >> Buffer<int>{} >> Exit{};
	auto actual = Entry<int>{} >> Log<int>{"actual"} >> Buffer<int>{} >> Exit{};
	auto cache = Entry<size_t>{} >> Mux{actual, candidate} >> Buffer<int>{} >> Log<int>{"set"}
		     >> Exit{};

	10 >> actual;
	0 >> cache;
	EXPECT_EQ(cache.extract(), 10);

	1 >> candidate;
	EXPECT_EQ(cache.extract(), 10);
	2 >> candidate;
	EXPECT_EQ(cache.extract(), 10);
	1 >> cache;
	EXPECT_EQ(cache.extract(), 2);
	3 >> candidate;
	EXPECT_EQ(cache.extract(), 2);
	0 >> cache;
	EXPECT_EQ(cache.extract(), 10);
}

TEST(Pipes, Struct)
{
	using namespace stored::pipes;

	static int copies = 0;
	static int moves = 0;

	struct Test {
		int i = 0;

		Test(int i_ = 0)
			: i{i_}
		{}

		Test(Test&& t) noexcept
		{
			*this = std::move(t);
		}

		Test(Test const& t) noexcept
		{
			*this = t;
		}

		Test& operator=(Test&& t) noexcept
		{
			i = t.i;
			moves++;
			return *this;
		}

		Test& operator=(Test const& t) noexcept
		{
			i = t.i;
			copies++;
			return *this;
		}

		~Test() = default;
	};

	auto p = Entry<Test>{} >> Call{[](Test const& t) { printf("%d\n", t.i); }}
		 >> Buffer<Test>{Test{0}} >> Exit{};

	EXPECT_EQ(p.extract().get().i, 0);
	printf("copies: %d, moves: %d\n", copies, moves);

	Test{1} >> p;
	EXPECT_EQ(p.extract().get().i, 1);
	printf("copies: %d, moves: %d\n", copies, moves);

	Test t{2};
	t >> p;
	EXPECT_EQ(p.extract().get().i, 2);
	printf("copies: %d, moves: %d\n", copies, moves);
}

TEST(Pipes, similar_to)
{
	using namespace stored::pipes;

	EXPECT_TRUE((similar_to<int, 1>{}(0, 0)));
	EXPECT_TRUE((similar_to<int, 1>{}(10, 10)));
	EXPECT_TRUE((similar_to<int, 1>{}(10, 11)));
	EXPECT_FALSE((similar_to<int, 1>{}(10, 12)));
	EXPECT_TRUE((similar_to<int, 1>{}(11, 10)));
	EXPECT_TRUE((similar_to<int, 1>{}(-10, -11)));

	EXPECT_TRUE((similar_to<double, 2>{}(-10, -10.09)));
	EXPECT_FALSE((similar_to<double, 2>{}(-10, -10.11)));
	EXPECT_FALSE((similar_to<double, 2>{}(-10, std::numeric_limits<double>::quiet_NaN())));
	EXPECT_TRUE((similar_to<double, 2>{}(
		std::numeric_limits<double>::quiet_NaN(),
		std::numeric_limits<double>::quiet_NaN())));
	EXPECT_TRUE((similar_to<double, 2>{}(
		std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity())));
	EXPECT_FALSE((similar_to<double, 2>{}(1, std::numeric_limits<double>::infinity())));
	EXPECT_FALSE((similar_to<double, 2>{}(std::numeric_limits<double>::infinity(), 1)));
}

TEST(Pipes, Changes)
{
	using namespace stored::pipes;

	int changes = 0;
	auto c0 = Entry<int>{} >> Call{[&](int) { changes++; }} >> Exit{};
	auto p0 = Entry<int>{} >> Changes{c0} >> Exit{};

	0 >> p0;
	EXPECT_EQ(changes, 0);

	1 >> p0;
	EXPECT_EQ(changes, 1);

	1 >> p0;
	EXPECT_EQ(changes, 1);

	changes = 0;
	auto c1 = Entry<double>{} >> Call{[&](double) { changes++; }} >> Exit{};
	auto p1 = Entry<double>{} >> Changes{c1, similar_to<double>{}} >> Exit{};

	0 >> p1;
	EXPECT_EQ(changes, 0);

	1 >> p1;
	EXPECT_EQ(changes, 1);

	1.0001 >> p1;
	EXPECT_EQ(changes, 1);
}

TEST(Pipes, Constrained)
{
	using namespace stored::pipes;

	auto p = Entry<double>{} >> Constrained{Bounded{-1.0, 4.5}} >> Buffer<double>{} >> Cap{};

	1 >> p;
	EXPECT_EQ(p.extract().get(), 1.0);

	-2 >> p;
	EXPECT_EQ(p.extract().get(), -1.0);

	4.6 >> p;
	EXPECT_EQ(p.extract().get(), 4.5);

	3 >> p;
	EXPECT_EQ(p.extract().get(), 3.0);
}

TEST(Pipes, Convert)
{
	using namespace stored::pipes;

	auto p = Entry<double>{} >> Convert{Scale<double, std::milli>{}} >> Buffer<double>{} >> Cap{};

	1 >> p;
	EXPECT_EQ(p.extract().get(), 1e-3);

	EXPECT_EQ(p.entry_cast(4e-3), 4.0);
}

TEST(Pipes, IndexMap)

{
	using namespace stored::pipes;

	auto p0 = Entry<size_t>{} >> Mapped<int, int, IndexMap<int, 4>>{10, 20, 30, 40} >> Cap{};

	int v = 0 >> p0;
	EXPECT_EQ(v, 10);

	v = 2 >> p0;
	EXPECT_EQ(v, 30);

	v = 5 >> p0;
	EXPECT_EQ(v, 10);



	auto p1 = Entry<size_t>{} >> Map({10, 20, 30, 40}) >> Cap{};

	v = 0 >> p1;
	EXPECT_EQ(v, 10);

	v = 2 >> p1;
	EXPECT_EQ(v, 30);

	v = 5 >> p1;
	EXPECT_EQ(v, 10);



	auto p2 = Entry<size_t>{} >> Map<long>({10L, 20L, 30L, 40L}) >> Cap{};

	long v2 = 0 >> p2;
	EXPECT_EQ(v2, 10L);

	v2 = 2 >> p2;
	EXPECT_EQ(v2, 30L);

	v2 = 5 >> p2;
	EXPECT_EQ(v, 10L);



	auto p3 = Entry<size_t>{} >> Map(10L, 20L, 30L, 40L) >> Cap{};

	v2 = 0 >> p3;
	EXPECT_EQ(v2, 10L);

	v2 = 2 >> p3;
	EXPECT_EQ(v2, 30L);

	v2 = 5 >> p3;
	EXPECT_EQ(v, 10L);



	struct comp {
		bool operator()(long a, long b) const
		{
			return a == b + 1;
		}
	};

	auto p4 = Entry<size_t>{} >> Map({10L, 20L, 30L, 40L}, comp{}) >> Cap{};

	EXPECT_EQ(p4.entry_cast(29L), 2);
	EXPECT_EQ(p4.entry_cast(25L), 0);
}

TEST(Pipes, OrderedMap)
{
	using namespace stored::pipes;

	auto p0 = Entry<int>{} >> Map<int, int>({{0, 10}, {1, 20}, {5, 30}, {100, 40}}) >> Cap{};

	int v0 = 0 >> p0;
	EXPECT_EQ(v0, 10);

	v0 = 1 >> p0;
	EXPECT_EQ(v0, 20);

	v0 = 2 >> p0;
	EXPECT_EQ(v0, 10);

	v0 = 5 >> p0;
	EXPECT_EQ(v0, 30);

	v0 = 1000 >> p0;
	EXPECT_EQ(v0, 10);



	auto p1 = Entry<unsigned>{}
		  >> Map<unsigned, float>({{0U, 10.F}, {1U, 20.F}, {5U, 30.F}, {100U, 40.F}})
		  >> Cap{};

	float v1 = 0 >> p1;
	EXPECT_EQ(v1, 10.F);

	v1 = 1U >> p1;
	EXPECT_EQ(v1, 20.F);

	v1 = 2U >> p1;
	EXPECT_EQ(v1, 10.F);

	v1 = 5U >> p1;
	EXPECT_EQ(v1, 30.F);

	v1 = 1000U >> p1;
	EXPECT_EQ(v1, 10.F);

	EXPECT_EQ(p1.entry_cast(30.F), 5U);
	EXPECT_EQ(p1.entry_cast(25.F), 0U);
}

TEST(Pipes, RandomMap)
{
	using namespace stored::pipes;

	auto p0 = Entry<int>{}
		  >> Mapped(make_random_map<int, int>({{1, 20}, {0, 10}, {100, 40}, {5, 30}}))
		  >> Cap{};

	int v0 = 0 >> p0;
	EXPECT_EQ(v0, 10);

	v0 = 1 >> p0;
	EXPECT_EQ(v0, 20);

	v0 = 2 >> p0;
	EXPECT_EQ(v0, 20);

	v0 = 5 >> p0;
	EXPECT_EQ(v0, 30);

	v0 = 1000 >> p0;
	EXPECT_EQ(v0, 20);
}

#ifndef STORED_COMPILER_MINGW
// MinGW does not implement std::thread.
TEST(Pipes, RateLimit)
{
	using namespace stored::pipes;
	using namespace std::literals::chrono_literals;

	auto out = Entry<int>{} >> Buffer<int>{} >> Cap{};
	auto in = Entry<int>{} >> RateLimit{out, 50ms} >> Buffer<int>{} >> Cap{};

	1 >> in;
	EXPECT_EQ(in.extract(), 1);
	EXPECT_EQ(out.extract(), 1);

	2 >> in;
	EXPECT_EQ(in.extract(), 2);
	EXPECT_EQ(out.extract(), 1); // suppressed

	std::this_thread::sleep_for(100ms);

	3 >> in;
	EXPECT_EQ(in.extract(), 3);
	EXPECT_EQ(out.extract(), 3); // pass-through, but start timer

	4 >> in;
	EXPECT_EQ(in.extract(), 4);
	EXPECT_EQ(out.extract(), 3);

	5 >> in;
	EXPECT_EQ(in.extract(), 5);
	EXPECT_EQ(out.extract(), 3);

	bool triggered = false;
	in.trigger(&triggered);
	EXPECT_EQ(out.extract(), 3); // no time to trigger yet
	EXPECT_FALSE(triggered);

	std::this_thread::sleep_for(100ms);

	triggered = false;
	in.trigger(&triggered);
	EXPECT_EQ(out.extract(), 5); // should be there now
	EXPECT_TRUE(triggered);

	triggered = false;
	in.trigger(&triggered);
	EXPECT_EQ(out.extract(), 5);
	EXPECT_FALSE(triggered); // nothing to trigger

	5 >> in;
	EXPECT_EQ(in.extract(), 5);
	EXPECT_EQ(out.extract(), 5); // nothing changed, no timer

	6 >> in;
	EXPECT_EQ(in.extract(), 6);
	EXPECT_EQ(out.extract(), 6);
}
#endif // STORED_COMPILER_MINGW

} // namespace
