#include <stored>
#include "ZmqServerStore.h"

int main() {
	stored::ZmqServerStore store;
	stored::Debugger debugger("zmqserver");
	debugger.map(store);

	stored::ZmqLayer zmqLayer;
	zmqLayer.wrap(debugger);

	while(true)
		zmqLayer.recv(true);
}

