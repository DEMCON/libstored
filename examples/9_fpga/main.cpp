#include <stored>
#include "ExampleFpga.h"
#include "ExampleFpga2.h"

class ExampleFpga : public STORE_SYNC_BASECLASS(ExampleFpgaBase, ExampleFpga) {
	STORE_SYNC_CLASS_BODY(ExampleFpgaBase, ExampleFpga)
public:
	ExampleFpga() is_default
};

class ExampleFpga2 : public STORE_SYNC_BASECLASS(ExampleFpga2Base, ExampleFpga2) {
	STORE_SYNC_CLASS_BODY(ExampleFpga2Base, ExampleFpga2)
public:
	ExampleFpga2() is_default
};

int main() {
	puts(stored::banner());

	ExampleFpga exampleFpga;
	ExampleFpga2 exampleFpga2;

	stored::Debugger debugger("9_fpga");
	debugger.map(exampleFpga, "/ExampleFpga");
	debugger.map(exampleFpga2, "/ExampleFpga2");

	stored::DebugZmqLayer zmq;
	zmq.wrap(debugger);

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

	stored::Poller poller;
	poller.add(xsim, nullptr, stored::Poller::PollIn);
	poller.add(xsim.req(), nullptr, stored::Poller::PollIn);
	poller.add(zmq, nullptr, stored::Poller::PollIn);

	while(true) {
		poller.poll(1000000); // 1 s
		zmq.recv();
		xsim.recv();

		// Inject a dummy byte to keep xsim alive, as it blocks on a read from file.
		xsim.keepAlive();
	}
}

