// SPDX-FileCopyrightText: 2020-2024 Jochem Rutgers
//
// SPDX-License-Identifier: CC0-1.0

/*!
 * \file
 * \brief Example to show how to use libstored in VHDL.
 *
 * This is the C++ application, which can be used as a bridge between the VHDL
 * simulation and a Debugger client, using named pipes.
 */

#include "ExampleFpga.h"
#include "ExampleFpga2.h"
#include <stored>

class ExampleFpga : public STORE_T(
			    ExampleFpga, stored::ExampleFpgaDefaultFunctions,
			    stored::Synchronizable, stored::ExampleFpgaBase) {
	STORE_CLASS(
		ExampleFpga, stored::ExampleFpgaDefaultFunctions, stored::Synchronizable,
		stored::ExampleFpgaBase)
public:
	ExampleFpga() is_default
};

class ExampleFpga2 : public STORE_T(
			     ExampleFpga2, stored::ExampleFpga2DefaultFunctions,
			     stored::Synchronizable, stored::ExampleFpga2Base) {
	STORE_CLASS(
		ExampleFpga2, stored::ExampleFpga2DefaultFunctions, stored::Synchronizable,
		stored::ExampleFpga2Base)
public:
	ExampleFpga2() is_default
};

int main()
{
	puts(stored::banner());

	ExampleFpga exampleFpga;
	ExampleFpga2 exampleFpga2;

	stored::Debugger debugger("9_fpga");
	debugger.map(exampleFpga, "/ExampleFpga");
	debugger.map(exampleFpga2, "/ExampleFpga2");

	stored::DebugZmqLayer zmq;
	zmq.wrap(debugger);
	if((errno = zmq.lastError())) {
		perror("Cannot initialize ZMQ");
		return 1;
	}

	stored::Synchronizer synchronizer;
	synchronizer.map(exampleFpga);
	synchronizer.map(exampleFpga2);

	stored::SegmentationLayer segmentation(24);
	synchronizer.connect(segmentation);

	stored::ArqLayer arq;
	arq.wrap(segmentation);

	stored::Crc16Layer crc;
	crc.wrap(arq);

	stored::AsciiEscapeLayer ascii;
	ascii.wrap(crc);

	stored::TerminalLayer term;
	term.wrap(ascii);

#ifdef STORED_OS_WINDOWS
	stored::XsimLayer xsim("9_fpga");
#else
	stored::XsimLayer xsim("/tmp/9_fpga");
#endif

#if 0 // Enable to dump all data to the terminal for debugging.
	stored::PrintLayer print(stdout);
	print.wrap(term);
	xsim.wrap(print);
#else
	xsim.wrap(term);
#endif

	if((errno = xsim.lastError()) && errno != EAGAIN) {
		perror("Cannot initialize XSIM interface");
		return 1;
	}

	printf("\n"
	       "Start XSIM with the 9_fpga example. It connects to this application.\n"
	       "Use a Debugger client to see interaction with the VHDL simulation.\n");

	stored::Poller poller;

	stored::PollableFileLayer xsimp(xsim, stored::Pollable::PollIn);
	stored::PollableFileLayer xsimreqp(xsim.req(), stored::Pollable::PollIn);
	stored::PollableZmqLayer zmqp(zmq, stored::Pollable::PollIn);
	if((errno = poller.add(xsimp)) || (errno = poller.add(xsimreqp))
	   || (errno = poller.add(zmqp))) {
		perror("Cannot initialize poller");
		return 1;
	}

	while(true) {
		// 1 s timeout, to force keep alive once in a while.
		if(poller.poll(1000).empty()) {
			switch(errno) {
			case EAGAIN:
			case EINTR:
				break;
			default:
				perror("poll failed");
				return 1;
			}
		}

		zmq.recv();
		xsim.recv();
		synchronizer.process();

		// Inject a dummy byte to keep xsim alive, as it blocks on a read from file.
		xsim.keepAlive();
	}
}
