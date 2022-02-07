/*!
 * \file
 * \brief Example to show how to use generated *Meta.py for further store
 *	processing.
 *
 * This example uses the generated meta data to generate another wrapper for a
 * store.  In this case, a wrapper that prints changes to the store of every
 * variable.
 *
 * The generator used for this example, takes libstored/doc/ExampleMetaMeta.py
 * and the jinja2 template input LoggingWrapper.h.tmpl, and generates the
 * store-specific output file LoggingExampleMeta.h.
 */

#include "ExampleMeta.h"
#include "LoggingExampleMeta.h"

#include <stored>

// Create an ExampleMeta store, which uses the generated LoggingExampleMeta as
// a wrapper.
class ExampleMeta : public STORE_T(ExampleMeta, LoggingExampleMeta, stored::ExampleMetaBase) {
	STORE_CLASS(ExampleMeta, LoggingExampleMeta, stored::ExampleMetaBase)
public:
	ExampleMeta() is_default
};

int main()
{
	ExampleMeta store;

	// No change expected, no output.
	store.some_int = 42;

	// Expect some logging output.
	store.a_double = 2.718;
	store.world.set("hi");
}
