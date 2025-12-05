// SPDX-FileCopyrightText: 2020-2025 Jochem Rutgers
//
// SPDX-License-Identifier: CC0-1.0

/*!
 * \file
 * \brief Example with synchronization between server and client with a lossy
 *        channel.
 */

#include "ExampleSync.h"

#include <array>
#include <chrono>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <stored>
#include <thread>

#include <getopt_mini.h>

enum {
	// Interval in between polling of the sockets.
	PollInterval_ms = 100,

	// Interval to check if the Synchronizers need to send out updates to the other party.
	SyncInterval_ms = PollInterval_ms * 5,

	// Interval to retransmit unacknowledged packets over the lossy line.
	// Do this more often than SyncInterval_ms to avoid delays.
	RetransmitInterval_ms = PollInterval_ms * 3,

	// Timeout value to send out a keep-alive packet.
	// In this application, this should not be needed, as synchronization is faster.
	IdleTimeout_ms = SyncInterval_ms * 2,

	// Timeout until we give up on the connection.
	DisconnectTimeout_ms = IdleTimeout_ms * 5,

	// Update the heartbeat value in the store.
	HeartbeatInterval_ms = 1000,

	// Delay before trying to reconnect after a disconnection.
	ReconnectDelay_ms = DisconnectTimeout_ms + IdleTimeout_ms * 2,
};


/////////////////////////////////////////////////////////////////////////
// Logging
//

static std::function<void(char const*)> logger_callback;

// flawfinder: ignore
__attribute__((format(printf, 1, 0))) static void logv(char const* format, va_list args)
{
	static std::array<char, 1024> msg;
	// flawfinder: ignore
	vsnprintf(msg.data(), msg.size(), format, args);
	fputs(msg.data(), stderr);

	if(logger_callback)
		logger_callback(msg.data());
}

// flawfinder: ignore
__attribute__((format(printf, 1, 2))) static void log(char const* format, ...)
{
	va_list args;
	va_start(args, format);
	logv(format, args);
	va_end(args);
}



/////////////////////////////////////////////////////////////////////////
// The store
//

class ExampleSync : public STORE_T(ExampleSync, stored::Synchronizable, stored::ExampleSyncBase) {
	STORE_CLASS(ExampleSync, stored::Synchronizable, stored::ExampleSyncBase)
public:
	ExampleSync() is_default

	void __ber(bool set, float& value) noexcept
	{
		auto l = m_lossyLayer.lock();
		if(set)
			l->ber(value);
		else
			value = l ? l->ber() : std::numeric_limits<float>::quiet_NaN();
	}

	void __errors(bool set, uint32_t& value) noexcept
	{
		if(set)
			return;

		auto l = m_lossyLayer.lock();
		value = l ? static_cast<uint32_t>(l->errors()) : 0U;
	}

	void setLossyLayer(std::shared_ptr<stored::LossyLayer>&& layer) noexcept
	{
		m_lossyLayer = std::move(layer);
	}

	void setLossyLayer(std::shared_ptr<stored::LossyLayer> const& layer) noexcept
	{
		m_lossyLayer = layer;
	}

private:
	std::weak_ptr<stored::LossyLayer> m_lossyLayer;
};



/////////////////////////////////////////////////////////////////////////
// Argument parsing and help
//

static void print_help(FILE* out, char const* progname)
{
	fprintf(out,
		"Usage: %s [-h] [-v] [-p <port>] {-s <endpoint>|-c <endpoint>} [-b <BER>] [-e "
		"<key file>]\n",
		progname);
	fprintf(out, "where\n");
	fprintf(out, "  -h   Show this help message.\n");
	fprintf(out, "  -s   Server 0MQ endpoint for downstream sync, such as: tcp://*:5555\n");
	fprintf(out,
		"  -c   Client 0MQ endpoint for upstream sync, such as: tcp://localhost:5555\n");
	fprintf(out, "  -p   Set debugger's port. Default: %d\n",
		stored::DebugZmqLayer::DefaultPort);
	fprintf(out, "  -v   Verbose output of sync connections.\n");
	fprintf(out, "  -b   Bit error rate (BER) for lossy channel. Default: 0\n");
	fprintf(out,
		"  -e   Encrypt communication with the %zu-byte AES-256 key, read from the file.\n",
		(size_t)stored::Aes256Layer::KeySize);
}

