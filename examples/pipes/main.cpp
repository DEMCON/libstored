/*!
 * \file
 * \brief Pipes example.
 *
 * Pipes require at least C++14, but C++17 gives you especially the template
 * deduction guides, which simplifies the pipes.  This example requires C++17.
 */

#include "ExamplePipes.h"

#include <algorithm>
#include <stored>

static void measurement()
{
	using namespace stored::pipes;

	stored::ExamplePipes store;

	// clang-format off
	auto data_m =
		Entry<double>{} >>
		Buffer<double>{} >>
		Exit{};

	auto data_km =
		Entry<double>{} >>
		Convert{Scale<double, std::milli>{}} >>
		Buffer<double>{} >>
		Exit{};

	auto data =
		Entry<double>{} >>
		Call{[](double x) {
			printf("changed %g m\n", x);
		}} >>
		Tee{data_m, data_km} >>
		Exit{};

	auto getter =
		// When something is injected...
		Entry<bool>{} >>
		// ...retrieve data from the store...
		Get{store.sensor} >>
		// ...cast it to double...
		Cast<float, double>{} >>
		// ...upon changes, forward the value to the 'data' pipe.
		Changes{data, similar_to<double>{}} >>
		Cap{};

	enum class Unit { m, km };

	auto view_unit =
		Entry<Unit>{} >>
		Mapped(make_random_map<Unit, char const*>({{Unit::m, "m"}, {Unit::km, "km"}})) >>
		Buffer<char const*>{} >>
		Exit{};

	stored::Signal<void*, void*, double> sig;

	auto view =
		Entry<Unit>{} >>
		Tee{view_unit} >>
		// Map the Unit to an index, corresponding with the Mux below.
		Mapped(make_random_map<Unit, size_t>({{Unit::m, 0}, {Unit::km, 1}})) >>
		// Retrieve the data from the proper unit converted pipe.
		Mux{data_m, data_km} >>
		// Save for later retrieval.
		Signal{sig} >>
		Exit{};

	// clang-format on

	sig.connect([](double x) { printf("signalled %g\n", x); });

	store.sensor = 1.F;
	true >> getter;
	true >> getter;

	store.sensor = 10.F;
	getter.trigger();
	Unit::km >> view;
	printf("sensor view = %g %s\n", view.extract().get(), view_unit.extract().get());
	Unit::m >> view;
	printf("sensor view = %g %s\n", view.extract().get(), view_unit.extract().get());

	store.sensor = 11.F;
	getter.trigger();
	printf("sensor view = %g %s\n", view.extract().get(), view_unit.extract().get());
}

static void setpoint()
{
	using namespace stored::pipes;

	stored::ExamplePipes store;

	// clang-format off
	auto setter =
		Entry<double>{} >>
		Cast<double, float>{} >>
		Set{store.setpoint} >>
		Exit{};

	auto getter =
		Entry<bool>{} >>
		Get{store.setpoint} >>
		Exit{};

	auto editor =
		Entry<double>{} >>
		Triggered<double>{} >>
		Call{[](double x) {
			printf("edited: %g\n", x);
		}} >>
		Exit{};

	auto view =
		Entry<float>{} >>
		Cast<float, double>{} >>
		Call{[](double x) {
			printf("setpoint: %g\n", x);
		}} >>
		Changes{editor} >>
		Buffer<double>{} >>
		Exit{};

	// clang-format on

//	editor >> setter;
	setter >> view;
	getter >> view;

	1 >> setter;
	2 >> setter;
	store.setpoint = 3;
	getter.trigger();
	4 >> editor;
	editor.trigger();
}

int main()
{
	measurement();
	setpoint();
}
