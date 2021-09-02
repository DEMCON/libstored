/*!
 * \file
 * \brief libstored's control example.
 */

#include "ExampleControl.h"

#include <stored>

#include <chrono>
#include <iostream>
#include <utility>

class ExampleControlStore : public STORE_BASE_CLASS(ExampleControlBase, ExampleControlStore) {
	STORE_CLASS_BODY(ExampleControlBase, ExampleControlStore)
public:
	ExampleControlStore() = default;

	// You can change the control frequency dynamically.
	void __pid__frequency_Hz(bool set, float& value)
	{
		if(!set)
			value = frequency_Hz.get();
	}

	void __sine__sample_frequency_Hz(bool set, float& value)
	{
		if(!set)
			value = frequency_Hz.get();
	}

	void __lowpass__sample_frequency_Hz(bool set, float& value)
	{
		if(!set)
			value = frequency_Hz.get();
	}
};

static ExampleControlStore store;
using value_type = decltype(std::declval<ExampleControlStore>().interconnect__x_0.get());

static void pid()
{
	constexpr auto pid_o = stored::PID<ExampleControlStore>::objects("/pid/");
	static stored::PID<ExampleControlStore, pid_o.flags()> pid_v{pid_o, store};

	auto x_setpoint = store.interconnect__x_a(store.pid__x_setpoint.get());
	if(x_setpoint.valid())
		store.pid__setpoint = x_setpoint.get<value_type>();

	auto x_y = store.interconnect__x_a(store.pid__x_y.get());
	if(x_y.valid())
		store.pid__y = x_y.get<value_type>();

	auto u = pid_v();
	if(!pid_v.isHealthy())
		std::cout << "/pid not healthy" << std::endl;

	auto x_u = store.interconnect__x_a(store.pid__x_u.get());
	if(x_u.valid())
		x_u.set(u);
}

static void amp()
{
	constexpr auto amp_o = stored::Amplifier<ExampleControlStore>::objects("/amp/");
	static stored::Amplifier<ExampleControlStore, amp_o.flags()> amp_v{amp_o, store};

	auto x_input = store.interconnect__x_a(store.amp__x_input.get());
	if(x_input.valid())
		store.amp__input = x_input.get<value_type>();

	auto output = amp_v();

	auto x_output = store.interconnect__x_a(store.amp__x_output.get());
	if(x_output.valid())
		x_output.set(output);
}

static void sine()
{
	constexpr auto sine_o = stored::Sine<ExampleControlStore>::objects("/sine/");
	static stored::Sine<ExampleControlStore, sine_o.flags()> sine_v{sine_o, store};

	auto y = sine_v();

	auto x_y = store.interconnect__x_a(store.sine__x_y.get());
	if(x_y.valid())
		x_y.set(y);
}

static void lowpass()
{
	constexpr auto lowpass_o = stored::LowPass<ExampleControlStore>::objects("/lowpass/");
	static stored::LowPass<ExampleControlStore, lowpass_o.flags()> lowpass_v{lowpass_o, store};

	auto x_input = store.interconnect__x_a(store.lowpass__x_input.get());
	if(x_input.valid())
		store.lowpass__input = x_input.get<value_type>();

	auto output = lowpass_v();

	auto x_output = store.interconnect__x_a(store.lowpass__x_output.get());
	if(x_output.valid())
		x_output.set(output);
}

static void control()
{
	printf("tick\n");

	using f_type = std::pair<void(*)(), stored::Variable<uint8_t, ExampleControlStore>>;

	static std::array<f_type, 4> fs = {
		f_type{&pid, store.pid__evaluation_order},
		f_type{&amp, store.amp__evaluation_order},
		f_type{&sine, store.sine__evaluation_order},
		f_type{&lowpass, store.lowpass__evaluation_order},
	};

	std::stable_sort(fs.begin(), fs.end(),
		[](f_type const& a, f_type const& b) { return a.second.get() < b.second.get(); });

	for(auto const& f : fs)
		f.first();
}

int main()
{

	// Construct the protocol stack.
	stored::Debugger debugger{"control"};
	debugger.map(store);

	stored::DebugZmqLayer zmqLayer;
	if((errno = zmqLayer.lastError())) {
		printf("Cannot initialize ZMQ layer; %s (error %d)\n", zmq_strerror(errno), errno);
		exit(1);
	}
	zmqLayer.wrap(debugger);

	stored::Poller poller;

	if((errno = poller.add(zmqLayer, nullptr, stored::Poller::PollIn))) {
		printf("Cannot add to poller; %s (error %d)\n", zmq_strerror(errno), errno);
		exit(1);
	}

	auto t = std::chrono::system_clock::now();

	while(true) {
		auto now = std::chrono::system_clock::now();
		auto rem = std::chrono::duration_cast<std::chrono::microseconds>(t - now);
		auto rem_us = rem.count();

		if(rem_us <= 0) {
			t += std::chrono::milliseconds((long long)(1.0e3f / store.frequency_Hz.get()));
			// This is where the magic takes place.
			control();
			continue;
		}

		if(poller.poll(std::max(0L, (long)rem_us)).empty()) {
			switch(errno) {
			case EINTR:
			case EAGAIN:
				break;
			default:
				perror("Cannot poll");
				exit(1);
			} // else timeout
		} else if((errno = zmqLayer.recv())) {
			printf("Cannot recv; %s (error %d)\n", zmq_strerror(errno), errno);
			exit(1);
		}
	}
}
