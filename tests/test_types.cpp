/*
 * libstored, a Store for Embedded Debugger.
 * Copyright (C) 2020  Jochem Rutgers
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

#include <stored>

#include "TestStore.h"
#include "gtest/gtest.h"

#ifdef STORED_OS_WINDOWS
#  include <malloc.h>
#  ifndef alloca
#    define alloca(s) _malloca(s)
#  endif
#else
#  include <alloca.h>
#endif

namespace {

TEST(Types, Int8) {
	stored::TestStore store;
	EXPECT_EQ(store.default_int8.get(), 0);
	store.default_int8 = 42;
	EXPECT_EQ(store.default_int8.get(), 42);
}

TEST(Types, Int16) {
	stored::TestStore store;
	EXPECT_EQ(store.default_int16.get(), 0);
	store.default_int16 = 0x1234;
	EXPECT_EQ(store.default_int16.get(), 0x1234);
	store.default_int16 = -100;
	EXPECT_EQ(store.default_int16.get(), -100);
}

TEST(Types, Int32) {
	stored::TestStore store;
	EXPECT_EQ(store.default_int32.get(), 0);
	store.default_int32 = 0x7abcdef0;
	EXPECT_EQ(store.default_int32.get(), 0x7abcdef0);
}

TEST(Types, Int64) {
	stored::TestStore store;
	EXPECT_EQ(store.default_int64.get(), 0);
	store.default_int64 = 0x0123456789abcdefll;
	EXPECT_EQ(store.default_int64.get(), 0x123456789abcdefll);
}

TEST(Types, Uint8) {
	stored::TestStore store;
	EXPECT_EQ(store.default_uint8.get(), 0);
	store.default_uint8 = 42;
	EXPECT_EQ(store.default_uint8.get(), 42);
}

TEST(Types, Uint16) {
	stored::TestStore store;
	EXPECT_EQ(store.default_uint16.get(), 0);
	store.default_uint16 = 0x1234;
	EXPECT_EQ(store.default_uint16.get(), 0x1234);
}

TEST(Types, Uint32) {
	stored::TestStore store;
	EXPECT_EQ(store.default_uint32.get(), 0);
	store.default_uint32 = 0x8abcdef0;
	EXPECT_EQ(store.default_uint32.get(), 0x8abcdef0);
}

TEST(Types, Uint64) {
	stored::TestStore store;
	EXPECT_EQ(store.default_uint64.get(), 0);
	store.default_uint64 = 0xf123456789abcdefull;
	EXPECT_EQ(store.default_uint64.get(), 0xf123456789abcdefull);
}

TEST(Types, Float) {
	stored::TestStore store;
	EXPECT_EQ(store.default_float.get(), 0);
	store.default_float = 3.14f;
	EXPECT_FLOAT_EQ(store.default_float.get(), 3.14f);
}

TEST(Types, Double) {
	stored::TestStore store;
	EXPECT_EQ(store.default_double.get(), 0);
	store.default_double = 3.14;
	EXPECT_DOUBLE_EQ(store.default_double.get(), 3.14);
}

TEST(Types, Bool) {
	stored::TestStore store;
	EXPECT_FALSE(store.default_bool.get());
	store.default_bool = true;
	EXPECT_TRUE(store.default_bool.get());
}

TEST(Types, Pointer) {
	stored::TestStore store;
#if (defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 8) || defined(_WIN64)
	EXPECT_FALSE(store.default_ptr64.get());
	void* p = reinterpret_cast<void*>((uintptr_t)0xcafebabel);
	store.default_ptr64 = p;
	EXPECT_EQ(store.default_ptr64.get(), p);
#else
	EXPECT_FALSE(store.default_ptr32.get());
	void* p = reinterpret_cast<void*>((uintptr_t)0xcafebabel);
	store.default_ptr32 = p;
	EXPECT_EQ(store.default_ptr32.get(), p);
#endif
}

TEST(Types, Blob) {
	stored::TestStore store;
	size_t s = store.default_blob.size();
	char* buffer1 = (char*)alloca(s);
	memset(buffer1, 0, s);
	char* buffer2 = (char*)alloca(s);
	EXPECT_EQ(store.default_blob.get(buffer2, s), s);
	EXPECT_EQ(memcmp(buffer1, buffer2, s), 0);

	for(size_t i = 0; i < s; i++)
		buffer1[i] = (char)(i + 1);

	EXPECT_EQ(store.default_blob.set(buffer1, s), s);
	EXPECT_EQ(store.default_blob.get(buffer2, s), s);
	EXPECT_EQ(memcmp(buffer1, buffer2, s), 0);
}

TEST(Types, String) {
	stored::TestStore store;
	size_t s = store.default_string.size();
	char* buffer1 = (char*)alloca(s + 1);
	memset(buffer1, 0, s + 1);
	char* buffer2 = (char*)alloca(s + 1);
	memset(buffer2, 0, s + 1);
	EXPECT_EQ(store.default_string.get(buffer2, s), 0);

	for(size_t i = 0; i < s + 1; i++)
		buffer1[i] = 'a';

	EXPECT_EQ(store.default_string.set(buffer1, s), s);
	EXPECT_EQ(store.default_string.get(buffer2, s), s);
	EXPECT_EQ(memcmp(buffer1, buffer2, s), 0);
	EXPECT_EQ(strlen(buffer2), s);
	EXPECT_EQ(strlen(static_cast<char*>(store.default_string.buffer())), s);

	ASSERT_TRUE(s >= 4);
	memcpy(buffer1, "a\0b\0", 4);
	EXPECT_EQ(store.default_string.set(buffer1, s), 1);
	EXPECT_EQ(store.default_string.get(buffer2, s), 1);
}

} // namespace

