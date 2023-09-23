// SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
//
// SPDX-License-Identifier: MPL-2.0

#include "libstored/allocator.h"

std::function<void(std::type_info const*, void*, size_t, size_t)> TestAllocatorBase::allocate_cb;
std::function<void(std::type_info const*, void*, size_t, size_t)> TestAllocatorBase::deallocate_cb;
TestAllocatorBase::Stats TestAllocatorBase::allocate_stats;
TestAllocatorBase::Stats TestAllocatorBase::deallocate_stats;
