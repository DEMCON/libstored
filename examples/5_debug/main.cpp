#include <stored>

#include "ExampleDebugSomeStore.h"
#include "ExampleDebugAnotherStore.h"

#ifdef STORED_COMPILER_MSVC
#  define strdup(s) _strdup(s)
#endif

class PrintfPhysical : public stored::ProtocolLayer {
public:
	typedef stored::ProtocolLayer base;

	PrintfPhysical(ProtocolLayer& up)
		: base(&up)
		, m_encoding()
	{
		up.setDown(this);
	}

	void decode(char const* frame) {
		char* s = strdup(frame);
		if(s)
			decode(s, strlen(s));
		free(s);
	}

	void decode(void* buffer, size_t len) override final {
		printf(">>   %.*s\n", (int)len, (char const*)buffer);
		base::decode(buffer, len);
	}

	void encode(void* buffer, size_t len, bool last = true) override final {
		encode((void const*)buffer, len, last);
	}

	void encode(void const* buffer, size_t len, bool last = true) override final {
		if(!m_encoding) {
			printf("<<   ");
			m_encoding = true;
		}

		if(len)
			printf("%.*s", (int)len, (char const*)buffer);

		if(last) {
			printf("\n");
			m_encoding = false;
		}
	}

private:
	bool m_encoding;
};

int main() {
	// Create a few stores.
	stored::ExampleDebugSomeStore someStore1;
	stored::ExampleDebugSomeStore someStore2;
	stored::ExampleDebugAnotherStore anotherStore;

	// Register them to a debugger.
	stored::Debugger debugger("5_debug");
	debugger.setVersions("123");
	debugger.map(someStore1, "/SomeStore");
	debugger.map(someStore2, "/OtherInstanceOfSomeStore");
	debugger.map(anotherStore); // Use default name.

	// Some accesses to the stores objects using the full prefix.
	int32_t i;
	// The stored::DebugVariant is a bit more expensive than directly
	// accessing the store's accessors, but allows a template-independant interface,
	// as the debugger will operate only on such an interface.
	stored::DebugVariant i1 = debugger.find("/SomeStore/i");
	i1.get(&i, sizeof(i));
	printf("/SomeStore/i = %" PRId32 "\n", i);
	i++;
	i1.set(&i, sizeof(i));
	printf("/SomeStore/i = %" PRId32 "\n", someStore1.i().get());
	
	stored::DebugVariant i2 = debugger.find("/OtherInstanceOfSomeStore/i");
	i2.get(&i, sizeof(i));
	printf("/OtherInstanceOfSomeStore/i = %" PRId32 "\n", i);

	stored::DebugVariant j = debugger.find("/ExampleDebugAnotherStore/j");
	j.get(&i, sizeof(i));
	printf("/ExampleDebugAnotherStore/j = %" PRId32 "\n", i);

	// DebugVariants are small, copyable and assignable, so they can be used in
	// std::map, for example.
	i2 = i1; // let i2 point to /SomeStore/i
	i2.get(&i, sizeof(i));
	printf("i2 = %" PRId32 "\n", i);

	// Now process some Embedded Debugger messages
	PrintfPhysical phy(debugger);
	phy.decode("?");
	phy.decode("i");
	phy.decode("v");
	phy.decode("r/ExampleDebugAnotherStore/j");
	phy.decode("wf00f/SomeStore/i");
	phy.decode("r/SomeStore/i");
	phy.decode("eHello World!!1");
	phy.decode("l");
	phy.decode("a0/SomeStore/i");
	phy.decode("r0");
	phy.decode("m* r0 e; r0 e; r/ExampleDebugAnotherStore/j");
	phy.decode("*");
	phy.decode("m*");

	int mem = 0xbeef;
	char buffer[32] = {};
	snprintf(buffer, sizeof(buffer), "R%" PRIxPTR " %zu", (uintptr_t)&mem, sizeof(mem));
	phy.decode(buffer);
	
	snprintf(buffer, sizeof(buffer), "W%" PRIxPTR " cafe", (uintptr_t)&mem);
	phy.decode(buffer);

	printf("mem = 0x%x\n", mem);

	return 0;
}

