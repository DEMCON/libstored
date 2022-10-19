/*
 * libstored, distributed debuggable data stores.
 * Copyright (C) 2020-2022  Jochem Rutgers
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "libstored/allocator.h"

std::function<void(std::type_info const*, void*, size_t, size_t)> TestAllocatorBase::allocate_cb;
std::function<void(std::type_info const*, void*, size_t, size_t)> TestAllocatorBase::deallocate_cb;
TestAllocatorBase::Stats TestAllocatorBase::allocate_stats;
TestAllocatorBase::Stats TestAllocatorBase::deallocate_stats;
