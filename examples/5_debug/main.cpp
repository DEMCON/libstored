/*!
 * \file
 * \brief Example with a debugger instance and two stores.
 */

#include <stored>

#include "ExampleDebugSomeStore.h"
#include "ExampleDebugAnotherStore.h"

#include <cstdio>

#ifdef STORED_COMPILER_MSVC
#  define strdup(s) _strdup(s)
#endif

// A 'physical layer' that sends the outgoing (encoded) data to print().
class PrintfPhysical : public stored::ProtocolLayer {
	CLASS_NOCOPY(PrintfPhysical)
public:
	typedef stored::ProtocolLayer base;

	explicit PrintfPhysical(ProtocolLayer& up)
		: base(&up)
		, m_encoding()
		, m_silenced()
	{
		up.setDown(this);
	}

	void decode(char const* frame) {
		char* s = strdup(frame);
		if(s)
			decode(s, strlen(s));
		free(s);
	}

	void decode(void* buffer, size_t len) final {
		if(!m_silenced)
			printf(">>   %.*s\n", (int)len, (char const*)buffer);
		base::decode(buffer, len);
	}

	void encode(void const* buffer, size_t len, bool last = true) final {
		if(!m_encoding) {
			if(!m_silenced)
				printf("<<   ");
			m_encoding = true;
		}

		if(len && !m_silenced)
			printf("%.*s", (int)len, (char const*)buffer);

		if(last) {
			if(!m_silenced)
				printf("\n");
			m_encoding = false;
		}
	}

	void silence(bool silenced) {
		m_silenced = silenced;
	}

private:
	bool m_encoding;
	bool m_silenced;
};

// Extend the capabilities with the 'z' command.
class ExtendedDebugger : public stored::Debugger {
	CLASS_NOCOPY(ExtendedDebugger)
public:
	typedef stored::Debugger base;
	explicit ExtendedDebugger(char const* identification = nullptr)
		: base(identification)
	{}

	virtual ~ExtendedDebugger() noexcept override is_default;

	virtual void capabilities(char*& caps, size_t& len, size_t reserve = 0) override {
		// Get the default capabilities.
		base::capabilities(caps, len, reserve + 1 /* add room for our 'z' cmd */);
		// Add our 'z' cmd.
		caps[len++] = 'z';
	}

	virtual void process(void const* frame, size_t len, ProtocolLayer& response) override {
		if(unlikely(!frame || len == 0))
			return;

		char const* p = static_cast<char const*>(frame);

		switch(p[0]) {
		case 'z':
			// That's our cmd. Let's respond with something useful...
			response.encode("Zzzz", 4);
			break;
		default:
			// Not for us, forward to our base.
			base::process(frame, len, response);
		}
	}
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
	printf("/SomeStore/i = %" PRId32 "\n", someStore1.i.get());

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

	// Suppress output, such that the application always prints the same.
	// This is handy for testing the behavior of the application by unit tests.
	phy.silence(true);

	int mem = 0xbeef;
	char buffer[32] = {};
	snprintf(buffer, sizeof(buffer), "R%" PRIxPTR " %zu", (uintptr_t)&mem, sizeof(mem));
	phy.decode(buffer);

	snprintf(buffer, sizeof(buffer), "W%" PRIxPTR " cafe", (uintptr_t)&mem);
	phy.decode(buffer);

	printf("mem = 0x%x\n", mem);

	phy.silence(false);

	phy.decode("s");
	debugger.stream('A', "Hello");
	phy.decode("s");
	phy.decode("sA");
	debugger.stream('A', "stream!!1");
	phy.decode("sA");
	phy.decode("s");
	phy.decode("sA/");
	phy.decode("sB/");


	// Test our debugger with the z capability.
	ExtendedDebugger extdebugger;
	PrintfPhysical extphy(extdebugger);
	extphy.decode("?");
	extphy.decode("z");

	return 0;
}

