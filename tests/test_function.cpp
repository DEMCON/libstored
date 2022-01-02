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

#include "TestStore.h"
#include "gtest/gtest.h"

namespace {

class FunctionTestStore : public STORE_BASE_CLASS(TestStoreBase, FunctionTestStore) {
	STORE_CLASS_BODY(TestStoreBase, FunctionTestStore)
public:
	FunctionTestStore() : m_f_read__write(4) {}

	void __f_read__write(bool set, double& value)
	{
		if(set)
			m_f_read__write = value;
		else
			value = m_f_read__write;
	}

	void __f_read_only(bool set, uint16_t& value)
	{
		if(!set)
			value = saturated_cast<uint16_t>(m_f_read__write);
	}

	size_t __f_write_only(bool set, char* buffer, size_t len)
	{
		if(!set)
			return 0;

		printf("f write-only: %.*s\n", (int)len, buffer);
		return len;
	}

	void __array_f_int_0(bool set, int32_t& value) { if(!set) value = (int32_t)0; }
	void __array_f_int_1(bool set, int32_t& value) { if(!set) value = (int32_t)0; }
	void __array_f_int_2(bool set, int32_t& value) { if(!set) value = (int32_t)0; }
	void __array_f_int_3(bool set, int32_t& value) { if(!set) value = (int32_t)0; }
	size_t __array_f_blob_0(bool UNUSED_PAR(set), void* UNUSED_PAR(value), size_t UNUSED_PAR(len)) { return 0; }
	size_t __array_f_blob_1(bool UNUSED_PAR(set), void* UNUSED_PAR(value), size_t UNUSED_PAR(len)) { return 0; }

private:
	double m_f_read__write;
};

TEST(Function, ReadWrite)
{
	FunctionTestStore store;
	EXPECT_DOUBLE_EQ(store.f_read__write(), 4.0);
	store.f_read__write(5.0);
	EXPECT_DOUBLE_EQ(store.f_read__write(), 5.0);
}

TEST(Function, ReadOnly)
{
	FunctionTestStore store;
	EXPECT_EQ(store.f_read_only(), 4u);
	store.f_read__write(5.6);
	EXPECT_EQ(store.f_read_only(), 6u);
}

TEST(Function, WriteOnly)
{
	FunctionTestStore store;
	char buffer[] = "hi all!";
	EXPECT_EQ(store.f_write_only.get(buffer, sizeof(buffer)), 0);
	EXPECT_EQ(store.f_write_only.set(buffer, strlen(buffer)), 4u);
}

TEST(Function, FreeFunction)
{
	FunctionTestStore store;

	constexpr auto rw = FunctionTestStore::freeFunction<double>("/f read/write");
	static_assert(rw.valid(), "");

	rw.apply(store) = 123.4;
	EXPECT_DOUBLE_EQ(store.f_read__write.get(), 123.4);

	store.f_read__write = 56.7;
	constexpr auto ro = FunctionTestStore::freeFunction<uint16_t>("/f read-only");
	EXPECT_EQ(ro.apply(store).get(), 57);
}

} // namespace

