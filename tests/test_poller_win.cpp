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

#include "TestStore.h"
#include "libstored/poller.h"
#include "gtest/gtest.h"

#ifdef STORED_HAVE_ZMQ
#  include <zmq.h>
#endif

namespace {

TEST(Poller, Win) {
	EXPECT_TRUE(true);

	HANDLE e = CreateEvent(NULL, TRUE, FALSE, NULL);
	ASSERT_NE(e, (HANDLE)NULL);

	stored::Poller poller;

	// Test HANDLE
	poller.addh(e, (void*)1, stored::Poller::PollIn);

	auto res = poller.poll(0);
	EXPECT_EQ(res, nullptr);

	EXPECT_EQ(SetEvent(e), TRUE);
	res = poller.poll(0);
	ASSERT_NE(res, nullptr);
	ASSERT_EQ(res->size(), 1);
	EXPECT_EQ(res->at(0).handle, e);
	EXPECT_EQ(res->at(0).user_data, (void*)1);

	// Test fd (non-socket)
	EXPECT_EQ(poller.add(fileno(stdin), (void*)2, stored::Poller::PollIn), 0);
	EXPECT_EQ(poller.add(fileno(stdout), (void*)2, stored::Poller::PollOut), 0);
	res = poller.poll(0);
	ASSERT_NE(res, nullptr);
	EXPECT_GE(res->size(), 2); // stdin may or may not be readable. stdout is always writable.

	EXPECT_EQ(poller.remove(fileno(stdin)), 0);
	EXPECT_EQ(poller.remove(fileno(stdout)), 0);

	EXPECT_EQ(ResetEvent(e), TRUE);
	res = poller.poll(0);
	EXPECT_EQ(res, nullptr);

	CloseHandle(e);
}

#if defined(STORED_HAVE_ZMQ)
TEST(Poller, Zmq) {
	void* context = zmq_ctx_new();
	ASSERT_NE(context, nullptr);
	void* rep = zmq_socket(context, ZMQ_REP);
	ASSERT_EQ(zmq_bind(rep, "inproc://poller"), 0);
	void* req = zmq_socket(context, ZMQ_REQ);
	ASSERT_EQ(zmq_connect(req, "inproc://poller"), 0);

	stored::Poller poller;
	EXPECT_EQ(poller.add(rep, (void*)1, stored::Poller::PollOut | stored::Poller::PollIn), 0);
	EXPECT_EQ(poller.add(req, (void*)2, stored::Poller::PollOut | stored::Poller::PollIn), 0);

	auto res = poller.poll(0);
	ASSERT_NE(res, nullptr);
	ASSERT_EQ(res->size(), 1);
	EXPECT_EQ(res->at(0).user_data, (void*)2);
	EXPECT_EQ(res->at(0).events, stored::Poller::PollOut);

	zmq_send(req, "Hi", 2, 0);

	res = poller.poll(0);
	ASSERT_NE(res, nullptr);
	ASSERT_EQ(res->size(), 1);
	EXPECT_EQ(res->at(0).user_data, (void*)1);

	char buffer[16];
	zmq_recv(rep, buffer, sizeof(buffer), 0);
	zmq_send(rep, buffer, 2, 0);

	res = poller.poll(0);
	ASSERT_NE(res, nullptr);
	EXPECT_EQ(res->size(), 1);

	zmq_close(req);
	zmq_close(rep);
	zmq_ctx_destroy(context);
}
#endif // STORED_HAVE_ZMQ

} // namespace