struct Arguments {
	bool verbose = false;
	int debug_port = stored::DebugZmqLayer::DefaultPort;
	std::string client;
	std::string server;
	std::string key;
	float ber = 0;
};

class exit_now : public std::exception {};

static Arguments parse_arguments(int argc, char** argv)
{
	Arguments args;

	int c;
	// flawfinder: ignore
	while((c = getopt(argc, argv, "hs:c:p:vb:e:")) != -1) {
		switch(c) {
		case 'p':
			try {
				int port = std::stoi(optarg);
				if(port <= 0 || port >= 0x10000)
					STORED_throw(std::invalid_argument{"Invalid port"});
				args.debug_port = port;
			} catch(std::exception& e) {
				STORED_throw(std::invalid_argument{e.what()});
			}
			break;
		case 'v':
			args.verbose = true;
			printf("Enable verbose output\n");
			break;
		case 's':
			args.server = optarg;
			break;
		case 'c':
			args.client = optarg;
			break;
		case 'b':
			try {
				args.ber = std::stof(optarg);
				if(args.ber < 0.F || args.ber > 1.F)
					STORED_throw(std::invalid_argument{"Invalid BER"});
			} catch(std::invalid_argument&) {
				STORED_rethrow;
			} catch(std::exception& e) {
				STORED_throw(std::invalid_argument{e.what()});
			}
			break;
		case 'e': {
			// flawfinder: ignore
			FILE* f = fopen(optarg, "rb");
			args.key.resize(stored::Aes256Layer::KeySize);

			if(!f) {
				log("Cannot open key file '%s'; %s\n", optarg, strerror(errno));
				STORED_throw(std::invalid_argument{"Cannot open key file"});
			}

			if(fread(&args.key[0], stored::Aes256Layer::KeySize, 1, f) != 1) {
				log("Cannot read key file '%s'; %s\n", optarg, strerror(errno));
				fclose(f);
				STORED_throw(std::invalid_argument{"Cannot read key file"});
			}

			fclose(f);
			log("Read AES-256 key from '%s'\n", optarg);
			break;
		}
		case 'h':
			print_help(stdout, argv[0]);
			STORED_throw(exit_now());
		default:
			print_help(stderr, argv[0]);
			STORED_throw(std::invalid_argument{""});
		}
	}

	if(!args.client.empty() && !args.server.empty()) {
		log("Cannot be both client and server\n");
		STORED_throw(std::invalid_argument{""});
	}

	if(args.client.empty() && args.server.empty()) {
		log("Must be either client or server\n");
		STORED_throw(std::invalid_argument{""});
	}

	return args;
}



/////////////////////////////////////////////////////////////////////////
// The stacks
//

class disconnected : public std::exception {};

/*!
 * \brief ZeroMQ interface for the debugger.
 */
class DebugStack {
	STORED_CLASS_NOCOPY(DebugStack)
public:
	explicit DebugStack(
		ExampleSync& store, int port, char const* name = nullptr, char const* key = nullptr)
		: m_debugLayer{nullptr, port}
	{
		if((errno = m_debugLayer.lastError())) {
			log("Cannot initialize ZMQ for debugging, got error %d; %s\n", errno,
			    zmq_strerror(errno));
			STORED_throw(std::runtime_error{"ZMQ initialization failed"});
		}

		m_id = "lossy_sync";
		if(name)
			m_id += std::string{" ("} + name + ")";
		m_debugger.setIdentification(m_id.c_str());

		m_debugger.map(store);

		if(key) {
			// Encrypted debug channel.
			m_aes.reset(new stored::Aes256Layer{key});
			m_aes->wrap(m_debugger);
			m_debugLayer.wrap(*m_aes);
		} else {
			m_debugLayer.wrap(m_debugger);
		}

		logger_callback = [&](char const* msg) { m_debugger.stream('l', msg); };
	}

