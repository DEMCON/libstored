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

#include "libstored/fifo.h"

#include "gtest/gtest.h"

#include <thread>

namespace {

TEST(Fifo, UnboundedFifo) {
	stored::Fifo<int> f;

	EXPECT_FALSE(f.bounded());
	EXPECT_TRUE(f.empty());
	EXPECT_FALSE(f.full());
	EXPECT_EQ(f.size(), 0u);

	f.push_back(1);
	EXPECT_FALSE(f.empty());
	EXPECT_FALSE(f.full());
	EXPECT_EQ(f.size(), 1u);
	EXPECT_EQ(f.front(), 1u);

	f.push_back(2);
	EXPECT_EQ(f.size(), 2u);
	EXPECT_EQ(f.front(), 1u);

	f.push_back(3);
	f.push_back(4);
	EXPECT_EQ(f.size(), 4u);

	f.pop_front();
	EXPECT_EQ(f.front(), 2u);

	f.push_back(5);
	// Not empty, should still be growing.
	EXPECT_EQ(f.size(), 5u);

	f.pop_front();
	f.pop_front();
	f.pop_front();
	f.pop_front();
	EXPECT_TRUE(f.empty());

	f.push_back(6);
	f.push_back(7);
	f.push_back(8);
	// Restarted at beginning of buffer.
	EXPECT_EQ(f.size(), 5u);
	EXPECT_EQ(f.front(), 6u);
}

TEST(Fifo, BoundedFifo) {
	stored::Fifo<int, 4> f;

	EXPECT_TRUE(f.bounded());
	EXPECT_TRUE(f.empty());
	EXPECT_FALSE(f.full());
	EXPECT_LE(f.size(), 5u);

	f.push_back(1);
	EXPECT_FALSE(f.empty());
	EXPECT_FALSE(f.full());
	EXPECT_LE(f.size(), 5u);
	EXPECT_EQ(f.front(), 1u);

	f.push_back(2);
	EXPECT_LE(f.size(), 5u);
	EXPECT_EQ(f.front(), 1u);

	f.push_back(3);
	f.push_back(4);
	EXPECT_TRUE(f.full());
	EXPECT_LE(f.size(), 5u);

	f.pop_front();
	EXPECT_FALSE(f.full());
	EXPECT_EQ(f.front(), 2u);

	f.push_back(5);
	EXPECT_TRUE(f.full());
	EXPECT_LE(f.size(), 5u);

	f.pop_front();
	EXPECT_EQ(f.front(), 3u);
	f.pop_front();
	EXPECT_EQ(f.front(), 4u);
	f.pop_front();
	EXPECT_EQ(f.front(), 5u);
	f.pop_front();
	EXPECT_TRUE(f.empty());
}

TEST(Fifo, IterateFifo) {
	stored::Fifo<int, 4> f;

	f.push_back({1, 2, 3});
	int i = 1;
	for(auto x : f)
		EXPECT_EQ(x, i++);

	EXPECT_TRUE(f.empty());

	f.push_back({10, 11});
	i = 10;
	for(auto x : f) {
		EXPECT_EQ(x, i++);
		f.push_back(i);
	}

	// Should stop at the content when the iterator was created.
	EXPECT_EQ(f.front(), 11u);
	f.pop_front();
	EXPECT_EQ(f.front(), 12u);
	f.pop_front();
	EXPECT_TRUE(f.empty());
}

#define EXPECT_EQ_MSG(msg, str) \
	({ \
		auto m_ = (msg); \
		EXPECT_NE(m_.message, nullptr); \
		std::string s_(m_.message, m_.length);\
		EXPECT_EQ(s_, "" str); \
	})

TEST(Fifo, UnboundedMessageFifo) {
	stored::MessageFifo<> f;

	EXPECT_FALSE(f.bounded());
	EXPECT_TRUE(f.empty());
	EXPECT_TRUE(f.push_back("abc", 3));
	EXPECT_FALSE(f.empty());
	EXPECT_EQ(f.available(), 1u);
	EXPECT_EQ(f.size(), 3u);

	EXPECT_EQ_MSG(f.front(), "abc");

	EXPECT_TRUE(f.push_back("defg", 4));
	EXPECT_EQ(f.available(), 2u);
	EXPECT_EQ(f.size(), 7u);
	EXPECT_EQ_MSG(f.front(), "abc");
	f.pop_front();
	EXPECT_EQ_MSG(f.front(), "defg");
	f.pop_front();
	EXPECT_TRUE(f.empty());

	// Fifo is empty, new message should be saved at the start of the buffer again.
	EXPECT_TRUE(f.push_back(stored::Message{"hi", 2}));
	EXPECT_EQ_MSG(f.front(), "hi");
	EXPECT_FALSE(f.empty());
	EXPECT_EQ(f.size(), 7u);
}

TEST(Fifo, BoundedMessageFifo) {
	stored::MessageFifo<16, 4> f;

	EXPECT_TRUE(f.bounded());
	EXPECT_EQ(f.push_back({{"abc", 3}, {"defg", 4}, {"ghijk", 5}, {"lmno", 4}}), 4u);
	// Does not fit
	EXPECT_FALSE(f.push_back({"h", 1}));

	EXPECT_EQ_MSG(f.front(), "abc");
	f.pop_front();

	// Too long message.
	EXPECT_FALSE(f.push_back({"hijl", 4}));

	// Fits.
	EXPECT_TRUE(f.push_back({"h", 1}));

	// Buffer fits, message queue not.
	EXPECT_FALSE(f.push_back({"i", 1}));

	f.clear();
	EXPECT_TRUE(f.empty());
	EXPECT_EQ(f.push_back({{"0123", 4}, {"456789abcde", 11}}), 2u);
	EXPECT_EQ_MSG(f.front(), "0123");

	// No room in buffer.
	EXPECT_FALSE(f.push_back({"ab", 2}));

	f.pop_front();
	// Put at start of buffer, leaving the last byte unused.
	EXPECT_TRUE(f.push_back({"abcd", 4}));
	EXPECT_EQ_MSG(f.front(), "456789abcde");

	// There is one unusable byte at the end, so this won't fit.
	EXPECT_FALSE(f.push_back({"e", 1}));
}

TEST(Fifo, IterateMessageFifo) {
	stored::MessageFifo<16, 4> f;

	EXPECT_EQ(f.push_back({{"0", 1}, {"1", 1}, {"2", 1}, {"3", 1}}), 4u);
	int i = 0;
	for(auto x : f)
		EXPECT_EQ(x.message[0] - '0', i++);

	EXPECT_TRUE(f.empty());
}

TEST(Fifo, ProducerConsumer) {
	stored::MessageFifo<16, 4> f;
	char const msg[17] = "abcdefghijklmnop";

	long pcs = 0;
	std::thread p([&](){
			for(int l = 0; l < 1000; l++) {
				size_t len = (size_t)(rand() % 16) + 1;
				for(size_t i = 0; i < len; i++)
					pcs += (long)(msg[i] - 'a');

				while(!f.push_back(msg, len))
					std::this_thread::yield();
			}
	});

	long ccs = 0;
	std::thread c([&](){
			for(int l = 0; l < 1000; l++) {
				while(f.empty())
					std::this_thread::yield();

				auto m = f.front();
				for(size_t i = 0; i < m.length; i++)
					ccs += (long)(m.message[i] - 'a');

				f.pop_front();
			}
	});

	c.join();
	p.join();

	EXPECT_EQ(pcs, ccs);
}

} // namespace

