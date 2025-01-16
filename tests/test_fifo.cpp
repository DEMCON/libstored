// SPDX-FileCopyrightText: 2020-2025 Jochem Rutgers
//
// SPDX-License-Identifier: MPL-2.0

#include "libstored/fifo.h"

#include "gtest/gtest.h"

#include <thread>
#include <vector>

namespace {

#define EXPECT_EQ_VIEW(f)                           \
  do {                                              \
    using type = std::decay_t<decltype((f))>::type; \
                                                    \
    auto view = (f).view();                         \
    EXPECT_EQ((f).available(), view.size());        \
    std::vector<type> v;                            \
    v.resize(view.size());                          \
    view.copy(v.data());                            \
                                                    \
    std::vector<type> spm;                          \
    spm.resize(view.size());                        \
    auto const* p = view.contiguous(spm.data());    \
                                                    \
    size_t i = 0;                                   \
    for(auto x : view) {                            \
      EXPECT_EQ(x, (f).peek(i));                    \
      EXPECT_EQ(x, v[i]);                           \
      EXPECT_EQ(x, p[i]);                           \
      i++;                                          \
    }                                               \
    EXPECT_EQ(i, (f).available());                  \
  } while(0)

TEST(Fifo, UnboundedFifo)
{
	stored::Fifo<int> f;

	EXPECT_FALSE(f.bounded());
	EXPECT_TRUE(f.empty());
	EXPECT_FALSE(f.full());
	EXPECT_EQ(f.size(), 0u);
	EXPECT_EQ_VIEW(f);

	f.push_back(1);
	EXPECT_FALSE(f.empty());
	EXPECT_FALSE(f.full());
	EXPECT_EQ(f.size(), 1u);
	EXPECT_EQ(f.front(), 1u);
	EXPECT_EQ_VIEW(f);

	f.push_back(2);
	EXPECT_EQ(f.size(), 2u);
	EXPECT_EQ(f.front(), 1u);
	EXPECT_EQ_VIEW(f);

	f.push_back(3);
	f.push_back(4);
	EXPECT_EQ(f.size(), 4u);
	EXPECT_EQ_VIEW(f);

	f.pop_front();
	EXPECT_EQ(f.front(), 2u);
	EXPECT_EQ_VIEW(f);

	f.push_back(5);
	// Not empty, should still be growing.
	EXPECT_EQ(f.size(), 5u);
	EXPECT_EQ_VIEW(f);

	f.pop_front();
	f.pop_front();
	f.pop_front();
	f.pop_front();
	EXPECT_TRUE(f.empty());
	EXPECT_EQ_VIEW(f);

	f.push_back(6);
	f.push_back(7);
	f.push_back(8);
	// Restarted at beginning of buffer.
	EXPECT_EQ(f.size(), 5u);
	EXPECT_EQ(f.front(), 6u);
	EXPECT_EQ_VIEW(f);
}

TEST(Fifo, BoundedFifo)
{
	stored::Fifo<int, 4> f;

	EXPECT_TRUE(f.bounded());
	EXPECT_TRUE(f.empty());
	EXPECT_FALSE(f.full());
	EXPECT_LE(f.size(), 5u);
	EXPECT_EQ_VIEW(f);

	f.push_back(1);
	EXPECT_FALSE(f.empty());
	EXPECT_FALSE(f.full());
	EXPECT_LE(f.size(), 5u);
	EXPECT_EQ(f.front(), 1u);
	EXPECT_EQ_VIEW(f);

	f.push_back(2);
	EXPECT_LE(f.size(), 5u);
	EXPECT_EQ(f.front(), 1u);
	EXPECT_EQ_VIEW(f);

	f.push_back(3);
	f.push_back(4);
	EXPECT_TRUE(f.full());
	EXPECT_LE(f.size(), 5u);
	EXPECT_EQ_VIEW(f);

	f.pop_front();
	EXPECT_FALSE(f.full());
	EXPECT_EQ(f.front(), 2u);
	EXPECT_EQ_VIEW(f);

	f.push_back(5);
	EXPECT_TRUE(f.full());
	EXPECT_LE(f.size(), 5u);
	EXPECT_EQ_VIEW(f);

	f.pop_front();
	EXPECT_EQ(f.front(), 3u);
	EXPECT_EQ_VIEW(f);
	f.pop_front();
	EXPECT_EQ(f.front(), 4u);
	EXPECT_EQ_VIEW(f);
	f.pop_front();
	EXPECT_EQ(f.front(), 5u);
	EXPECT_EQ_VIEW(f);
	f.pop_front();
	EXPECT_TRUE(f.empty());
	EXPECT_EQ_VIEW(f);
}

TEST(Fifo, IterateFifo)
{
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

#define EXPECT_EQ_MSG(msg, str)           \
  do {                                    \
    auto m_ = (msg);                      \
    EXPECT_NE(m_.data(), nullptr);        \
    std::string s_(m_.data(), m_.size()); \
    EXPECT_EQ(s_, "" str);                \
  } while(0)

TEST(Fifo, UnboundedMessageFifo)
{
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
	EXPECT_TRUE(f.push_back(stored::MessageView{"hi", 2}));
	EXPECT_EQ_MSG(f.front(), "hi");
	EXPECT_FALSE(f.empty());
	EXPECT_EQ(f.size(), 7u);

	EXPECT_TRUE(f.append_back("jk", 2));
	EXPECT_EQ_MSG(f.front(), "hi");
	f.pop_front();
	EXPECT_TRUE(f.empty());
	EXPECT_TRUE(f.append_back("lmn", 3));
	EXPECT_TRUE(f.push_back());
	EXPECT_FALSE(f.empty());
	EXPECT_EQ_MSG(f.front(), "jklmn");
	f.pop_front();
	EXPECT_TRUE(f.empty());
}

TEST(Fifo, BoundedMessageFifo)
{
	stored::MessageFifo<16, 4> f;

	EXPECT_TRUE(f.bounded());
	EXPECT_EQ(f.space(), 15u);
	EXPECT_FALSE(f.full());
	EXPECT_EQ(f.push_back({{"abc", 3}, {"defg", 4}, {"ghijk", 5}, {"lmn", 3}}), 4u);
	EXPECT_EQ(f.space(), 0u);
	// Does not fit
	EXPECT_FALSE(f.push_back({"h", 1}));

	EXPECT_EQ_MSG(f.front(), "abc");
	f.pop_front();
	EXPECT_EQ(f.space(), 2u);

	// Too long message.
	EXPECT_FALSE(f.push_back({"hijl", 4}));

	// Fits.
	EXPECT_TRUE(f.push_back({"h", 1}));
	EXPECT_EQ(f.space(), 0u); // 0 as the fifo is full
	EXPECT_TRUE(f.full());

	// Buffer fits, message queue not.
	EXPECT_FALSE(f.push_back({"i", 1}));
	EXPECT_TRUE(f.append_back({"i", 1}));
	EXPECT_EQ(f.space(), 0u); // 0 as fhte fifo is full
	EXPECT_TRUE(f.full());
	EXPECT_FALSE(f.push_back());

	// Buffer is full.
	EXPECT_FALSE(f.append_back({"jk", 2}));
	f.pop_back();

	// Not anymore.
	EXPECT_TRUE(f.append_back({"jk", 2}));
	EXPECT_EQ(f.space(), 0u);

	f.clear();
	EXPECT_TRUE(f.empty());
	EXPECT_EQ(f.push_back({{"0123", 4}, {"456789abcd", 10}}), 2u);
	EXPECT_EQ_MSG(f.front(), "0123");

	// No room in buffer.
	EXPECT_FALSE(f.push_back({"ab", 2}));
	EXPECT_EQ(f.space(), 1u);

	f.pop_front();
	// Put at start of buffer, leaving the last byte unused.
	EXPECT_EQ(f.space(), 3u);
	EXPECT_TRUE(f.push_back({"abc", 3}));
	EXPECT_EQ_MSG(f.front(), "456789abcd");

	// There is one unusable byte at the end, so this won't fit.
	EXPECT_FALSE(f.push_back({"e", 1}));
	EXPECT_EQ(f.space(), 0u);
}

TEST(Fifo, IterateMessageFifo)
{
	stored::MessageFifo<16, 4> f;

	EXPECT_EQ(f.push_back({{"0", 1}, {"1", 1}, {"2", 1}, {"3", 1}}), 4u);
	int i = 0;
	for(auto x : f)
		EXPECT_EQ(x.data()[0] - '0', i++);

	EXPECT_TRUE(f.empty());
}

#ifndef STORED_COMPILER_MINGW
// MinGW does not implement std::thread.

TEST(Fifo, ProducerConsumer)
{
	stored::MessageFifo<16, 4> f;
	char const msg[16] = "abcdefghijklmno";

	long pcs = 0;
	std::thread p([&]() {
		for(int l = 0; l < 1000; l++) {
			size_t len = (size_t)(rand() % 15) + 1;
			for(size_t i = 0; i < len; i++)
				pcs += (long)(msg[i] - 'a');

			while(!f.push_back(msg, len))
				std::this_thread::yield();
		}
	});

	long ccs = 0;
	std::thread c([&]() {
		for(int l = 0; l < 1000; l++) {
			while(f.empty())
				std::this_thread::yield();

			auto m = f.front();
			for(size_t i = 0; i < m.size(); i++)
				ccs += (long)(m.data()[i] - 'a');

			f.pop_front();
		}
	});

	c.join();
	p.join();

	EXPECT_EQ(pcs, ccs);
}
#endif // STORED_COMPILER_MINGW

} // namespace
