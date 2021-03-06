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

#include "TestStore.h"
#include "gtest/gtest.h"

#include <algorithm>
#include <list>
#include <functional>

namespace {

TEST(Directory, FullMatch) {
	stored::TestStore store;
	EXPECT_TRUE(store.find("/default int8").valid());
	EXPECT_TRUE(store.find("/f read/write").valid());
	EXPECT_TRUE(store.find("/f read-only").valid());
	EXPECT_TRUE(store.find("/array bool[1]").valid());
	EXPECT_TRUE(store.find("/scope/inner bool").valid());
}

TEST(Directory, ShortMatch) {
	stored::TestStore store;
	EXPECT_TRUE(store.find("/d.......f").valid());
	EXPECT_TRUE(store.find("/f.r.../").valid());
	EXPECT_TRUE(store.find("/f.r...-").valid());
	EXPECT_TRUE(store.find("/init float 3").valid());
	EXPECT_TRUE(store.find("/sc/i.....b").valid());
	EXPECT_TRUE(store.find("/so/s").valid());
}

TEST(Directory, Ambiguous) {
	stored::TestStore store;
	EXPECT_FALSE(store.find("/default int").valid());
	EXPECT_FALSE(store.find("/s/inner bool").valid());
}

TEST(Directory, Bogus) {
	stored::TestStore store;
	EXPECT_FALSE(store.find("").valid());
	EXPECT_FALSE(store.find("/").valid());
	EXPECT_FALSE(store.find("asdf").valid());
	EXPECT_FALSE(store.find("/zzz").valid());
}

static int objects;
static void list_cb_templ(stored::TestStore*, char const*, stored::Type::type, void*, size_t) { objects++; }
static void list_cb_arg(void*, char const*, stored::Type::type, void*, size_t, void*) { objects++; }

TEST(Directory, ListFunctions) {
	stored::TestStore store;

	// list() using an rvalue lambda (>= C++11)
	objects = 0;
	store.list([&](stored::TestStore*, char const*, stored::Type::type, void*, size_t) { objects++; });
	EXPECT_GT(objects, 1);

	// list() using an lvalue lambda (>= C++11)
	objects = 0;
	auto l = [&](stored::TestStore*, char const*, stored::Type::type, void*, size_t) { objects++; };
	store.list(l);
	EXPECT_GT(objects, 1);

	// list() using an std::function (>= C++11)
	objects = 0;
	std::function<void(stored::TestStore*, char const*, stored::Type::type, void*, size_t)> f =
		[&](stored::TestStore*, char const*, stored::Type::type, void*, size_t) { objects++; };
	store.list(f);
	EXPECT_GT(objects, 1);

	// list() using a static function (>= C++11)
	objects = 0;
	store.list(&list_cb_templ);
	EXPECT_GT(objects, 1);

	// list() using a static function with argument (< C++11)
	objects = 0;
	store.list(&list_cb_arg, nullptr);
	EXPECT_GT(objects, 1);
}

TEST(Directory, List) {
	stored::TestStore store;

	std::list<std::string> names;
	store.list([&](stored::TestStore*, char const* name, stored::Type::type, void*, size_t) { names.push_back(name); });

	// We should find something.
	EXPECT_GT(names.size(), 10);

	// Check a few names.
	EXPECT_TRUE(std::find(names.begin(), names.end(), "/default int8") != names.end());
	EXPECT_TRUE(std::find(names.begin(), names.end(), "/f read/write") != names.end());
	EXPECT_TRUE(std::find(names.begin(), names.end(), "/f read-only") != names.end());
	EXPECT_TRUE(std::find(names.begin(), names.end(), "/array bool[0]") != names.end());
	EXPECT_TRUE(std::find(names.begin(), names.end(), "/scope/inner int") != names.end());
	EXPECT_TRUE(std::find(names.begin(), names.end(), "/non existent object") == names.end());

	// Check all collected names.
	for(auto const& n : names)
		EXPECT_TRUE(std::find(names.begin(), names.end(), n.c_str()) != names.end());
}

} // namespace

