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

#include <unistd.h>
#include <poll.h>

#ifdef STORED_HAVE_ZMQ
#  include <zmq.h>
#endif

namespace {

TEST(Poller, Pipe) {
	int fd[2];
	ASSERT_EQ(pipe(fd), 0);

	char buf = '0';

	// Check if the pipe works at all.
	write(fd[1], "1", 1);
	read(fd[0], &buf, 1);
	EXPECT_EQ(buf, '1');

	stored::Poller poller;
	EXPECT_EQ(poller.add(fd[0], (void*)1, stored::Poller::PollIn), 0);

	auto res = poller.poll(0);
	EXPECT_EQ(res, nullptr);

	// Put something in the pipe.
	write(fd[1], "2", 1);
	res = poller.poll(0);
	ASSERT_NE(res, nullptr);
	ASSERT_EQ(res->size(), 1);
	EXPECT_EQ(res->at(0).fd, fd[0]);
	EXPECT_EQ(res->at(0).events, stored::Poller::PollIn);
	EXPECT_EQ(res->at(0).user_data, (void*)1);

	// Drain pipe.
	read(fd[0], &buf, 1);
	EXPECT_EQ(buf, '2');
	res = poller.poll(0);
	EXPECT_EQ(res, nullptr);

	// Add a second fd.
	EXPECT_EQ(poller.add(fd[1], (void*)2, stored::Poller::PollOut), 0);
	res = poller.poll(0);
	ASSERT_NE(res, nullptr);
	ASSERT_EQ(res->size(), 1);
	EXPECT_EQ(res->at(0).fd, fd[1]);
	EXPECT_EQ(res->at(0).events, stored::Poller::PollOut);
	EXPECT_EQ(res->at(0).user_data, (void*)2);

	write(fd[1], "3", 1);
	res = poller.poll(0);
	ASSERT_NE(res, nullptr);
	ASSERT_EQ(res->size(), 2);
	// The order is undefined.
	if(res->at(0).fd == fd[0]) {
		EXPECT_EQ(res->at(1).fd, fd[1]);
		EXPECT_EQ(res->at(1).events, stored::Poller::PollOut);
		EXPECT_EQ(res->at(1).user_data, (void*)2);
		EXPECT_EQ(res->at(0).fd, fd[0]);
		EXPECT_EQ(res->at(0).events, stored::Poller::PollIn);
		EXPECT_EQ(res->at(0).user_data, (void*)1);
	} else {
		EXPECT_EQ(res->at(0).fd, fd[1]);
		EXPECT_EQ(res->at(0).events, stored::Poller::PollOut);
		EXPECT_EQ(res->at(0).user_data, (void*)2);
		EXPECT_EQ(res->at(1).fd, fd[0]);
		EXPECT_EQ(res->at(1).events, stored::Poller::PollIn);
		EXPECT_EQ(res->at(1).user_data, (void*)1);
	}

	// Drain pipe again.
	read(fd[0], &buf, 1);
	EXPECT_EQ(buf, '3');
	res = poller.poll(0);
	ASSERT_NE(res, nullptr);
	ASSERT_EQ(res->size(), 1);
	EXPECT_EQ(res->at(0).fd, fd[1]);
	EXPECT_EQ(res->at(0).events, stored::Poller::PollOut);
	EXPECT_EQ(res->at(0).user_data, (void*)2);

	// Only check for errors now.
	EXPECT_EQ(poller.modify(fd[1], stored::Poller::PollErr), 0);
	res = poller.poll(0);
	EXPECT_EQ(res, nullptr); // not writable anymore

	// Close read end.
	EXPECT_EQ(poller.remove(fd[0]), 0);
	close(fd[0]);
	res = poller.poll(0);
	ASSERT_NE(res, nullptr);
	ASSERT_EQ(res->size(), 1);
	EXPECT_EQ(res->at(0).fd, fd[1]);
	EXPECT_EQ(res->at(0).events, stored::Poller::PollErr);
	EXPECT_EQ(res->at(0).user_data, (void*)2);
}

#if defined(STORED_HAVE_ZMQ) && !defined(STORED_POLL_POLL) && !defined(STORED_POLL_LOOP) && !defined(STORED_POLL_ZTH_LOOP)
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

#if defined(STORED_POLL_LOOP) || defined(STORED_POLL_ZTH_LOOP)
namespace stored {
	int poll_once(Poller::Event const& e, short& revents) {
		switch(e.type) {
		case Poller::Event::TypeFd: {
			struct pollfd fd = {};
			fd.fd = e.fd;
			if(e.events & Poller::PollIn)
				fd.events |= POLLIN;
			if(e.events & Poller::PollOut)
				fd.events |= POLLOUT;
			if(e.events & Poller::PollErr)
				fd.events |= POLLERR;

do_poll:
			switch(poll(&fd, 1, 0)) {
			case 0:
				// Timeout
				return 0;
			case 1:
				// Got it.
				revents = 0;
				if(fd.revents & POLLIN)
					revents |= (short)Poller::PollIn;
				if(fd.revents & POLLOUT)
					revents |= (short)Poller::PollOut;
				if(fd.revents & POLLERR)
					revents |= (short)Poller::PollErr;
				return 0;
			case -1:
				switch(errno) {
				case EINTR:
				case EAGAIN:
					goto do_poll;
				default:
					return errno;
				}
			default:
				return EINVAL;
			}
		}
		default:
			return EINVAL;
		}
	}
} // namespace
#endif
