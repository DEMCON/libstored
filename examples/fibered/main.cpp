/*!
 * \file
 * \brief Zth integration example.
 */

#include "ExampleFibered.h"

#include <cstdio>
#include <stored>
#include <zth>

static void sender(stored::ExampleFibered& store)
{
	printf("Started sender\n");

	// Create something to communicate with the receiver fiber.  In this
	// case, use a SyncZmqLayer, but any Win32 SOCKET, file descriptor,
	// ZeroMQ socket, etc. will do.
	stored::SyncZmqLayer layer(nullptr, "inproc://fibered", true);

	// Sleep for a while, to make the receiver block on poll().
	zth::nap(1);

	// Send something.
	printf("Sending...\n");
	zth::cow_string msg = zth::format("From sender fiber: %d", store.i.as<int>());
	layer.encode(msg.c_str(), msg.size());
}
zth_fiber(sender)

static void receiver()
{
	printf("Started receiver\n");

	stored::PrintLayer print;
	stored::SyncZmqLayer layer(nullptr, "inproc://fibered", false);
	layer.wrap(print);

	stored::Poller poller;
	stored::PollableZmqLayer pollable(layer, stored::Pollable::PollIn);

	if((errno = poller.add(pollable))) {
		perror("Cannot add pollable");
		return;
	}

	printf("poll...\n");

	// Without Zth, poll() will block the current thread. With Zth, it will
	// only block the current fiber. All pollables are forwarded to a
	// single fiber that does the actual poll.
	if(poller.poll().empty()) {
		perror("Cannot poll");
		return;
	}

	if((errno = layer.recv())) {
		perror("Cannot recv");
		return;
	}
}
zth_fiber(receiver)

int main_fiber(int UNUSED_PAR(argc), char** UNUSED_PAR(argv))
{
	puts(zth::banner());
	puts(stored::banner());

	// By default, Zth only supports (ZeroMQ) sockets, but libstored has
	// extended this with files, ProtocolLayers, and more. To handle these,
	// we have to register libstored's poller as the poller server.
	stored::PollerServer pollerServer;
	zth::currentWorker().waiter().setPoller(&pollerServer);

	// Now, do something that poll()s.
	stored::ExampleFibered s;

	async sender(s);
	receiver_future f = async receiver();
	f->wait();

	// Reset to default poller, before we destruct pollerServer.
	zth::currentWorker().waiter().setPoller();

	return 0;
}
