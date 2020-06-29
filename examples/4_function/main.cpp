#include "ExampleFunction.h"

#include <ctime>

#ifdef STORED_COMPILER_MSVC
#  pragma warning(disable: 4996)
#endif

// Create a subclass of stored::ExampleFunctionBase to define the side effects of the functions.
class MyExample : public stored::ExampleFunctionBase<MyExample> {
	CLASS_NOCOPY(MyExample)
public:
	typedef stored::ExampleFunctionBase<MyExample> base;
	friend class stored::ExampleFunctionBase<MyExample>;
	MyExample() {}

protected:
	// Override the default functions from base. Even though they are not
	// virtual, these are called as expected.  To do this, you had to specify
	// the class as the template parameter of ExampleFunctionBase :)

	// The function gets the parameter set, which indicates if the value is
	// written (true) or should be returned (false).
	void __time_s(bool set, uint64_t& value) {
		if(set)
			return; // read-only

		value = (uint64_t)time(NULL);
	}

	void __echo_0(bool set, int32_t& value) { __echo(0, set, value); }
	void __echo_1(bool set, int32_t& value) { __echo(1, set, value); }

private:
	void __echo(int i, bool set, int32_t& value) {
		assert(i >= 0 && (size_t)i < sizeof(m_echo) / sizeof(m_echo[0]));

		if(set)
			m_echo[i] = value;
		else
			value = m_echo[i];

		printf("%s echo[%d] = %" PRId32 "\n", set ? "set" : "get", i, value);
	}

private:
	int32_t m_echo[2];
};

int main() {
	MyExample e;

	time_t now = (time_t)e.time_s().get();
	printf("time = %s\n", ctime(&now));

	e.echo_0().set(10);
	e.echo_1().set(11);
	printf("echo[0] returned %" PRId32 "\n", e.echo_0().get());
	printf("echo[1] returned %" PRId32 "\n", e.find("/echo[1]").function<int32_t>().get());

	return 0;
}

