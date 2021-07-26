#include "ExampleComponents.h"

#include <stored>

#include <chrono>
#include <iostream>

class ExampleComponentsStore : public STORE_BASE_CLASS(ExampleComponentsBase, ExampleComponentsStore) {
	STORE_CLASS_BODY(ExampleComponentsBase, ExampleComponentsStore)
public:
	ExampleComponentsStore() = default;

	void __pid__frequency_Hz(bool set, float& value) {
		if(!set)
			value = 5.0f;
	}
};

static ExampleComponentsStore store;

float fly(float power)
{
	float G = store.environment__G_m__s_2;
	float air_pressure = store.environment__surface_air_pressure_Pa;
	float air_molar_mass = store.environment__air_molar_mass_kg__mol;
	float temperature = store.environment__temperature_K;

	float height = store.helicopter__height_m;
	float speed = store.helicopter__speed_m__s;

	float air_density = air_pressure * std::exp(-(G * height * air_molar_mass) / (temperature * 8.314462618f)) / (air_molar_mass * temperature);

	float lift = .5f * air_density * std::pow(power * store.helicopter__motor_constant, 2.0f) * store.helicopter__lift_coefficient;
	float drag = .5f * air_density * std::pow(speed, 2.0f) * store.helicopter__drag_coefficient;
	float weight = store.helicopter__mass_kg * G;

	float F = lift - weight - drag;
	float accelleration = F / store.helicopter__mass_kg;

	speed += accelleration;
	height += speed;

	if(height < 0) {
		height = 0;
		speed = 0;
	}

	store.helicopter__speed_m__s = speed;
	store.helicopter__height_m = height;

	std::cout
		<< "height: " << height
		<< "  speed: " << speed
		<< "  lift: " << lift
		<< "  acc: " << accelleration
		<< "  density: " << air_density
		<< std::endl;
	return height;
}

int main()
{
	constexpr auto n1 = stored::FreeObjects<stored::FreeVariable<float, ExampleComponentsStore>, 'i'>::create("/amp/");
	static_assert(n1.size() == 1, "");
	static_assert(n1.validSize() == 1, "");

	// j is not valid
	constexpr auto n2 = stored::FreeObjects<stored::FreeVariable<float, ExampleComponentsStore>, 'i', 'j'>::create("/amp/");
	static_assert(n2.size() == 2, "");
	static_assert(n2.validSize() == 1, "");

	// find by long name
	constexpr auto n3 = stored::FreeObjects<stored::FreeVariable<float, ExampleComponentsStore>, 'i', 'o'>::create("/amp/", "input", "output");
	static_assert(n3.size() == 2, "");
	static_assert(n3.validSize() == 2, "");

	// skip o
	constexpr auto n4 = stored::FreeObjects<stored::FreeVariable<float, ExampleComponentsStore>, 'i', 'o'>::create<'i'>("/amp/", "input", "output");
	static_assert(n4.size() == 2, "");
	static_assert(n4.validSize() == 1, "");

	// fix ambiguity
	constexpr auto n5 = stored::FreeObjects<stored::FreeVariable<float, ExampleComponentsStore>, 'g', 'f', 'o'>::create<'g','o'>("/simple amp/", "gain", "override", "output");
	static_assert(n5.size() == 3, "");
	static_assert(n5.validSize() == 2, "");
	static_assert(n5.template valid<'o'>(), "");
	static_assert(!n5.template valid<'f'>(), "");

	constexpr auto n6 = stored::FreeObjectsList<
		stored::FreeVariables<float, ExampleComponentsStore, 'i', 'g'>,
		stored::FreeVariables<bool, ExampleComponentsStore, 'e'>
	>::create("/amp/");
	static_assert(n6.size() == 3, "");
	static_assert(n6.flags() == 7ULL, "");
	static_assert(n6.valid<'i'>(n6.flags()), "");
	static_assert(n6.valid<'e'>(n6.flags()), "");


	constexpr auto amp_o = stored::Amplifier<ExampleComponentsStore>::objects("/amp/");
	stored::Amplifier<ExampleComponentsStore, amp_o.flags()> amp{amp_o, store};

	std::cout << amp(3) << std::endl;
	std::cout << amp(5) << std::endl;
	std::cout << amp(-2) << std::endl;
	std::cout << sizeof(amp) << std::endl;

	constexpr auto amp_o2 = stored::Amplifier<ExampleComponentsStore>::objects<'g','O'>("/simple amp/");
	using Amp2 = stored::Amplifier<ExampleComponentsStore, amp_o2.flags()>;
	Amp2 amp2;
	amp2 = Amp2{amp_o2, store};
	std::cout << amp2(3) << std::endl;
	std::cout << sizeof(amp2) << std::endl;

	constexpr auto gpio_in_o = stored::PinIn<ExampleComponentsStore>::objects("/gpio in/");
	stored::PinIn<ExampleComponentsStore, gpio_in_o.flags()> gpio_in{gpio_in_o, store};

	std::cout << gpio_in() << std::endl;
	store.gpio_in__override = 1;
	std::cout << gpio_in() << std::endl;

	constexpr auto gpio_out_o = stored::PinOut<ExampleComponentsStore>::objects("/gpio out/");
	stored::PinOut<ExampleComponentsStore, gpio_out_o.flags()> gpio_out{gpio_out_o, store};

	std::cout << gpio_out() << std::endl;
	std::cout << gpio_out(true) << std::endl;
	gpio_out.override_(0);
	std::cout << gpio_out() << std::endl;

	constexpr auto pid_o = stored::PID<ExampleComponentsStore>::objects("/pid/");
	stored::PID<ExampleComponentsStore, pid_o.flags()> pid{pid_o, store};




	stored::Debugger debugger("components");
	debugger.map(store);

	stored::DebugZmqLayer zmqLayer;
	zmqLayer.wrap(debugger);

	printf("Connect via ZMQ to debug this application.\n");

	stored::Poller poller;

	if((errno = poller.add(zmqLayer, nullptr, stored::Poller::PollIn))) {
		perror("Cannot add to poller");
		exit(1);
	}

	auto dt = std::chrono::milliseconds((long long)(1.0e3f / store.pid__frequency_Hz()));
	auto t = std::chrono::system_clock::now() + dt;

	while(true) {
		auto now = std::chrono::system_clock::now();
		auto rem = std::chrono::duration_cast<std::chrono::microseconds>(t - now);
		auto rem_us = rem.count();

		if(rem_us <= 0) {
			t += dt;
			store.pid__y = fly(pid());
			continue;
		}

		if(poller.poll(rem_us).empty()) {
			switch(errno) {
			case EINTR:
			case EAGAIN:
				break;
			default:
				perror("Cannot poll");
				exit(1);
			} // else timeout
		} else if((errno = zmqLayer.recv())) {
			perror("Cannot recv");
			exit(1);
		}
	}
}
