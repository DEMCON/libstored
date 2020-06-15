#include <stored>

#include "SomeStore.h"
#include "AnotherStore.h"

int main() {
	stored::SomeStore someStore1;
	stored::SomeStore someStore2;
	stored::AnotherStore anotherStore;

	stored::Debugger debugger;
	debugger.map(someStore1, "/SomeStore");
	debugger.map(someStore2, "/OtherInstanceOfSomeStore");
	debugger.map(anotherStore);

	int32_t i;
	stored::DebugVariant i1 = debugger.find("/SomeStore/i");
	i1.get(&i, sizeof(i));
	printf("/SomeStore/i = %" PRId32 "\n", i);
	i++;
	i1.set(&i, sizeof(i));
	printf("/SomeStore/i = %" PRId32 "\n", i);
	
	stored::DebugVariant i2 = debugger.find("/OtherInstanceOfSomeStore/i");
	i2.get(&i, sizeof(i));
	printf("/OtherInstanceOfSomeStore/i = %" PRId32 "\n", i);

	stored::DebugVariant j = debugger.find("/AnotherStore/j");
	j.get(&i, sizeof(i));
	printf("/AnotherStore/j = %" PRId32 "\n", i);
}

