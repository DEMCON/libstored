/*!
 * \file
 * \brief libstored's components example.
 *
 * When building some control application, e.g., one that drives a motor, you
 * would like to have access to all hardware pins, all ADC conversion
 * parameters, all controllers, etc. A common design pattern is to add these
 * parameters to a store, and instantiate the corresponding components in C++.
 * Then, you can access, override, and tune the application via the store.
 *
 * libstored provides several of such components, such as GPIO pins, Amplifier,
 * and PID controller. This example shows how to instantiate and use such a
 * component class and how it is coupled to your store. You will need at least
 * C++14 for this.
 *
 * And don't crash the helicopter...
 */

#include "ExampleComponents.h"

#include <stored>

#include <chrono>
#include <iostream>

class ExampleComponentsStore : public STORE_BASE_CLASS(ExampleComponentsBase, ExampleComponentsStore) {
	STORE_CLASS_BODY(ExampleComponentsBase, ExampleComponentsStore)
public:
	ExampleComponentsStore() = default;

	// You can change the control frequency dynamically.
	void __pid__frequency_Hz(bool set, float& value)
	{
		if(set) {
			if(std::isnan(value) || value <= 0.1f)
				value = 0.1f;

			m_frequency_Hz = value;
		} else {
			value = m_frequency_Hz;
		}
	}
private:
	float m_frequency_Hz{5.0};
};

static ExampleComponentsStore store;

static float fly(float power)
{
	// Greatly simplified model of a helicopter. The power lets the blades
	// spin. If you have enough lift, you can take off.

	float dt = 1.0f / store.pid__frequency_Hz();
	float G = store.environment__G_m__s_2;
	float air_pressure = store.environment__surface_air_pressure_Pa;
	float air_molar_mass = store.environment__air_molar_mass_kg__mol;
	float temperature = store.environment__temperature_K;

	float height = store.helicopter__height_m;
	float speed = store.helicopter__speed_m__s;

	power = std::max(0.f, std::min(1.f, power));
	if(std::isnan(power))
		power = 0;

	float air_density = air_pressure * std::exp(-(G * height * air_molar_mass) / (temperature * 8.314462618f)) / (air_molar_mass * temperature);

	float lift = .5f * air_density * std::pow(power * store.helicopter__motor_constant, 2.0f) * store.helicopter__lift_coefficient;
	float drag = .5f * air_density * std::pow(speed, 2.0f) * store.helicopter__drag_coefficient;
	float weight = store.helicopter__mass_kg * G;

	float F = lift - weight;
	if(speed > 0)
		F -= drag;
	else
		F += drag;

	float accelleration = F / store.helicopter__mass_kg;

	speed += accelleration * dt;
	height += speed * dt;

	if(height < 0) {
		if(speed < -1)
			std::cout << " ... Crash ... " << std::endl;
		height = 0;
		speed = 0;
	}

	store.helicopter__speed_m__s = speed;
	store.helicopter__height_m = height;

	std::cout
		<< "power throttle: " << power << "  "
		<< "height: " << height << " m  "
		<< "speed: " << speed << " m/s  "
		<< "lift: " << lift << " N  "
		<< "drag: " << drag << " N  "
		<< "F: " << F << " N  "
		<< "acc: " << accelleration << " m/s^2  "
		<< "air density: " << air_density << " kg/m^3"
		<< std::endl;
	return height;
}

int main()
{
	std::cout << "Helicopter flight simulator" << std::endl << std::endl;
	std::cout << "Try to fly this helicopter, using a poorly-tuned (PID) controller." << std::endl;
	std::cout << "Connect via ZMQ and set /pid/setpoint to the desired height." << std::endl;
	std::cout << "For example, set it to 1000, and see the heli take off." << std::endl;
	std::cout << "Tune all parameters at will and see what happens." << std::endl << std::endl;

	// This is the PID controller.
	// This line finds all variables within the /pid/ scope, that are to be
	// used by the PID instance. All lookup is done at compile-time. pid_o
	// holds a set of flags that can be used to leave out unused (optional)
	// parameters.

	constexpr auto pid_o = stored::PID<ExampleComponentsStore>::objects("/pid/");
	// Now, instantiate the PID, tailored to the variables in your store.  The
	// pid_o is also passed to the constructor to prove the addresses of the
	// variables in the store, as found by (constexpr) find() in the store's
	// directory.
	stored::PID<ExampleComponentsStore, pid_o.flags()> pid{pid_o, store};

	// Construct the protocol stack.
	stored::Debugger debugger{"components"};
	debugger.map(store);

	stored::DebugZmqLayer zmqLayer;
	if(zmqLayer.lastError()) {
		perror("Cannot initialize ZMQ layer");
		exit(1);
	}
	zmqLayer.wrap(debugger);

	stored::Poller poller;
	stored::PollableZmqLayer pollableZmq(zmqLayer, stored::Pollable::PollIn);

	if((errno = poller.add(pollableZmq))) {
		perror("Cannot add to poller");
		exit(1);
	}

	// Determine polling/control interval.
	auto dt = std::chrono::milliseconds((long long)(1.0e3f / store.pid__frequency_Hz()));
	auto t = std::chrono::system_clock::now() + dt;

	while(true) {
		auto now = std::chrono::system_clock::now();
		auto rem = std::chrono::duration_cast<std::chrono::microseconds>(t - now);
		auto rem_us = rem.count();

		if(rem_us <= 0) {
			t += std::chrono::milliseconds((long long)(1.0e3f / store.pid__frequency_Hz()));
			// This is where the magic takes place.
			store.pid__y = fly(pid());
			if(!pid.isHealthy())
				std::cout << "Warning: numerically unstable" << std::endl;
			continue;
		}

		if(poller.poll((int)(rem_us / 1000L)).empty()) {
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
