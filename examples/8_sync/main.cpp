/*!
 * \file
 * \brief Example with multiple stores to be synced between multiple nodes.
 *
 * You can build any topology you want, but as an example with two parties:
 *
 * - Run first instance: `8_sync -i inst1 -d ipc://libstored/example -p 2222`
 * - Run second instance: `8_sync -i inst2 -u ipc://libstored/example -p 2223`
 * - Run a debugger for first instance: `python3 -m ed2.gui -p 2222`
 * - Run a debugger for second instance: `python3 -m ed2.gui -p 2223`
 * - Enable tracing on all variables. You will notice that when you change a
 *   value within /ExampleSync1, it will be synchronized immediately.
 *   Changes within /ExampleSync2 are only synchronized once you write to
 *   /ExampleSync1/sync ExampleSync2.
 */

#include "ExampleSync1.h"
#include "ExampleSync2.h"

#include <stored>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

static stored::Synchronizer synchronizer;

class ExampleSync2 : public stored::Synchronizable<stored::ExampleSync2Base<ExampleSync2> > {
	CLASS_NOCOPY(ExampleSync2)
public:
	typedef stored::Synchronizable<stored::ExampleSync2Base<ExampleSync2> > base;
	using typename base::Implementation;
	friend class stored::ExampleSync2Base<ExampleSync2>;

	ExampleSync2() is_default
};

static ExampleSync2 store2;

class ExampleSync1 : public stored::Synchronizable<stored::ExampleSync1Base<ExampleSync1> > {
	CLASS_NOCOPY(ExampleSync1)
public:
	typedef stored::Synchronizable<stored::ExampleSync1Base<ExampleSync1> > base;
	using typename base::Implementation;
	friend class stored::ExampleSync1Base<ExampleSync1>;

	ExampleSync1() is_default

protected:
	void __sync_ExampleSync2(bool set, bool& value) {
		if(set) {
			printf("Triggered synchronization of store2\n");
			synchronizer.process(store2);
		} else
			value = false;
	}
};

static ExampleSync1 store1;

int main(int argc, char** argv) {
	stored::Debugger debugger("8_sync");
	debugger.map(store1);
	debugger.map(store2);

	synchronizer.map(store1);
	synchronizer.map(store2);

	std::list<stored::SyncZmqLayer*> connections;
	int debug_port = stored::DebugZmqLayer::DefaultPort;

	int c;
	while((c = getopt(argc, argv, "i:d:u:p:")) != -1)
		switch(c) {
		case 'i': {
			printf("This is %s\n", optarg);
			debugger.setIdentification(optarg);
			break;
		}
		case 'p': {
			debug_port = atoi(optarg);
			break;
		}
		case 'd': {
			printf("Listen at %s for downstream sync\n", optarg);
			stored::SyncZmqLayer* z = new stored::SyncZmqLayer(nullptr, optarg, true);
			connections.push_back(z);
			synchronizer.connect(*z);
			break;
		}
		case 'u': {
			printf("Connect to %s for upstream sync\n", optarg);
			stored::SyncZmqLayer* z = new stored::SyncZmqLayer(nullptr, optarg, false);
			connections.push_back(z);
			synchronizer.connect(*z);
			synchronizer.syncFrom(store1, *z);
			synchronizer.syncFrom(store2, *z);
			break;
		}
		default:
			printf("Usage: %s [-i <name>] [-p <port>] [-d <endpoint>|-u <endpoint>]*\n", argv[0]);
			printf("where\n");
			printf("  -d   Listen for incoming 0MQ endpoint for downstream sync.\n");
			printf("  -i   Set debugger's identification name.\n");
			printf("  -p   Set debugger's port\n");
			printf("  -u   Connect to 0MQ endpoint for upstream sync.\n\n");
			printf("Specify -i and -u as often as required.\n\n");
			return 1;
		}

	stored::DebugZmqLayer debugLayer(nullptr, debug_port);
	debugLayer.wrap(debugger);

	std::vector<zmq_pollitem_t> fds;
	fds.reserve(connections.size() + 1 /* debugger */);

	for(std::list<stored::SyncZmqLayer*>::iterator it = connections.begin(); it != connections.end(); ++it) {
		zmq_pollitem_t fd = {};
		fd.socket = (*it)->socket();
		fd.events = ZMQ_POLLIN;
		fds.push_back(fd);
	}

	{
		zmq_pollitem_t fd = {};
		fd.socket = debugLayer.socket();
		fd.events = ZMQ_POLLIN;
		fds.push_back(fd);
	}

	while(true) {
		// Go sync store1 on all connections.
		synchronizer.process(store1);

		// Wait for input...
		int cnt = zmq_poll(&fds.front(), (int)fds.size(), -1);
		switch(cnt) {
		case -1:
			printf("Poll returned error %d; %s\n", errno, zmq_strerror(errno));
			goto done;
		case 0:
			// Nothing to be done.
			continue;
		default:;
		}

		// Look for connection that has activity.
		size_t i = 0;
		int res = 0;
		for(std::list<stored::SyncZmqLayer*>::iterator it = connections.begin(); it != connections.end() && cnt; ++it, i++) {
			if(fds[i].revents & ZMQ_POLLIN) {
				cnt--;
				if((res = (*it)->recv())) {
					printf("Sync socket recv error %d; %s\n", res, zmq_strerror(res));
					goto done;
				}
			}
		}

		if(cnt) {
			// Must be the debugger socket, which was last in line.
			if(fds[fds.size() - 1].revents & ZMQ_POLLIN) {
				cnt--;
				if((res = debugLayer.recv())) {
					printf("Debugger socket recv error %d; %s\n", res, zmq_strerror(res));
					goto done;
				}
			}
		}

		stored_assert(cnt == 0);
	}

done:
	for(std::list<stored::SyncZmqLayer*>::iterator it = connections.begin(); it != connections.end(); ++it)
		delete *it;

	return 0;
}

