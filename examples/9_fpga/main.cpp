#include <stored>
#include "ExampleFpga.h"
#include "ExampleFpga2.h"

#include <conio.h>
#include <unistd.h>

class ExampleFpga : public stored::Synchronizable<stored::ExampleFpgaBase<ExampleFpga>> {
	CLASS_NOCOPY(ExampleFpga)
public:
	typedef stored::Synchronizable<stored::ExampleFpgaBase<ExampleFpga>> base;
	using typename base::Implementation;
	friend class stored::ExampleFpgaBase<ExampleFpga>;
	ExampleFpga() is_default
};

class ExampleFpga2 : public stored::Synchronizable<stored::ExampleFpga2Base<ExampleFpga2>> {
	CLASS_NOCOPY(ExampleFpga2)
public:
	typedef stored::Synchronizable<stored::ExampleFpga2Base<ExampleFpga2>> base;
	using typename base::Implementation;
	friend class stored::ExampleFpga2Base<ExampleFpga2>;
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

