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

#include "libstored/spm.h"
#include "gtest/gtest.h"

namespace {

TEST(ScrachPad, Alloc)
{
	stored::ScratchPad<> spm;

	EXPECT_EQ(spm.chunks(), 0);
	EXPECT_EQ(spm.size(), 0);
	EXPECT_EQ(spm.max(), 0);

	// First chunk alloc.
	void** a = spm.alloc<void*>();
	EXPECT_NE(a, nullptr);
	EXPECT_EQ(spm.chunks(), 1);
	EXPECT_EQ(spm.size(), sizeof(void*));
	EXPECT_EQ(spm.max(), sizeof(void*));
	EXPECT_GE(spm.capacity(), sizeof(void*));

	// Second chunk alloc.
	void** b = spm.alloc<void*>();
	EXPECT_NE(b, nullptr);
	EXPECT_NE(a, b);
	EXPECT_EQ(spm.size(), sizeof(void*) * 2);
	EXPECT_EQ(spm.max(), sizeof(void*) * 2);
	EXPECT_GE(spm.capacity(), sizeof(void*) * 2);

	// Random allocs.
	EXPECT_NE(spm.alloc<void*>(10), nullptr);
	EXPECT_NE(spm.alloc<std::string>(2), nullptr);
	EXPECT_NE(spm.alloc<char>(100), nullptr);
	EXPECT_NE(spm.alloc<float>(3), nullptr);
}

template <size_t S>
void spmInfo(stored::ScratchPad<S>& spm)
{
	printf("%p: buffer=%p size=%zu cap=%zu max=%zu chunks=%zu\n",
		&spm, spm.template alloc<char>(0), spm.size(), spm.capacity(), spm.max(), spm.chunks());
}

TEST(ScratchPad, Reset)
{
	stored::ScratchPad<> spm;

	// Empty reset.
	spm.reset();
	EXPECT_EQ(spm.chunks(), 0);
	EXPECT_EQ(spm.size(), 0);
	EXPECT_EQ(spm.max(), 0);

	// First chunk reset.
	int* i = spm.alloc<int>();
	EXPECT_NE(i, nullptr);
	*i = 42;
	spm.reset();
	spmInfo(spm);
	EXPECT_EQ(spm.chunks(), 1);
	EXPECT_EQ(spm.size(), 0);
	EXPECT_EQ(spm.max(), sizeof(int));

	// Force an additional chunk.
	i = spm.alloc<int>();
	EXPECT_NE(i, nullptr);
	spmInfo(spm);
	char* p = spm.alloc<char>(spm.capacity() - spm.size() + 1);
	size_t total = spm.size();
	spmInfo(spm);
	EXPECT_NE(p, nullptr);
	EXPECT_EQ(spm.chunks(), 2);
	spm.reset();
	spmInfo(spm);
	EXPECT_EQ(spm.chunks(), 1);
	EXPECT_EQ(spm.size(), 0);
	EXPECT_GE(spm.capacity(), total);
}

TEST(ScratchPad, Alignment)
{
	stored::ScratchPad<> spm;
	spm.reserve(sizeof(int) * 2 + sizeof(double) * 2);

	// 1 byte
	char* c = spm.alloc<char>();
	EXPECT_NE(c, nullptr);

	// Add padding bytes for int
	int* i = spm.alloc<int>();
	EXPECT_NE(i, nullptr);
	EXPECT_EQ((uintptr_t)i & (sizeof(int) - 1), 0);
	EXPECT_EQ(spm.size(), sizeof(int) * 2);

	// Another few bytes
	c = spm.alloc<char>();
	EXPECT_NE(c, nullptr);
	c = spm.alloc<char>();
	EXPECT_NE(c, nullptr);
	c = spm.alloc<char>();
	EXPECT_NE(c, nullptr);

	// More padding for double
	double* d = spm.alloc<double>();
	EXPECT_NE(d, nullptr);
	EXPECT_EQ((uintptr_t)d & (sizeof(double) - 1), 0);
	EXPECT_EQ(spm.size(), sizeof(int) * 2 + sizeof(void*) + sizeof(double));
}

TEST(ScratchPad, Snapshot)
{
	stored::ScratchPad<> spm;
	spm.reserve(sizeof(double) * 8);

	auto c = spm.alloc<char>();
	EXPECT_NE(c, nullptr);
	EXPECT_EQ(spm.size(), 1);

	// Rollback within same chunk.
	auto s1 = spm.snapshot();
	c = spm.alloc<char>();
	EXPECT_NE(c, nullptr);
	EXPECT_EQ(spm.size(), 2);
	s1.rollback();
	EXPECT_EQ(spm.size(), 1);

	auto d = spm.alloc<double>();
	EXPECT_NE(d, nullptr);
	EXPECT_EQ(spm.size(), sizeof(void*) + sizeof(double));
	s1.rollback();
	EXPECT_EQ(spm.size(), 1);

	// Rollback to previous chunk.
	c = spm.alloc<char>(spm.capacity() - spm.size() + 1);
	EXPECT_NE(c, nullptr);
	EXPECT_EQ(spm.chunks(), 2);
	s1.rollback();
	EXPECT_EQ(spm.size(), 1);
	EXPECT_EQ(spm.chunks(), 1);
}

TEST(ScratchPad, Shrink)
{
	stored::ScratchPad<> spm;

	auto i = spm.alloc<int>();
	EXPECT_NE(i, nullptr);
	spm.shrink_to_fit();
	EXPECT_EQ(spm.max(), sizeof(int));

	auto s = spm.snapshot();
	i = spm.alloc<int>();
	s.rollback();
	spm.shrink_to_fit();
	EXPECT_EQ(spm.max(), sizeof(int));

	auto c = spm.alloc<char>(spm.capacity() + 1);
	EXPECT_NE(c, nullptr);
	EXPECT_EQ(spm.chunks(), 2);
	s.rollback();
	s.reset();
	EXPECT_EQ(spm.chunks(), 1);

	spm.reset();
	EXPECT_EQ(spm.chunks(), 1);
	spm.shrink_to_fit();
	EXPECT_EQ(spm.chunks(), 0);
	EXPECT_EQ(spm.capacity(), 0);
}

TEST(ScratchPad, Stress)
{
	stored::ScratchPad<> spm;

	srand((unsigned int)time(NULL));

	for(int j = 0; j < 100; j++) {
		spm.reset();
		for(int i = 0; i < 1000; i++) {
			auto c = spm.alloc<char>((size_t)(rand() % 19), (size_t)(rand() % 31));
			EXPECT_NE(c, nullptr);
			if(rand() % 128 == 0)
				spm.reset();
		}
	}

	spmInfo(spm);
}

} // namespace

