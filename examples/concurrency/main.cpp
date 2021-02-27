/*!
 * \file
 * \brief Example with concurrency and message passing for synchronization.
 *
 * Although threads are undesirable, as it makes your application to a large
 * extend unpredictable, concurrency may not be avoidable some times. This
 * example shows how you can synchronize stores between threads, such that
 * threads do not share memory (except for the communication channels, but that
 * is handled by libstored).
 *
 * This pattern discussed in this example resembles a microcontroller, which
 * runs the main application, while this application is interrupted by a timer,
 * such that a real-time control loop can be executed. So, the main application
 * is not real-time and may consume all CPU cycles left over from the control
 * loop. In this case, the control loop runs concurrently to the application,
 * with all threading issues you can imagine. libstored gives you
 * message-passing channels, such that the control loop (interrupt handler) and
 * the main application has their own instance of the same store, which is
 * synchronized.
 *
 * Visualized, the setup of the application is as follows:
 *
 *
 * main()                                          interrupt handler
 * - background tasks                              - control loop
 *    |            |                                     |
 *    |            |                                     |
 * Main store     Control store                     Control store
 *    |              |   |                               |
 *    +--------------+   |                               |
 *    |                  |                               |
 * Debugger         Synchronizer                    Synchronizer
 *                       |                               |
 *                       |                               |
 *                       +--------- FifoLoopback --------+
 *
 *
 * So, the main application exposes its instances of the stores to the
 * Debugger.  The main()'s Control store is synchronized with the interrupt
 * handler's instance. The FifoLoopback is a thread-safe bidirectional
 * ProtocolLayer, with bounded FIFO memory. No dynamic allocation is done after
 * initialization. The FIFO is lock-free. However, you have to specify what
 * happens when it gets full (drop data, suspend for a while, etc.)
 *
 * For demo purposes, the interrupt handler is in this example implemented as
 * an std::thread.
 */

#include "ExampleConcurrencyMain.h"
#include "ExampleConcurrencyControl.h"

#include <stored>
#include <thread>
#include <chrono>
#include <cstdio>
#include <cerrno>
#include <string>

// Make the Control store synchronizable.
class ControlStore : public STORE_SYNC_BASE_CLASS(ExampleConcurrencyControlBase, ControlStore) {
	STORE_SYNC_CLASS_BODY(ExampleConcurrencyControlBase, ControlStore)
public:
	ControlStore() = default;
};

// Use a bounded memory for the FifoLoopback channels. This is set at four
// times the maximum message a Synchronizer may send (which is usually only
// during initial setup when the full buffer is transmitted). You have to think
// about what is appropriate for your application.
using FifoLoopback = stored::FifoLoopback<ControlStore::MaxMessageSize * 4>;

// This is the 'interrupt handler'. In this example an std::thread.
static void control(ControlStore& controlStore, stored::Synchronizer& synchronizer, FifoLoopback& loopback)
{
	// Specify the handler to be called when a Synchronizer message that is
	// pushed into the FifoLoopback does not fit anymore. In this case, we just
	// yield and wait a while.  You may also decide to abort the application,
	// if you determined that it should not happen in your case.
	loopback.b2a().setOverflowHandler([&](){
		while(loopback.b2a().full())
			std::this_thread::yield();

		return true;
	});

	while(controlStore.run) {
		// Sleep for a while (or wait for an 'interrupt').
		std::this_thread::sleep_for(std::chrono::seconds(1));

		// The Synchronizer may push at most one message back into the
		// FifoLoopback channel when receiving one. Therefore, only try to
		// decode a message when we know that it will not block.
		while(loopback.b2a().space() >= ControlStore::MaxMessageSize)
			if(loopback.a2b().recv())
				break;

		// This 'control loop' allows you to override the actual value.
		// Otherwise, it steps towards the setpoint.
		if(controlStore.override_obj >= 0)
			controlStore.actual = (uint32_t)controlStore.override_obj;
		else if(controlStore.actual < controlStore.setpoint)
			controlStore.actual = controlStore.actual + 1;
		else if(controlStore.actual > controlStore.setpoint)
			controlStore.actual = controlStore.actual - 1;

		// Only send updates when we know it will fit in the FifoLoopback.
		if(loopback.b2a().space() >= ControlStore::MaxMessageSize)
			synchronizer.process();
	}

	// Send a bye message and terminate the connection.
	synchronizer.disconnect(loopback.b());
}

int main(int argc, char** argv)
{
	// Before starting the control thread, initialize all components.  This
	// will use the heap, but that is OK, as we are not in the 'interrupt
	// handler'.  After initialization, it is safe to use the store and
	// Synchronizer instances.
	stored::ExampleConcurrencyMain mainStore;
	ControlStore controlStore;
	ControlStore controlStoreOther;

	// Create the debugger for both stores.
	stored::Debugger debug("concurrency");
	debug.map(mainStore);
	debug.map(controlStore);

	// Create a ZeroMQ connection for the debugger.
	stored::DebugZmqLayer zmq;
	zmq.wrap(debug);

	// This is the Synchronizer for this thread.
	stored::Synchronizer synchronizer;
	synchronizer.map(controlStore);

	// This is the Synchronizer for the other thread.
	stored::Synchronizer synchronizerOther;
	synchronizerOther.map(controlStoreOther);

	// The thread-safe message-passing channel between both Synchronizers.
	FifoLoopback loopback;

	// In case the FIFO gets full, this thread just stalls...
	loopback.a2b().setOverflowHandler([&](){
		// ...but there should not be a deadlock.
		assert(!loopback.a2b().full() || !loopback.b2a().full());

		while(loopback.a2b().full())
			std::this_thread::yield();

		return true;
	});

	// Connect the loopback channel.
	synchronizer.connect(loopback.a());
	synchronizerOther.connect(loopback.b());
	// Specify that the other thread will use the channel as the source of its
	// store instance.
	synchronizerOther.syncFrom(controlStoreOther, loopback.b());

	// We need a poller to check for ZeroMQ (debugger) messages.
	stored::Poller poller;
	if(poller.add(zmq, nullptr, stored::Poller::PollIn)) {
		perror("Cannot register zmq to poller");
		return 1;
	}

	// When actual value changes, it is printed to the console.
	auto prevActual = controlStore.actual.get();
	prevActual++; // Force to be different.

	// For demo purposes, you can specify the setpoint as a command line
	// argument.  If set, the application quits when the actual reaches the
	// setpoint.
	bool demo = false;
	if(argc >= 2) {
		try {
			controlStore.setpoint = (uint32_t)std::stoul(argv[1], nullptr, 0);
			demo = true;
			printf("Enabled demo mode with setpoint = %u\n", (unsigned)controlStore.setpoint);
		} catch(...) {
		}
	}

	// Ready to start the control thread.
	std::thread controller(control, std::ref(controlStoreOther), std::ref(synchronizerOther), std::ref(loopback));

	// Main loop.
	while(!demo || controlStore.run) {
		// Check for ZeroMQ input.
		auto const& res = poller.poll(100000L);
		if(res.empty()) {
			if(errno != EAGAIN)
				perror("Cannot poll");
		} else {
			if(zmq.recv())
				perror("Cannot recv");
		}

		// Check for Synchronizer messages from the other thread.
		loopback.b2a().recvAll();

		if(prevActual != controlStore.actual)
			printf("actual = %u\n", (unsigned)(prevActual = controlStore.actual));

		if(demo && prevActual == controlStore.setpoint)
			// Done, terminate.
			controlStore.run = false;

		// Push updates in our Control store to the other thread.
		synchronizer.process();
	}

	controller.join();
	return 0;
}