	~DebugStack() noexcept
	{
		logger_callback = nullptr;
	}

	stored::Pollable& pollable()
	{
		return m_pollable;
	}

	void process()
	{
		int res = m_debugLayer.recv();

		switch(res) {
		case 0:
		case EAGAIN:
			return;
		default:
			log("Debugger recv failed with error %d; %s\n", res, zmq_strerror(res));
			break;
		}
	}

private:
	std::string m_id;
	stored::Debugger m_debugger;
	stored::DebugZmqLayer m_debugLayer;
	std::unique_ptr<stored::Aes256Layer> m_aes;
	stored::PollableZmqSocket m_pollable{m_debugLayer.socket(), stored::Pollable::PollIn};
};

/*!
 * \brief Lossy synchronization stack.
 */
class SyncStack {
	STORED_CLASS_NOCOPY(SyncStack)
public:
	explicit SyncStack(
		ExampleSync& store, char const* endpoint, bool server, bool verbose = false,
		float ber = 0, char const* key = nullptr)
		: m_zmqLayer(nullptr, endpoint, server)
	{
		if((errno = m_zmqLayer.lastError())) {
			log("Cannot initialize ZMQ for sync, got error %d; %s\n", errno,
			    zmq_strerror(errno));
			STORED_throw(std::runtime_error{"ZMQ initialization failed"});
		}

		int linger = 0;
		if(zmq_setsockopt(m_zmqLayer.socket(), ZMQ_LINGER, &linger, sizeof(linger)) == -1) {
			log("Cannot set ZMQ_LINGER, got error %d; %s\n", errno,
			    zmq_strerror(errno));
			STORED_throw(std::runtime_error{"ZMQ setsockopt failed"});
		}

		if(verbose)
			wrap<stored::PrintLayer>(stdout, "sync");

		// Add another channel to send pings.
		auto mux = wrap<stored::MuxLayer>();
		auto ch1_print = alloc<stored::PrintLayer>(stdout, "chan");
		m_ch1 = alloc<stored::ProtocolLayer>();
		m_ch1->wrap(*ch1_print);
		mux.get()->map(1, *m_ch1);

		if(key)
			// Encrypt communication.
			wrap<stored::Aes256Layer>(key);

		// We don't want to do ARQ on large messages, so we segment them to some
		// appropriate size.
		wrap<stored::SegmentationLayer>(32U);
		// Perform retransmits. Limit the encode queue to have some bound on the maximum
		// RTT.
		m_arq = wrap<stored::ArqLayer>(1024U);
		m_arq->setEventCallback(
			[](stored::ArqLayer&, stored::ArqLayer::Event event, void* arg) {
				static_cast<SyncStack*>(arg)->event(event);
			},
			this);
		// Check if we have communication at all.
		m_idle = wrap<stored::IdleCheckLayer>();

		if(verbose)
			wrap<stored::PrintLayer>(stdout, "arq");

		// Do CRC checks. Do this below the ARQ, such that the ARQ sees no or
		// correct messages.
		wrap<stored::Crc32Layer>();
		// Do escaping to allow framing by the terminal layer.
		wrap<stored::AsciiEscapeLayer>();
		// Framing.
		wrap<stored::TerminalLayer>();
		if(server)
			// The server simulates a lossy channel.
			store.setLossyLayer(wrap<stored::LossyLayer>(ber));

		// Optional: buffer partial messages to reduce the number of sends/receives on the
		// wire.
		wrap<stored::BufferLayer>(64U);

		if(verbose)
			wrap<stored::PrintLayer>(stdout, "raw");

		// Connect to I/O.
		m_zmqLayer.wrap(*m_stack.back());

		// Register the store...
		m_synchronizer.map(store);
		// ...and the protocol stack.
		m_synchronizer.connect(**m_stack.begin());

		// There we go!
		auto now = std::chrono::steady_clock::now();
		m_idleUpSince = now;
		m_idleDownSince = now;
		m_lastRetransmit = now;
		m_lastSync = now;
		m_lastHeartbeat = now;
		m_heartbeat = server ? store.server_heartbeat.variable()
				     : store.client_heartbeat.variable();

		if(!server) {
			m_synchronizer.syncFrom(store, *m_stack.front());
			m_connected = true;
			m_arq->keepAlive();
		}
	}

