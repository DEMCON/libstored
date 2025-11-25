// SPDX-FileCopyrightText: 2020-20255555 Jochem Rutgers
//
// SPDX-License-Identifier: CC0-1.0

/*!
 * \file
 * \brief Example with synchronization between server and client with a lossy
 *        channel.
 */

#include "ExampleSync.h"

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <stored>
#include <thread>

#include <getopt_mini.h>

/////////////////////////////////////////////////////////////////////////
// The store
//

class ExampleSync : public STORE_T(ExampleSync, stored::Synchronizable, stored::ExampleSyncBase) {
	STORE_CLASS(ExampleSync, stored::Synchronizable, stored::ExampleSyncBase)
public:
	ExampleSync() is_default
};



/////////////////////////////////////////////////////////////////////////
// Argument parsing and help
//

static int parse_port(char const* str)
{
	char* endptr = nullptr;
	long port = strtol(str, &endptr, 0);
	if(*endptr || port <= 0 || port >= 0x10000)
		throw std::invalid_argument{"Invalid port"};
	return (int)port;
}

static void print_help(FILE* out, char const* progname)
{
	fprintf(out, "Usage: %s [-h] [-v] [-p <port>] {-s <endpoint>|-c <endpoint>}\n", progname);
	fprintf(out, "where\n");
	fprintf(out, "  -h   Show this help message.\n");
	fprintf(out, "  -s   Server 0MQ endpoint for downstream sync.\n");
	fprintf(out, "  -c   Client 0MQ endpoint for upstream sync.\n");
	fprintf(out, "  -p   Set debugger's port. Default: %d\n",
		stored::DebugZmqLayer::DefaultPort);
	fprintf(out, "  -v   Verbose output of sync connections.\n");
}

struct Arguments {
	bool verbose = false;
	int debug_port = stored::DebugZmqLayer::DefaultPort;
	int client_port = 0;
	int server_port = 0;
};

class exit_now : public std::exception {};

static Arguments parse_arguments(int argc, char** argv)
{
	Arguments args;

	int c;
	// flawfinder: ignore
	while((c = getopt(argc, argv, "hs:c:p:v")) != -1) {
		switch(c) {
		case 'p':
			try {
				args.debug_port = parse_port(optarg);
			} catch(std::invalid_argument&) {
				fprintf(stderr, "Invalid debug port '%s'\n", optarg);
				throw;
			}
			break;
		case 'v':
			args.verbose = true;
			printf("Enable verbose output\n");
			break;
		case 's':
			try {
				args.server_port = parse_port(optarg);
			} catch(std::invalid_argument&) {
				fprintf(stderr, "Invalid server port '%s'\n", optarg);
				throw;
			}
			break;
		case 'c':
			try {
				args.client_port = parse_port(optarg);
			} catch(std::invalid_argument&) {
				fprintf(stderr, "Invalid client port '%s'\n", optarg);
				throw;
			}
			break;
		case 'h':
			print_help(stdout, argv[0]);
			throw exit_now();
		default:
			print_help(stderr, argv[0]);
			throw std::invalid_argument{""};
		}
	}

	if(args.client_port && args.server_port) {
		fprintf(stderr, "Cannot be both client and server\n");
		throw std::invalid_argument{""};
	}

	if(!args.client_port && !args.server_port) {
		fprintf(stderr, "Must be either client or server\n");
		throw std::invalid_argument{""};
	}

	return args;
}



/////////////////////////////////////////////////////////////////////////
// The stacks
//

/*!
 * \brief ZeroMQ interface for the debugger.
 */
class DebugStack {
	STORED_CLASS_NOCOPY(DebugStack)
public:
	explicit DebugStack(ExampleSync& store, int port)
		: m_debugger{"lossy_sync"}
		, m_debugLayer{nullptr, port}
	{
		if((errno = m_debugLayer.lastError())) {
			fprintf(stderr, "Cannot initialize ZMQ for debugging, got error %d; %s\n",
				errno, zmq_strerror(errno));
			throw std::runtime_error{"ZMQ initialization failed"};
		}

		m_debugger.map(store);
		m_debugLayer.wrap(m_debugger);
	}

	stored::Pollable& pollable()
	{
		return m_pollable;
	}

	void recv()
	{
		int res = m_debugLayer.recv();

		switch(res) {
		case 0:
		case EAGAIN:
			return;
		default:
			fprintf(stderr, "Debugger recv failed with error %d; %s\n", res,
				zmq_strerror(res));
			break;
		}
	}

private:
	stored::Debugger m_debugger;
	stored::DebugZmqLayer m_debugLayer;
	stored::PollableZmqSocket m_pollable{m_debugLayer.socket(), stored::Pollable::PollIn};
};

/*!
 * \brief Lossy synchronization stack.
 */
