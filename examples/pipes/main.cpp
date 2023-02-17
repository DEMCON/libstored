/*!
 * \file
 * \brief Pipes example.
 *
 * Pipes can be used to compose functionality, such that data streams through a
 * pipe and is modified on the go. Pipes are sequence of objects, which inspect
 * and/or modify the data that is passed through it.
 *
 * Of course, you can write normal functions to implement all this behavior,
 * but the pipe concept turns out to be very useful in case the operations on
 * data become complex and non-centralized. We used it in GUIs, where raw data
 * from sensors are type-converted, unit-converted, checked for boundaries,
 * written changes to the log, multiple views that are synchronized in
 * user-selected units, rate limited for GUI updates, and complex switching of
 * model data below the view logic.
 *
 * It is powerful in the sense that every pipe segment deals with its own
 * functional thing, while the combination of segments can become very complex.
 * Additionally, adding/removing parts of the pipe is easy to do. For example,
 * if you want to add logging afterwards, inserting a Log segment is trivial,
 * without worrying about that some corner cases or code paths do not hit your
 * logging operation, which may be harder in a normal imperative approach with
 * functions.
 *
 * This examples gives an impression what you could do with pipes. This example
 * only uses primitive types as the pipe data type, but actually any type is
 * supported (while moving/copying of data optimized through the pipe). The
 * library provides a series of standard segments.  Writing one yourself is
 * easy; any class/struct can be as segment, as long as it implements the
 * inject() function.
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

	// In this example, assume we have some measurement data in the store.
	// We want to visualize this on some GUI. Assume that the user can
	// select the unit for this visualization. Let's construct the
	// following pipes:

	// This pipe converts input data in SI units to m.
	auto data_m =
		// Assume that data in the store is in SI units, so m in this
		// case...
		Entry<double>{} >>
		// ...save the last value, which is already in the requested
		// unit m.
		Buffer<double>{} >>
		//
		Exit{};

	// Now, data_m is an object, with consists of a sequence of
	// Entry/Buffer/Exit in this case.  These three pipe segments are
	// combined (at compile time), and cannot be addressed and split
	// separately.  The resulting data_m pipe may be connected dynamically
	// to other pipes, though.

	// This pipe converts input data in SI units to km.
	auto data_km =
		// When SI data received...
		Entry<double>{} >>
		// ...divide by 1000 to convert m into km...
		Convert{Scale<double, std::milli>{}} >>
		// ...and save for later.
		Buffer<double>{} >>
		//
		Exit{};

	// This is the raw input data handling pipe.
	auto data =
		// Data is received from the store...
		Entry<double>{} >>
		// ...and written to the terminal...
		Call{[](double x) { printf("changed %g m\n", x); }} >>
		// ...and forwarded to both pipes for unit conversion.
		Tee{data_m, data_km} >>
		//
		Cap{};

	// This pipe actually reads data from the store.
	auto getter =
		// When something is injected...
		Entry<bool>{} >>
		// ...retrieve data from the store...
		Get{store.sensor} >>
		// ...cast it to double...
		Cast<float, double>{} >>
		// ...upon changes, forward the value to the 'data' pipe.
		Changes{data, similar_to<double>{}} >>
		//
		Cap{};

	// We only have two units in this example.
	enum class Unit { m, km };

	// This pipe converts the enum value to a string.
	auto view_unit =
		// A unit is received...
		Entry<Unit>{} >>
		// ...and we are using a lookup table to convert it to string...
		Mapped(make_random_map<Unit, char const*>({{Unit::m, "m"}, {Unit::km, "km"}})) >>
		// ...and save the output.
		Buffer<char const*>{} >>
		//
		Exit{};

	// Create some class that allows dynamic callbacks to be connected.
	// Something like Qt's signal/slot mechanism.
	stored::Signal<void*, void*, double> sig;

	// This is the view, which allows the unit selection.
	auto view =
		// Upon unit entry...
		Entry<Unit>{} >>
		// ...split of the selected unit for string conversion...
		Tee{view_unit} >>
		// ...map the Unit to an index, corresponding with the Mux below...
		Mapped(make_random_map<Unit, size_t>({{Unit::m, 0}, {Unit::km, 1}})) >>
		// ...retrieve the data from the proper unit converted pipe...
		Mux{data_m, data_km} >>
		// ...signal sig to indicate that the data has changed...
		Signal{sig} >>
		//
		Exit{};

	// Let's connect some callback to sig. In case you have Qt, you may
	// trigger some Qt signal to actually update the view.  (However, there
	// are also other ways to do that, like using the Call pipe segment,
	// and probably you want also some RateLimit first.)
	sig.connect([](double x) { printf("signalled %g\n", x); });


	// The following plumbing has be realized:
	//
	//         getter
	//
	//           ||
	//           vv
	//
	//          data
	//
	//           || tee
	//           ||
	//   //======[]======\|
	//   ||              ||
	//   vv              vv
	//
	// data_m          data_km
	//                               ||
	//   ||              ||          vv
	//   ||              ||
	//   ||              \\=====    view  =====> view_unit
	//   \\=====================
	//                       mux     ||
	//                               ||
	//                               vv



	// Let's test:
	store.sensor = 1.F;
	printf("\nUpdate the data from the store:\n");
	true >> getter;

	printf("\nUpdate the data from the store, but without changes:\n");
	// For the Getter(), trigger() is the same as injecting data. This is
	// probably cleaner, though.
	getter.trigger();

	// Assume the data has changed.
	store.sensor = 10.F;
	printf("\nUpdate the data:\n");
	getter.trigger();

	// Now the view is actually updated.
	printf("\nSelect km:\n");
	double x = Unit::km >> view;
	printf("sensor view = %g %s\n", x, view_unit.extract().get());

	printf("\nSelect m:\n");
	x = Unit::m >> view;
	printf("sensor view = %g %s\n", x, view_unit.extract().get());

	printf("\nSensor update:\n");
	store.sensor = 11.F;
	getter.trigger();
	printf("sensor view = %g %s\n", view.extract().get(), view_unit.extract().get());
}

static void setpoint()
{
	using namespace stored::pipes;

	stored::ExamplePipes store;

	// For this example, envision that we have a setpoint in the store.
	// Some GUI visualizes this setpoint. Additionally, the user can open a
	// popup and edit the setpoint.  While the user is editing, the
	// setpoint should not be written to the store, until the user presses
	// some OK button.
	//
	// The pipes we need are the following:

	printf("\n\nInitializing:\n");

	// A pipe that performs the actual store write.
	auto setter =
		// Upon injected data...
		Entry<double>{} >>
		// ...and log that we are going to write to the store...
		Log<double>{"setter setpoint"} >>
		// ...convert to the store's type...
		Cast<double, float>{} >>
		// ...and write to the store.
		Set{store.setpoint} >>
		//
		Exit{};

	// The editor popup, which holds the new data for a while.
	auto editor =
		// Let's say, here is data entered in some text field...
		Entry<double>{} >>
		// ...and it is saved, until trigger() is called. And if so, it
		// is forwarded to the setter pipe...
		Triggered{setter} >>
		// ...and log all changes to this setpoint value.
		Log<double>{"edited setpoint"} >>
		//
		Exit{};

	// The main view of the store's setpoint value.
	auto view =
		// When new data comes in...
		Entry<float>{} >>
		// ...properly convert it...
		Cast<float, double>{} >>
		// ...log all received values...
		Log<double>{"view setpoint"} >>
		// ...and when there are changes, forward these to the
		// editor. Let's say, if the underlaying data changes, you
		// probably want to reflect this in the input field...
		Changes{editor} >>
		// ...and save the data for future extract()s.
		Buffer<double>{} >>
		//
		Exit{};

	// Some mechanism to retrieve data from the underlaying store, if it
	// would be modified concurrently.
	auto getter =
		// Upon any injection...
		Entry<bool>{} >>
		// ...retrieve data from the store (although you could also do
		// trigger()).
		Get{store.setpoint} >>
		//
		Exit{};


	// Forward output of the setter to the view.
	setter >> view;
	// Forward explicitly read data to the view.
	getter >> view;



	// Now, we constructed the following plumbing:
	//
	//                      getter
	//
	//                        ||
	//                        VV
	//
	// setter =============> view
	//
	//  ^^                    || when changed
	//  ||                    VV
	//  ||
	//  \\================= editor
	//   when triggered



	// Let's test it.
	//
	// This will write 1 into the store. Expect also a log entry on the
	// console from the view and the editor.
	printf("\nWrite the store via the setter:\n");
	1 >> setter;
	printf("store.setpoint = %g\n\n", (double)store.setpoint.get());

	// We can do this again. Expect three more log lines on the console.
	2 >> setter;

	printf("\nEdit the store and trigger the getter:\n");
	store.setpoint = 3;
	// This will read the data from the store, and update the view and editor.
	getter.trigger();

	printf("\nEnter data in the editor, but do not write it yet:\n");
	// This data is only saved in the editor pipe. As long as the user does
	// not press OK, do not really write it to the store.
	4 >> editor;

	printf("\nNow, the user accepts the input:\n");
	// Let's say, the user pressed OK.
	editor.trigger();

	printf("\nAgain, but the data has not changed:\n");
	// So, no additional setter/view log lines are expected.
	editor.trigger();
}

int main()
{
	measurement();
	setpoint();
}
