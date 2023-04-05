/*
 * libstored, distributed debuggable data stores.
 * Copyright (C) 2020-2023  Jochem Rutgers
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "libstored/poller.h"
#include "gtest/gtest.h"

static bool poll_once_called;

namespace stored {

int poll_once(TypedPollable const& p, Pollable::Events& revents) noexcept
{
	poll_once_called = true;
	return poll_once_default(p, revents);
}

} // namespace stored

namespace {

TEST(Weak, Override)
{
	bool flag = false;

	auto p = stored::pollable(
		[&](stored::Pollable const& pollable) {
			flag = true;
			return pollable.events;
		},
		stored::Pollable::PollIn);

	stored::CustomPoller<stored::LoopPoller> poller = {p};

	poll_once_called = false;
	auto res = poller.poll();

	EXPECT_EQ(res.size(), 1U);
	EXPECT_TRUE(flag);
	EXPECT_TRUE(poll_once_called);
}

} // namespace