	stored::Pollable& pollable()
	{
		return m_pollable;
	}

	void process()
	{
		auto now = std::chrono::steady_clock::now();

		recv();
		doSync(now);
		checkRetransmit(now);
		checkIdle(now);
		checkDisconnect(now);
		doHeartbeat(now);

		m_idle->setIdle();
	}

	bool connected() const
	{
		return m_connected;
	}

protected:
	template <typename T, typename... Args>
	std::shared_ptr<T> alloc(Args&&... args)
	{
		auto* p = new T{std::forward<Args>(args)...};
		std::shared_ptr<T> layer{p};
		m_layers.emplace_back(layer);
		return layer;
	}

	template <typename T, typename... Args>
	std::shared_ptr<T> wrap(Args&&... args)
	{
		auto* p = new T{std::forward<Args>(args)...};
		std::shared_ptr<T> layer{p};

		if(!m_stack.empty())
			layer->wrap(*m_stack.back());

		m_stack.emplace_back(layer);
		return layer;
	}

	void recv()
	{
		// Process incoming messages.
		int res = m_zmqLayer.recv();

		switch(res) {
		case 0:
		case EAGAIN:
			return;
		default:
			log("Sync recv failed with error %d; %s\n", res, zmq_strerror(res));
			break;
		}
	}

	void doSync(std::chrono::time_point<std::chrono::steady_clock> const& now)
	{
		if(now - m_lastSync >= std::chrono::milliseconds(SyncInterval_ms)) {
			m_lastSync = now;
			m_synchronizer.process();
		}
	}

	void checkRetransmit(std::chrono::time_point<std::chrono::steady_clock> const& now)
	{
		// Check if we need to retransmit messages that have not been acked yet.

		if(!connected())
			return;

		if(m_idle->idleDown()) {
			auto dt = now - m_lastRetransmit;
			if(dt > std::chrono::milliseconds(RetransmitInterval_ms)) {
				m_arq->process();
				m_lastRetransmit = now;
			}
		} else {
			m_lastRetransmit = now;
		}
	}

	void checkIdle(std::chrono::time_point<std::chrono::steady_clock> const& now)
	{
		// Check if we need to send out a keep-alive message once in a while.

		if(!connected())
			return;

		if(m_idle->idleDown()) {
			auto dt = now - m_idleDownSince;
			if(dt > std::chrono::milliseconds(IdleTimeout_ms)) {
				m_arq->keepAlive();
				m_idleDownSince = now;
			}
		} else {
			m_idleDownSince = now;
		}
	}

	void checkDisconnect(std::chrono::time_point<std::chrono::steady_clock> const& now)
	{
		if(connected()) {
			if(m_idle->idleUp()) {
				auto dt = now - m_idleUpSince;
				if(dt > std::chrono::milliseconds(DisconnectTimeout_ms)) {
					log("No upstream activity, disconnecting\n");
					STORED_throw(disconnected{});
				}
			} else {
				m_idleUpSince = now;
			}
		} else if(!m_idle->idleUp()) {
			log("Upstream activity detected, connected\n");
			m_connected = true;
			m_idleUpSince = now;
		}
	}

	void doHeartbeat(std::chrono::time_point<std::chrono::steady_clock> const& now)
	{
		auto dt = now - m_lastHeartbeat;
		if(dt >= std::chrono::milliseconds(HeartbeatInterval_ms)) {
			m_lastHeartbeat = now;
			auto h = m_heartbeat++;

			if(connected()) {
				// flawfinder: ignore
				char buf[32];
				snprintf(buf, sizeof(buf), "ping %u", h);
				m_ch1->encode(buf, strlen(buf), true);
			}
		}
	}

