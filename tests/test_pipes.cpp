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
#include "gtest/gtest.h"

namespace {

TEST(Pipes, Basic)
{
	using namespace stored::pipes;

	auto p0 = Entry<int>{} >> Exit{};
	p0.inject(0);

	auto p1 = Entry<int>{} >> Identity<int>{} >> Exit{};
	p1.inject(1);

	// Pipes are copyable.
	auto p1_copy = p1;

	// Pipes are movable.
	auto p1_move = std::move(p1);
	p1_move.inject(2);
}

TEST(Pipes, Connect)
{
	using namespace stored::pipes;

	auto p1 = Entry<int>{} >> Log<int>("p1") >> Buffer<int>{} >> Exit{};
	auto p2 = Entry<int>{} >> Log<int>("p2") >> Buffer<int>{} >> Exit{};
	auto p3 = Entry<int>{} >> Log<int>("p3") >> Buffer<int>{} >> Exit{};

	p1 >> p2 >> p3;

	p1.inject(1);
	EXPECT_EQ(p3.extract(), 1);

	p1 >> p3;
	p1.inject(2);
	EXPECT_EQ(p2.extract(), 1);
	EXPECT_EQ(p3.extract(), 2);
}

TEST(Pipes, Misc)
{
	auto c = stored::pipes::Entry<int>{} >> stored::pipes::Identity<int>{}
		 >> stored::pipes::Cast<int, double>{} >> stored::pipes::Buffer<double>{11.0}
		 >> stored::pipes::Identity<double>{} >> stored::pipes::Exit{};

	auto s = stored::pipes::Entry<int>{} >> stored::pipes::Tee{c}
		 >> stored::pipes::Log<int>("s") >> stored::pipes::Exit{};
	auto b = stored::pipes::Entry<int>{} >> stored::pipes::Exit{};

	printf("%g\n", c.extract());
	printf("%g\n", c.inject(123456789));
	printf("%g\n", c.extract());
	s(5);
	printf("%g\n", c.extract());
}

} // namespace
