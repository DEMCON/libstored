// SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
//
// SPDX-License-Identifier: CC0-1.0

/*!
 * \file
 * \brief libstored's control example.
 *
 * This example instantiates several control related components.  All inputs
 * and output are mapped onto <tt>/interconnect/x</tt>, and this mapping can be
 * changed dynamically.  This allows you to play around with the sequence of
 * the components.
 *
 * The default configuration is:
 *
 * - sine wave sets the duty cycle of pulse
 * - pulse via ramp as setpoint to PID
 * - PID output via amplifier and lowpass filter to PID input
 *
 * C++14 is required for this example, as stored/components.h requires it.
 */

#include "ExampleControl.h"

#include <stored>

#include <chrono>
#include <iostream>
#include <utility>

class ExampleControlStore : public STORE_T(ExampleControlStore, stored::ExampleControlBase) {
	STORE_CLASS(ExampleControlStore, stored::ExampleControlBase)
public:
	ExampleControlStore() = default;

	// You can change the control frequency dynamically.
	void __frequency_Hz(bool set, float& value)
	{
		if(!set) {
			value = m_frequency_Hz;
		} else {
			m_frequency_Hz = std::max(0.1f, value);
			pid__reset = true;
			lowpass__reset = true;
			ramp__reset = true;
		}
	}

	void __pid__frequency_Hz(bool set, float& value)
	{
		if(!set)
			value = frequency_Hz.get();
		else
			frequency_Hz = value;
	}

	void __sine__sample_frequency_Hz(bool set, float& value)
	{
		if(!set)
			value = frequency_Hz.get();
		else
			frequency_Hz = value;
	}

	void __pulse__sample_frequency_Hz(bool set, float& value)
	{
		if(!set)
			value = frequency_Hz.get();
		else
			frequency_Hz = value;
	}

	void __lowpass__sample_frequency_Hz(bool set, float& value)
	{
		if(!set)
			value = frequency_Hz.get();
		else
			frequency_Hz = value;
	}

	void __ramp__sample_frequency_Hz(bool set, float& value)
	{
		if(!set)
			value = frequency_Hz.get();
		else
			frequency_Hz = value;
	}

private:
	float m_frequency_Hz{10.0f};
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
	if(!sine_v.isHealthy())
		std::cout << "/sine not healthy" << std::endl;

	auto x_output = store.interconnect__x_a(store.sine__x_output.get());
	if(x_output.valid())
		x_output.set(y);
}

static void pulse()
{
	constexpr auto pulse_o = stored::PulseWave<ExampleControlStore>::objects("/pulse/");
	static stored::PulseWave<ExampleControlStore, pulse_o.flags()> pulse_v{pulse_o, store};

	auto x_duty_cycle = store.interconnect__x_a(store.pulse__x_duty_cycle.get());
	if(x_duty_cycle.valid())
		store.pulse__duty_cycle = x_duty_cycle.get<value_type>();

	auto y = pulse_v();
	if(!pulse_v.isHealthy())
		std::cout << "/pulse not healthy" << std::endl;

	auto x_output = store.interconnect__x_a(store.pulse__x_output.get());
	if(x_output.valid())
		x_output.set(y);
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

static void ramp()
{
	constexpr auto ramp_o = stored::Ramp<ExampleControlStore>::objects("/ramp/");
	static stored::Ramp<ExampleControlStore, ramp_o.flags()> ramp_v{ramp_o, store};

	auto x_input = store.interconnect__x_a(store.ramp__x_input.get());
	if(x_input.valid())
		store.ramp__input = x_input.get<value_type>();

	auto output = ramp_v();
	if(!ramp_v.isHealthy())
		std::cout << "/ramp not healthy" << std::endl;

	auto x_output = store.interconnect__x_a(store.ramp__x_output.get());
	if(x_output.valid())
		x_output.set(output);
}

static void control()
{
	using f_type = std::pair<void (*)(), stored::Variable<uint8_t, ExampleControlStore>>;

	static std::array<f_type, 6> fs = {{
		f_type{&pid, store.pid__evaluation_order},
		f_type{&amp, store.amp__evaluation_order},
		f_type{&sine, store.sine__evaluation_order},
		f_type{&pulse, store.pulse__evaluation_order},
		f_type{&lowpass, store.lowpass__evaluation_order},
		f_type{&ramp, store.ramp__evaluation_order},
	}};

	std::stable_sort(fs.begin(), fs.end(), [](f_type const& a, f_type const& b) {
		return a.second.get() < b.second.get();
	});

	for(auto const& f : fs)
		f.first();
}

int main()
{
	printf("Dynamically change the interconnections between the components\n");
	printf("by modifying the /<component>/x <variable>.\n");

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
	stored::PollableZmqLayer pollableZmq(zmqLayer, stored::Pollable::PollIn);

	if((errno = poller.add(pollableZmq))) {
		printf("Cannot add to poller; %s (error %d)\n", zmq_strerror(errno), errno);
		exit(1);
	}

	auto t = std::chrono::system_clock::now();

	while(true) {
		auto now = std::chrono::system_clock::now();
		auto rem = std::chrono::duration_cast<std::chrono::microseconds>(t - now);
		auto rem_us = rem.count();

		if(rem_us <= 0) {
			t += std::chrono::milliseconds(
				(long long)(1.0e3f / store.frequency_Hz.get()));
			// This is where the magic takes place.
			control();
			continue;
		}

		if(poller.poll(std::max<int>(0, (int)(rem_us / 1000L))).empty()) {
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
