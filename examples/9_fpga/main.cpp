#include <stored>
#include "ExampleFpga.h"
#include "ExampleFpga2.h"

#include <conio.h>
#include <unistd.h>



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
	debugger.map(exampleFpga, "ExampleFpga");
	debugger.map(exampleFpga2, "ExampleFpga2");

	stored::DebugZmqLayer zmq;
	zmq.wrap(debugger);

	stored::Synchronizer synchronizer;
	synchronizer.map(exampleFpga);
	synchronizer.map(exampleFpga2);

	stored::AsciiEscapeLayer ascii;
	synchronizer.connect(ascii);
	stored::TerminalLayer term;
	term.wrap(ascii);
//	stored::FileLayer file;
//	file.wrap(term);

	stored::Poller poller;
	poller.add(STDIN_FILENO, nullptr, stored::Poller::PollIn);
	while(true) {
		stored::Poller::Result const* res = poller.poll(2000000);
		if(!res) {
			printf("timeout %d\n", errno);
		} else {
			printf("event %d\n", (int)res->size());
			INPUT_RECORD r[512];
			DWORD read;
			ReadConsoleInput(GetStdHandle(STD_INPUT_HANDLE), r, 512, &read );
		}
	}
}

