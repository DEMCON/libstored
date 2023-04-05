/*
 * libstored, distributed debuggable data stores.
 * Copyright (C) 2020-2023  Jochem Rutgers
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#define STORED_NO_DEPRECATED

#include "libstored/poller.h"
#include "TestStore.h"
#include "gtest/gtest.h"

#ifdef STORED_HAVE_ZMQ
#	include <zmq.h>
#endif

namespace {

TEST(Poller, Win)
{
	puts(stored::banner());

	EXPECT_TRUE(true);

	HANDLE e = CreateEvent(NULL, TRUE, FALSE, NULL);
	ASSERT_NE(e, (HANDLE)NULL);

	stored::Poller poller;

	// Test HANDLE
	EXPECT_EQ(poller.addh(e, (void*)1, stored::Poller::PollIn), 0);

	auto const* res = &poller.poll(0);
	EXPECT_NE(errno, 0);
	EXPECT_TRUE(res->empty());

	EXPECT_EQ(SetEvent(e), TRUE);
	res = &poller.poll(0);
	ASSERT_EQ(res->size(), 1);
	EXPECT_EQ(res->at(0).user_data, (void*)1);

	// Test fd (non-socket)
	EXPECT_EQ(poller.add(fileno(stdin), (void*)2, stored::Poller::PollIn), 0);
	EXPECT_EQ(poller.add(fileno(stdout), (void*)2, stored::Poller::PollOut), 0);
	res = &poller.poll(0);
	EXPECT_GE(res->size(), 2); // stdin may or may not be readable. stdout is always writable.

	EXPECT_EQ(poller.remove(fileno(stdin)), 0);
	EXPECT_EQ(poller.remove(fileno(stdout)), 0);

	EXPECT_EQ(ResetEvent(e), TRUE);
	res = &poller.poll(0);
	EXPECT_NE(errno, 0);
	EXPECT_TRUE(res->empty());

	CloseHandle(e);
}

#if defined(STORED_HAVE_ZMQ)
TEST(Poller, Zmq)
{
	void* context = zmq_ctx_new();
	ASSERT_NE(context, nullptr);
	void* rep = zmq_socket(context, ZMQ_REP);
	ASSERT_EQ(zmq_bind(rep, "inproc://poller"), 0);
	void* req = zmq_socket(context, ZMQ_REQ);
	ASSERT_EQ(zmq_connect(req, "inproc://poller"), 0);

	stored::Poller poller;
	EXPECT_EQ(poller.add(rep, (void*)1, stored::Poller::PollOut | stored::Poller::PollIn), 0);
	EXPECT_EQ(poller.add(req, (void*)2, stored::Poller::PollOut | stored::Poller::PollIn), 0);

	auto const* res = &poller.poll(0);
	ASSERT_EQ(res->size(), 1);
	EXPECT_EQ(res->at(0).user_data, (void*)2);
	EXPECT_EQ(res->at(0).revents, (stored::Poller::events_t)stored::Poller::PollOut);

	zmq_send(req, "Hi", 2, 0);

	res = &poller.poll(0);
	ASSERT_EQ(res->size(), 1);
	EXPECT_EQ(res->at(0).user_data, (void*)1);

	char buffer[16];
	zmq_recv(rep, buffer, sizeof(buffer), 0);
	zmq_send(rep, buffer, 2, 0);

	res = &poller.poll(0);
	EXPECT_EQ(res->size(), 1);

	zmq_close(req);
	zmq_close(rep);
	zmq_ctx_destroy(context);
}
#endif // STORED_HAVE_ZMQ

} // namespace
