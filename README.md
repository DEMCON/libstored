# libstored - Store for Embedded Debugger

This is a generator for a store. A store is a collection of objects, like
variables in a `struct`. These objects can be read and written, using common
get/set methods. However, the store has a few interesting properties:

- Objects are defined by a simple language. The generator produces an
  application-specific store implementation, and all files needed for your
  specific project.
- Objects can have a piece of memory as backing (just like a normal variable in
  a `struct`), but can also have custom callbacks on every get and set. This
  allows all kinds of side effects, even though the interface of the object is
  the same.
- Objects are accessible using a C++ interface, but also via name lookup by
  string. The generator generates a compact name parser, such that names,
  types, and sizes can be queried dynamically.
- The Embedded Debugger can be attached to a store, which exposes the full
  store to any external interface. The protocol's application layer is
  implemented and ready to use. As an application developer, you only have to
  implement the hardware-specifics, like the transport layer of the Embedded
  Debugger protocol. You can also easily extend the default set of Embedded
  Debugger commands by adding another capability in a subclass of
  stored::Debugger.
- There are sufficient hooks by the store to implement an application-specific
  synchronization method, other than the Embedded Debugger.
- All code is normal C++, there are no platform-dependent constructs used.
  Therefore, all platforms are supported: Windows/Linux/Mac/bare
  metal (newlib), x86/ARM, gcc/clang/MSVC).

Have a look in the `examples` directory for further in-depth reading.

## Table of contents

