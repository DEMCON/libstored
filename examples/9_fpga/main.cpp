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

class ExampleFpga : public STORE_SYNC_BASE_CLASS(ExampleFpgaBase, ExampleFpga) {
	STORE_SYNC_CLASS_BODY(ExampleFpgaBase, ExampleFpga)
public:
	ExampleFpga() is_default
};

class ExampleFpga2 : public STORE_SYNC_BASE_CLASS(ExampleFpga2Base, ExampleFpga2) {
	STORE_SYNC_CLASS_BODY(ExampleFpga2Base, ExampleFpga2)
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

	stored::AsciiEscapeLayer ascii;
	synchronizer.connect(ascii);
	stored::TerminalLayer term;
	term.wrap(ascii);
	stored::XsimLayer xsim("9_fpga");

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
		if(poller.poll(1000000).empty()) {
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