	void event(stored::ArqLayer::Event event)
	{
		switch(event) {
		case stored::ArqLayer::EventEncodeBufferOverflow:
			log("ARQ encode buffer overflow\n");
			STORED_throw(disconnected{});
		case stored::ArqLayer::EventReconnect:
			log("ARQ reconnect event\n");
			break;
		case stored::ArqLayer::EventRetransmit:
			log("ARQ retransmit limit exceeded, ignored\n");
			break;
		default:
			break;
		}
	}

private:
	stored::Synchronizer m_synchronizer;
	std::shared_ptr<stored::ArqLayer> m_arq;
	std::shared_ptr<stored::IdleCheckLayer> m_idle;
	std::shared_ptr<stored::ProtocolLayer> m_ch1;
	std::list<std::shared_ptr<stored::ProtocolLayer>> m_layers;
	std::list<std::shared_ptr<stored::ProtocolLayer>> m_stack;
	stored::ZmqLayer m_zmqLayer;
	stored::PollableZmqSocket m_pollable{m_zmqLayer.socket(), stored::Pollable::PollIn};
	std::chrono::time_point<std::chrono::steady_clock> m_idleUpSince;
	std::chrono::time_point<std::chrono::steady_clock> m_idleDownSince;
	std::chrono::time_point<std::chrono::steady_clock> m_lastRetransmit;
	std::chrono::time_point<std::chrono::steady_clock> m_lastSync;
	std::chrono::time_point<std::chrono::steady_clock> m_lastHeartbeat;
	stored::Variable<uint32_t, ExampleSync> m_heartbeat;
	bool m_connected = false;
};



/////////////////////////////////////////////////////////////////////////
// Main function
//

static void run(Arguments const& args, ExampleSync& store, DebugStack& debugStack)
{
	std::unique_ptr<SyncStack> syncStack;

	if(!args.client.empty()) {
		syncStack.reset(new SyncStack{
			store, args.client.c_str(), false, args.verbose, 0,
			args.key.empty() ? nullptr : args.key.c_str()});
	} else if(!args.server.empty()) {
		syncStack.reset(new SyncStack{
			store, args.server.c_str(), true, args.verbose, args.ber,
			args.key.empty() ? nullptr : args.key.c_str()});
	}

	stored::Poller poller;
	if((errno = poller.add(debugStack.pollable()))) {
		perror("Cannot add pollable");
		STORED_throw(std::runtime_error{"Poller add failed"});
	}

	if((errno = poller.add(syncStack->pollable()))) {
		perror("Cannot add pollable");
		STORED_throw(std::runtime_error{"Poller add failed"});
	}

	try {
		while(true) {
			poller.poll(PollInterval_ms);
			debugStack.process();
			syncStack->process();
		}
	} catch(disconnected&) {
		poller.remove(syncStack->pollable());
		syncStack.reset();

		log("Disconnected, lingering...\n");

		auto now = std::chrono::steady_clock::now();
		auto end = now + std::chrono::milliseconds(ReconnectDelay_ms);

		while(now < end) {
			poller.poll(PollInterval_ms);
			debugStack.process();
			now = std::chrono::steady_clock::now();
		}

		STORED_rethrow;
	}
}

int main(int argc, char** argv)
{
#ifdef STORED_OS_WINDOWS
	setvbuf(stdout, nullptr, _IONBF, 0);
#else
	setvbuf(stdout, nullptr, _IOLBF, 0);
#endif
	// flawfinder: ignore
	srand((unsigned int)time(nullptr));

	try {
		Arguments args = parse_arguments(argc, argv);

		ExampleSync store;
		DebugStack debugStack{
			store, args.debug_port, args.client.empty() ? "server" : "client",
			args.key.empty() ? nullptr : args.key.c_str()};

		while(true) {
			try {
				run(args, store, debugStack);
			} catch(disconnected&) {
				log("Restarting...\n");
				store.restarted++;
			}
		}
	} catch(exit_now&) {
		return 0;
	} catch(std::invalid_argument&) {
		return 1;
	} catch(std::exception& e) {
#ifdef STORED_cpp_exceptions
		log("Error: %s\n", e.what());
#endif
		return 2;
	} catch(...) {
		log("Unknown error\n");
		return 3;
	}
}
