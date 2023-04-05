/*
 * libstored, distributed debuggable data stores.
 * Copyright (C) 2020-2023  Jochem Rutgers
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
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
	constexpr auto amp_o =
		stored::Amplifier<stored::TestStore>::objects<'g', 'O'>("/ambiguous amp/");
	stored::Amplifier<stored::TestStore, amp_o.flags()> amp{amp_o, store};

	EXPECT_FLOAT_EQ(amp(1.0f), -1.0f);
	EXPECT_FLOAT_EQ(store.ambiguous_amp__output.get(), -1.0f);
}

TEST(Amplifier, Double)
{
	stored::TestStore store;
	constexpr auto amp_o =
		stored::Amplifier<stored::TestStore, 0, double>::objects<'g'>("/double amp/");
	stored::Amplifier<stored::TestStore, amp_o.flags(), double> amp{amp_o, store};

	EXPECT_DOUBLE_EQ(amp(1.0), -3.0);
}

TEST(Amplifier, Assign)
{
	stored::TestStore store;
	constexpr auto amp1_o = stored::Amplifier<stored::TestStore>::objects<'g', 'O'>("/amp/");
	constexpr auto amp2_o =
		stored::Amplifier<stored::TestStore>::objects<'g', 'O'>("/small amp/");
	static_assert(amp1_o.flags() == amp2_o.flags(), "");

	stored::Amplifier<stored::TestStore, amp1_o.flags()> amp;

	amp = stored::Amplifier<stored::TestStore, amp1_o.flags()>{amp1_o, store};
	EXPECT_FLOAT_EQ(amp(1.0f), 2.0f); // no offset

	amp = stored::Amplifier<stored::TestStore, amp2_o.flags()>{amp2_o, store};
	EXPECT_FLOAT_EQ(amp(1.0f), 3.5f);
}

} // namespace
