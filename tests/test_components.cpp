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

#include <stored>

namespace {

TEST(Amplifier, Full)
{
	stored::TestStore store;
	constexpr auto amp_o = stored::Amplifier<stored::TestStore>::objects("/amp/");
	stored::Amplifier<stored::TestStore, amp_o.flags()> amp{amp_o, store};

	EXPECT_FLOAT_EQ(amp(1.0f), 2.5f);
	EXPECT_FLOAT_EQ(store.amp__input.get(), 1.0f);
	EXPECT_FLOAT_EQ(store.amp__output.get(), 2.5f);

	EXPECT_FLOAT_EQ(amp(100.0f), 10.0f);
	EXPECT_FLOAT_EQ(store.amp__input.get(), 100.0f);
	EXPECT_FLOAT_EQ(store.amp__output.get(), 10.0f);

	EXPECT_FLOAT_EQ(amp(-100.0f), -1.0f);
	EXPECT_FLOAT_EQ(store.amp__input.get(), -100.0f);
	EXPECT_FLOAT_EQ(store.amp__output.get(), -1.0f);

	store.amp__override = 2;
	EXPECT_FLOAT_EQ(amp(0), 2.0f);
	EXPECT_FLOAT_EQ(store.amp__output.get(), 2.0f);
}

TEST(Amplifier, Small)
{
	stored::TestStore store;
	constexpr auto amp_o = stored::Amplifier<stored::TestStore>::objects("/small amp/");
	stored::Amplifier<stored::TestStore, amp_o.flags()> amp{amp_o, store};

	EXPECT_FLOAT_EQ(amp(1.0f), 3.5f);
	EXPECT_FLOAT_EQ(store.small_amp__output.get(), 3.5f);

	EXPECT_FLOAT_EQ(amp(100.0f), 350.0f);
	EXPECT_FLOAT_EQ(store.small_amp__output.get(), 350.0f);

	EXPECT_FLOAT_EQ(amp(-100.0f), -350.0f);
	EXPECT_FLOAT_EQ(store.small_amp__output.get(), -350.0f);

	constexpr auto big_amp_o = stored::Amplifier<stored::TestStore>::objects("/amp/");
	stored::Amplifier<stored::TestStore, big_amp_o.flags()> big_amp{big_amp_o, store};
	EXPECT_LE(sizeof(amp), sizeof(big_amp));
}

TEST(Amplifier, Ambiguous)
{
	stored::TestStore store;
	// Note that enable is not passed to objects(), so it is not used by amp.
	constexpr auto amp_o = stored::Amplifier<stored::TestStore>::objects<'g','O'>("/ambiguous amp/");
	stored::Amplifier<stored::TestStore, amp_o.flags()> amp{amp_o, store};

	EXPECT_FLOAT_EQ(amp(1.0f), -1.0f);
	EXPECT_FLOAT_EQ(store.ambiguous_amp__output.get(), -1.0f);
}

TEST(Amplifier, Double)
{
	stored::TestStore store;
	constexpr auto amp_o = stored::Amplifier<stored::TestStore, 0, double>::objects<'g'>("/double amp/");
	stored::Amplifier<stored::TestStore, amp_o.flags(), double> amp{amp_o, store};

	EXPECT_DOUBLE_EQ(amp(1.0), -3.0);
}

TEST(Amplifier, Assign)
{
	stored::TestStore store;
	constexpr auto amp1_o = stored::Amplifier<stored::TestStore>::objects<'g','O'>("/amp/");
	constexpr auto amp2_o = stored::Amplifier<stored::TestStore>::objects<'g','O'>("/small amp/");
	static_assert(amp1_o.flags() == amp2_o.flags(), "");

	stored::Amplifier<stored::TestStore, amp1_o.flags()> amp;

	amp = stored::Amplifier<stored::TestStore, amp1_o.flags()>{amp1_o, store};
	EXPECT_FLOAT_EQ(amp(1.0f), 2.0f); // no offset

	amp = stored::Amplifier<stored::TestStore, amp2_o.flags()>{amp2_o, store};
	EXPECT_FLOAT_EQ(amp(1.0f), 3.5f);
}

} // namespace
