#include <stored>

#include "ExampleSync.h"

#include <cstdio>

class SyncedExampleSync : public stored::ExampleSyncBase<SyncedExampleSync> {
	CLASS_NOCOPY(SyncedExampleSync)
public:
	typedef stored::ExampleSyncBase<SyncedExampleSync> base;
	friend class stored::ExampleSyncBase<SyncedExampleSync>;
	SyncedExampleSync() {}

protected:
	void __function(bool set, int32_t& value) { if(!set) value = 42; }

	void __hookEntryX(stored::Type::type type, void* buffer, size_t len) {
		printf("entry_x(%u, %p, %zu) key=%" PRIxPTR "\n",
			(unsigned)type, buffer, len, bufferToKey(buffer));
	}

	void __hookExitX(stored::Type::type type, void* buffer, size_t len, bool changed) {
		printf("exit_x(%u, %p, %zu, %schanged) key=%" PRIxPTR "\n",
			(unsigned)type, buffer, len, changed ? "" : "un", bufferToKey(buffer));
	}

	void __hookEntryRO(stored::Type::type type, void* buffer, size_t len) {
		printf("entry_ro(%u, %p, %zu) key=%" PRIxPTR "\n",
			(unsigned)type, buffer, len, bufferToKey(buffer));
	}

	void __hookExitRO(stored::Type::type type, void* buffer, size_t len) {
		printf("exit_ro(%u, %p, %zu) key=%" PRIxPTR "\n",
			(unsigned)type, buffer, len, bufferToKey(buffer));
	}
};

int main() {
	SyncedExampleSync store;

	printf("Function access (no hooks)\n");
	store.function.get();
	store.function.set(10);

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
