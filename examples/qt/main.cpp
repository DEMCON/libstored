/*!
 * \file
 * \brief Qt integration example.
 */

#include "ExampleQtStore.h"

#include <stored>

int main()
{
	puts(stored::banner());

	stored::QExampleQtStore store;
	stored::Debugger debugger("qt");
	debugger.map(store);

	stored::DebugZmqLayer zmqLayer;
	zmqLayer.wrap(debugger);

	printf("Connect via ZMQ to debug this application.\n");

	stored::Poller poller;
	stored::PollableZmqLayer pollableZmq(zmqLayer, stored::Pollable::PollIn);

	if((errno = poller.add(pollableZmq))) {
		perror("Cannot add to poller");
		exit(1);
	}

	while(true) {
		if(poller.poll().empty()) {
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