- [How to build](#build)
	- [How to integrate in your build](#integrate)
- [Syntax example](#syntax)
- [Debugging example](#debugging)
- [Embedded Debugger commands](#commands)
- [Protocol stack](#protocol)
- [License](#license)

## <a name="build"></a>How to build

Make sure to update the submodules after checkout:

	git submodule init
	git submodule update

Run `scripts/bootstrap` once to install all build dependencies.
Then run `scripts/build` to build the project. This does effectively:

	mkdir build
	cd build
	cmake ..
	cmake --build .

By default, all examples are built.  For example, notice that sources are
generated under `examples/1_hello`, while the example itself is built in the
`build` directory. The documentation can be viewed at
`doxygen/html/index.html`.

To run all tests:

	cmake --build . -- test

### <a name="integrate"></a>How to integrate in your build

Building libstored on itself is not too interesting, it is about how it can
generate stuff for you.  This is how to integrate it in your project:

- Add libstored to your source repository, for example as a submodule.
- Run `scripts/bootstrap` in the libstored directory once to install all
  dependencies.
- Include libstored to your cmake project. For example:

		set(LIBSTORED_EXAMPLES OFF CACHE BOOL "Disable libstored examples" FORCE)
		set(LIBSTORED_TESTS OFF CACHE BOOL "Disable libstored tests" FORCE)
		set(LIBSTORED_DOCUMENTATION OFF CACHE BOOL "Disable libstored documentation" FORCE)
		add_subdirectory(libstored)

- Optional: install `scripts/st.vim` in `$HOME/.vim/syntax` to have proper
  syntax highlighting in vim.
- Add some store definition file to your project, let's say `MyStore.st`.
  Assume you have a target `app` (which can be any type of cmake target), which
  is going to use `MyStore.st`, generate all required files. This will generate
  the sources in the `libstored` subdirectory of the current source directory,
  a library named `app-libstored`, and set all the dependencies right.

		add_application(app main.cpp)
		libstored_generate(app FumoStore.st)

- Now, build your `app`. The generated libstored library is automatically
  built.

Check out the examples of libstored, which are all independent applications
with their own generated store.

## <a name="syntax"></a>Syntax example

See `examples` for more explanation. This is just an impression of the syntax.

	// Comment
	// Grammar: type:size[array]=initializer long name with any character

	uint32 some int
	int8=42 another int, which is initialized
	(uint64) time (s)

	{
		bool=true b
		double[2] numbers
		string:16 s
	} scope

## <a name="debugging"></a>Debugging example

To get a grasp how debugging feels like, try the following.

- Build the examples, as discussed above.
- If you use Windows, execute `scripts/env.cmd` to set your environment
  properly.  In the instructions below, use `python` instead of `python3`.
- Run your favorite `lognplot` instance, e.g., by running `python3 -m lognplot`.
- Run `examples/zmqserver/zmqserver`. This starts an application with a store
  with all kinds of object types, and provides a ZMQ server interface for
  debugging.
- Run `python3 client/gui_client.py -l`. This GUI connects to both the
  `zmqserver` application via ZMQ, and to the `lognplot` instance.
- The GUI window will pop up and show the objects of the `zmqserver` example.
  If polling is enabled of one of the objects, the values are forwarded to
  `lognplot`.

## <a name="commands"></a>Embedded Debugger commands

A store can be queried using the following commands. These can be extended by
an application.

Requests always start with a ASCII characters are command.
Every request gets a response. Either with actual data, or with ack `!` or nack `?`.
Requests are processed in order.

This is the OSI application layer of the protocol stack.

### Capabilities

Request: `?`

	?

Response: a list of command characters.

	?rwe

### Echo

Request: `e` \<any data\>

	eHello World

Response: \<the same data\>

	Hello World

### Read

Request: `r` \<name of object\>

The name of the object may be abbreviated, as long as it is unambiguous.

	r/bla/asdf

Response: \<ASCII hex value of object\>

For values with fixed length (int, float), the byte order is big/network
endian.  For ints, the initial zeros can be omitted. For other data, all bytes
are encoded.

	123abc

### Write 

Request: `w` \<value in ASCII hex\> \<name of object\>

See Read for details about the hex value and object name.

	w10/b/a

Response: `!` | `?`

	!

### List

Requests a full list of all objects of all registered stores to the current Embedded Debugger.

Request: `l`

	l

Response: ( \<type byte in hex\> \<length in hex\> \<name of object\> `\n` ) * | `?`

	3b4/b/i8
	201/b/b

### Alias

Assigns a character to a object path.  An alias can be everywhere where an
object path is expected.  Creating aliases skips parsing the object path
repeatedly, so makes debugging more efficient.  If no object is specified, the
alias is removed.  The number of aliases may be limited. If the limit is hit,
the response will be `?`.
The alias name can be any char in the range 0x20 ` ` - 0x7e `~`, except for 0x2f `/`.

Request: `a` \<char\> ( \<name of object\> ) ?

	a0/bla/a

Response: `!` | `?`

	!

### Macro

Saves a sequence of commands and assigns a name to it.  The macro name can be
any char in the range 0x20 ` ` - 0x7e `~`.  In case of a name clash with an
existing non-macro command, the command is executed; the macro cannot hide or
replace the command.  The separator can be any char, as long as it is not used
within a command of the macro definition. Without the definition after the
macro name, the macro is removed. The system may be limited in total definition
length. The macro string is reinterpreted every time it is invoked. The
responses of the commands are merged into one response frame, without
separators.

Request: `m` \<char\> ( \<separator\> \<command\> ) *

	mZ r/bla/a e; r/bla/z

Response: `!` | `?`

	!

### Identification

Returns a fixed string that identifies the application.

Request: `i`

	i

Response: `?` | \<UTF-8 encoded application name\>

	libstored

### Version

Returns a list of versions.

Request: `v`

	v

Response: `?` | \<protocol version\> ( ` ` \<application-specific version\> ) *

	2 r243+trunk beta

### Read memory

Read a memory via a pointer instead of the store.  Returns the number of
requested bytes. If no length is specified, a word is returned.

Request: `R` \<pointer in hex\> ( ` ` \<length\> ) ?

	R1ffefff7cc 4

Response: `?` | \<bytes in hex\>

	efbe0000

### Write memory

Write a memory via a pointer instead of the store.

Request: `W` \<pointer in hex\> ` ` \<bytes in hex\>

	W1ffefff7cc 0123

Response: `?` | `!`

	!

### Streams

Read all available data from a stream. Streams are application-defined
sequences of bytes, like stdout and stderr. They may contain binary data.
There are an arbitrary number of streams, with an arbitrary single-char name.

To list all streams with data:

Request: `s`

To request all data from a stream, where the optional suffix is appended to the response:

Request: `s` \<char\> \<suffix\> ?

	sA/

Response: `?` | \<data\> \<suffix\>

	Hello World!!1/

Once data has been read from the stream, it is removed. The next call will
return new data.  If a stream was never used, `?` is returned. If it was used,
but it is empty now, the stream char does not show up in the `s` call, but does
respond with the suffix. If no suffix was provided, and there is no data, the
response is empty.

The number of streams and the maximum buffer size of a stream may be limited.



## <a name="protocol"></a>Protocol stack

Every embedded device is different, so the required protocol layers are too.
What is common, is the Application layer, but as the Transport and Physical
layer are often different, the layers in between are often different too.
To provide a common Embedded Debugger interface, the client (e.g., GUI, CLI,
python scripts), we standardize on ZeroMQ REQ/REP over TCP.

Not every device supports ZeroMQ, or even TCP. For this, several bridges are
required. Different configurations may be possible:

- In case of a Linux/Windows application: embed ZeroMQ server into the
  application, such that the application binds to a REP socket.  A client can
  connect to the application directly.
- Terminal application with only stdin/stdout: use escape sequences in the
  stdin/stdout stream. `client/stdio_wrapper.py` is provided to inject/extract
  these messages from those streams and prove a ZeroMQ interface.
- Application over CAN: like a `client/stdio_wrapper.py`, a CAN extractor to
  ZeroMQ bridge is required.

Then, the client can be connected to the ZeroMQ interface. The following
clients are provided:

- `client/ed2.ZmqClient`: a python class that allows easy access to all objects
  of the connected store. This is the basis of the clients below.
- `client/cli_client.py`: a command line tool that lets you directly enter the
  protocol messages as defined above.
- `client/gui_client.py`: a simple GUI that shows the list of objects and lets
  you manipulate the values. The GUI has support to send samples to `lognplot`.

Test it using the `terminal` example, started using the
`client/stdio_wrapper.py`. Then connect one of the clients above to it.

### Application layer

See above. See stored::Debugger.

### Presentation layer

For terminal or UART: In case of binary data, escape all bytes < 0x20 as
follows: the sequence `DEL` (0x7f) removes the 3 MSb of the successive byte.
For example, the sequence `DEL ;` (0x7f 0x3b) decodes as `ESC` (0x1b).
To encode `DEL` itself, repeat it.
See stored::AsciiEscapeLayer.

For CAN/ZeroMQ: nothing required.

### Session layer:

For terminal/UART/CAN: no session support, there is only one (implicit) session.

For ZeroMQ: use REQ/REP sockets, where the application-layer request and
response are exactly one ZeroMQ message. All layers below are managed by ZeroMQ.

### Transport layer

For terminal or UART: out-of-band message are captured using `ESC _` (APC) and
`ESC \` (ST).  A message consists of the bytes in between these sequences.
See stored::TerminalLayer.

In case of lossly channels (UART/CAN), CRC, message sequence number, and
retransmits should be implemented.  This depends on the specific transport
hardware and embedded device.

### Network layer

For terminal/UART/ZeroMQ, nothing has to be done.

For CAN: packet fragmentation/reassembling/routing is done here.

### Datalink layer

Depends on the device.

### Physical layer

Depends on the device.


## <a name="license"></a>License

The project license is specified in COPYING and COPYING.LESSER.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.

