// SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
//
// SPDX-License-Identifier: MPL-2.0

#include "libstored/poller.h"
#include "TestStore.h"
#include "gtest/gtest.h"

#include "LoggingLayer.h"

#include <poll.h>
#include <unistd.h>

#ifdef STORED_HAVE_ZMQ
#  include <zmq.h>
#endif

namespace {
TEST(Poller, Banner)
{
	puts(stored::banner());
}

TEST(Poller, PollableCallback)
{
	int count = 0;
	auto p1 = stored::pollable(
		[&](stored::Pollable const& p) {
			count++;
			return p.events;
		},
		stored::Pollable::PollIn);

	stored::CustomPoller<stored::LoopPoller> poller = {p1};

	auto res = poller.poll(0);
	EXPECT_EQ(res.size(), 1U);
}

#if defined(STORED_HAVE_ZMQ)
TEST(Poller, PollableZmqSocket)
{
	void* context = zmq_ctx_new();
	ASSERT_NE(context, nullptr);
	void* rep = zmq_socket(context, ZMQ_REP);
	ASSERT_EQ(zmq_bind(rep, "inproc://poller"), 0);
	void* req = zmq_socket(context, ZMQ_REQ);
	ASSERT_EQ(zmq_connect(req, "inproc://poller"), 0);

	stored::Poller poller;
	stored::PollableZmqSocket preq(rep, stored::Pollable::PollIn);
	EXPECT_EQ(poller.add(preq), 0);

	auto const* res = &poller.poll(0);
	EXPECT_EQ(res->size(), 0);
	EXPECT_EQ(errno, EAGAIN);

	zmq_send(req, "Hi", 2, 0);

	res = &poller.poll(0);
	ASSERT_EQ(res->size(), 1);
	EXPECT_EQ(res->at(0)->revents, stored::Pollable::PollIn + 0);

	zmq_close(req);
	zmq_close(rep);
	zmq_ctx_destroy(context);
}
#endif

} // namespace
