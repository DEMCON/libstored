// SPDX-FileCopyrightText: 2020-2025 Jochem Rutgers
//
// SPDX-License-Identifier: MPL-2.0

#include "libstored/poller.h"
#include "TestStore.h"
#include "gtest/gtest.h"

#ifdef STORED_HAVE_ZMQ
#  include <zmq.h>
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
	stored::PollableHandle h(e, stored::Pollable::PollIn, (void*)1);
	EXPECT_EQ(poller.add(h), 0);

	auto const* res = &poller.poll(0);
	EXPECT_NE(errno, 0);
	EXPECT_TRUE(res->empty());

	EXPECT_EQ(SetEvent(e), TRUE);
	res = &poller.poll(0);
	ASSERT_EQ(res->size(), 1);
	EXPECT_EQ(res->at(0)->user_data, (void*)1);

	// Test fd (non-socket)
	stored::PollableFd pstdin(fileno(stdin), stored::Pollable::PollIn, (void*)2);
	stored::PollableFd pstdout(fileno(stdout), stored::Pollable::PollOut, (void*)2);
	EXPECT_EQ(poller.add(pstdin), 0);
	EXPECT_EQ(poller.add(pstdout), 0);
	res = &poller.poll(0);
	EXPECT_GE(res->size(), 2); // stdin may or may not be readable. stdout is always writable.

	EXPECT_EQ(poller.remove(pstdin), 0);
	EXPECT_EQ(poller.remove(pstdout), 0);

	EXPECT_EQ(ResetEvent(e), TRUE);
	res = &poller.poll(0);
	EXPECT_NE(errno, 0);
	EXPECT_TRUE(res->empty());

	EXPECT_EQ(poller.remove(h), 0);

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
	stored::PollableZmqSocket prep(
		rep, stored::Pollable::PollOut | stored::Pollable::PollIn, (void*)1);
	stored::PollableZmqSocket preq(
		req, stored::Pollable::PollOut | stored::Pollable::PollIn, (void*)2);
	EXPECT_EQ(poller.add(prep), 0);
	EXPECT_EQ(poller.add(preq), 0);

	auto const* res = &poller.poll(0);
	ASSERT_EQ(res->size(), 1);
	EXPECT_EQ(res->at(0)->user_data, (void*)2);
	EXPECT_EQ(res->at(0)->revents, stored::Pollable::PollOut + 0);

	zmq_send(req, "Hi", 2, 0);

	res = &poller.poll(0);
	ASSERT_EQ(res->size(), 1);
	EXPECT_EQ(res->at(0)->user_data, (void*)1);

	char buffer[16];
	zmq_recv(rep, buffer, sizeof(buffer), 0);
	zmq_send(rep, buffer, 2, 0);

	res = &poller.poll(0);
	EXPECT_EQ(res->size(), 1);

	EXPECT_EQ(poller.remove(prep), 0);
	EXPECT_EQ(poller.remove(preq), 0);

	zmq_close(req);
	zmq_close(rep);
	zmq_ctx_destroy(context);
}
#endif // STORED_HAVE_ZMQ

} // namespace
