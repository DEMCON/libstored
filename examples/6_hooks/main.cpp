// SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
//
// SPDX-License-Identifier: CC0-1.0

/*!
 * \file
 * \brief Example to show the get/set synchronization hooks.
 */

#include <stored>

#include "ExampleHooks.h"

#include <cstdio>

class SyncedExampleHooks : public STORE_T(SyncedExampleHooks, stored::ExampleHooksBase) {
	STORE_CLASS(SyncedExampleHooks, stored::ExampleHooksBase)
public:
	SyncedExampleHooks() {}

	void __some_function(bool set, int32_t& value)
	{
		if(!set)
			value = 42;
	}

	void __hookEntryX(stored::Type::type type, void* buffer, size_t len) noexcept
	{
		base::__hookEntryX(type, buffer, len);

		printf("entry_x(%u, %p, %zu) key=%" PRIxPTR "\n", (unsigned)type, buffer, len,
		       bufferToKey(buffer));
	}

	void __hookExitX(stored::Type::type type, void* buffer, size_t len, bool changed) noexcept
	{
		printf("exit_x(%u, %p, %zu, %schanged) key=%" PRIxPTR "\n", (unsigned)type, buffer,
		       len, changed ? "" : "un", bufferToKey(buffer));

		base::__hookExitX(type, buffer, len, changed);
	}

	void __hookEntryRO(stored::Type::type type, void* buffer, size_t len) noexcept
	{
		base::__hookEntryRO(type, buffer, len);

		printf("entry_ro(%u, %p, %zu) key=%" PRIxPTR "\n", (unsigned)type, buffer, len,
		       bufferToKey(buffer));
	}

	void __hookExitRO(stored::Type::type type, void* buffer, size_t len) noexcept
	{
		printf("exit_ro(%u, %p, %zu) key=%" PRIxPTR "\n", (unsigned)type, buffer, len,
		       bufferToKey(buffer));

		base::__hookExitRO(type, buffer, len);
	}

	void __hookChanged(stored::Type::type type, void* buffer, size_t len) noexcept
	{
		printf("changed(%u, %p, %zu) key=%" PRIxPTR "\n", (unsigned)type, buffer, len,
		       bufferToKey(buffer));

		base::__hookChanged(type, buffer, len);
	}
};

int main()
{
	SyncedExampleHooks store;

	printf("Function access (no hooks)\n");
	store.some_function.get();
	store.some_function.set(10);

	printf("\nRead-only access to typed object\n");
	store.variable_1.get();

	printf("\nRead-only access to variant object\n");
	int32_t v;
	store.find("/variable 2").get(&v, sizeof(v));

	printf("\nExclusive access to typed object\n");
	store.variable_1 = 11;

	printf("\nExclusive access to variant object\n");
	v = 3;
	store.find("/variable 2").set(&v, sizeof(v));
}
