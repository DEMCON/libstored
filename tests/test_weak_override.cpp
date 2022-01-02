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