class SyncStack {
	STORED_CLASS_NOCOPY(SyncStack)
public:
	explicit SyncStack(
		ExampleSync& store, char const* endpoint, bool server, bool verbose, float ber = 0)
		: m_syncLayer(nullptr, endpoint, server)
	{
		if((errno = m_syncLayer.lastError())) {
			fprintf(stderr, "Cannot initialize ZMQ for sync, got error %d; %s\n", errno,
				zmq_strerror(errno));
			throw std::runtime_error{"ZMQ initialization failed"};
		}

		// We don't want to do ARQ on large messages, so we segment them to some
		// appropriate size.
		wrap<stored::SegmentationLayer>(32U);
		// Perform retransmits.
		m_arq = &wrap<stored::ArqLayer>();
		// Check if we have communication at all.
		m_idle = &wrap<stored::IdleCheckLayer>();
		// Do CRC checks. Do this below the ARQ, such that the ARQ sees no or
		// correct messages.
		wrap<stored::Crc32Layer>();
		// Do escaping to allow framing by the terminal layer.
		wrap<stored::AsciiEscapeLayer>();
		// Framing.
		wrap<stored::TerminalLayer>();
		if(ber > 0)
			// The server simulates a lossy channel.
			wrap<stored::LossyLayer>(ber);
		// Verbose output.
		if(verbose)
			wrap<stored::PrintLayer>();

		// Connect to I/O.
		m_syncLayer.wrap(*m_layers.back());

		// Register the store...
		m_synchronizer.map(store);
		// ...and the protocol stack.
		m_synchronizer.connect(**m_layers.begin());

		// There we go!
		if(!server)
			m_synchronizer.syncFrom(store, m_syncLayer);
	}

	stored::Pollable& pollable()
	{
		return m_pollable;
	}

	void recv()
	{
		int res = m_syncLayer.recv();

		switch(res) {
		case 0:
		case EAGAIN:
			return;
		default:
			fprintf(stderr, "Sync recv failed with error %d; %s\n", res,
				zmq_strerror(res));
			break;
		}
	}

protected:
	template <typename T, typename... Args>
	T& wrap(Args&&... args)
	{
		auto* p = new T{std::forward<Args>(args)...};
		std::unique_ptr<stored::ProtocolLayer> layer{p};

		if(!m_layers.empty())
			layer->wrap(*m_layers.back());

		m_layers.emplace_back(std::move(layer));
		return *p;
	}

private:
	stored::Synchronizer m_synchronizer;
	stored::ArqLayer* m_arq = nullptr;
	stored::IdleCheckLayer* m_idle = nullptr;
	std::list<std::unique_ptr<stored::ProtocolLayer>> m_layers;
	stored::SyncZmqLayer m_syncLayer;
	stored::PollableZmqSocket m_pollable{m_syncLayer.socket(), stored::Pollable::PollIn};
};



/////////////////////////////////////////////////////////////////////////
// Main function
//

class disconnected : public std::exception {};

static void run(Arguments const& args, ExampleSync& store, DebugStack& debugStack)
{
	std::unique_ptr<SyncStack> syncStack;

	if(args.client_port) {
		char endpoint[32]{};
		int res = snprintf(
			endpoint, sizeof(endpoint), "tcp://localhost:%d", args.client_port);
		if(res < 0 || (size_t)res >= sizeof(endpoint))
			throw std::runtime_error{"Endpoint string too long"};

		syncStack.reset(new SyncStack{store, endpoint, false, args.verbose});
	} else if(args.server_port) {
		char endpoint[32]{};
		int res = snprintf(
			endpoint, sizeof(endpoint), "tcp://localhost:%d", args.server_port);
		if(res < 0 || (size_t)res >= sizeof(endpoint))
			throw std::runtime_error{"Endpoint string too long"};

		syncStack.reset(new SyncStack{store, endpoint, true, args.verbose});
	}

	stored::Poller poller;
	if((errno = poller.add(debugStack.pollable()))) {
		perror("Cannot add pollable");
		throw std::runtime_error{"Poller add failed"};
	}

	if((errno = poller.add(syncStack->pollable()))) {
		perror("Cannot add pollable");
		throw std::runtime_error{"Poller add failed"};
	}

	while(true) {
		poller.poll(200);
		debugStack.recv();
		syncStack->recv();
	}
}

int main(int argc, char** argv)
{
	try {
		Arguments args = parse_arguments(argc, argv);

		ExampleSync store;
		DebugStack debugStack{store, args.debug_port};

		while(true) {
			try {
				run(args, store, debugStack);
			} catch(disconnected&) {
				fprintf(stderr, "Disconnected, restarting...\n");
				std::this_thread::sleep_for(std::chrono::seconds(1));
				++store.restarted;
			}
		}
	} catch(exit_now&) {
		return 0;
	} catch(std::invalid_argument&) {
		return 1;
	} catch(std::exception& e) {
		fprintf(stderr, "Error: %s\n", e.what());
		return 2;
	} catch(...) {
		fprintf(stderr, "Unknown error\n");
		return 3;
	}
}
