[![CI](https://github.com/DEMCON/libstored/workflows/CI/badge.svg)](https://github.com/DEMCON/libstored/actions?query=workflow%3ACI)

# libstored - Store for Embedded Debugger

If you have an embedded system, you probably want to debug it on-target.  One
of the questions you often have, is what is the value of internal variables of
the program, and how can I change them.  Debugging using `gdb` is great, but it
pauses the application, which also stops control loops, for example.

libstored helps you to access internal variables. These variables are part of
a _store_. A store is defined in a simple language, and libstored provides a
generator to produce a C++ class (as cmake library) with the variables you
want.  This generated C++ class is very efficient when accessed by the C++
application. However, it adds interesting functionality: all variables can be
accessed remotely.

The (OSI-stack) Application layer of this debugging interface is provided by
libstored.  You have to add all other (usually hardware-specific) layers of
the OSI stack to get the debugging protocol in and out of your system.
Although the protocol fits nicely to ZeroMQ, a TCP stream, or `stdio` via
terminal, the complexity of integrating this depends on your embedded device.
However, once you implemented this data transport, you can access the store,
and observe and manipulate it using an Embedded Debugger (PC) client, where
libstored provides Python classes, a CLI and GUI interface.

Your application can have one store with one debugging interface, but also
multiple stores with one debugging interface, or one store with multiple
debugging interfaces -- any combination is possible.

The store has a few other interesting properties:

- Objects can have a piece of memory as backing (just like a normal variable in
  a `struct`), but can also have custom callbacks on every get and set. This
  allows all kinds of side effects, even though the interface of the object is
  the same.
- Objects are accessible using a C++ interface, but also via name lookup by
  string. The generator generates a compact name parser, such that names,
  types, and sizes can be queried dynamically.
- The Embedded Debugger has a standard set of commands, which can be disabled
  for your specific project.  Moreover, you can also easily add custom commands
  by adding another capability in a subclass of stored::Debugger.
- There are sufficient hooks by the store to implement any application-specific
  synchronization method, other than the Embedded Debugger.
- The store and all other libstored classes are not thread-safe.
  Using threads is troubling anyway, use [fibers](https://github.com/jhrutgers/zth) instead.
- All code is normal C++, there are no platform-dependent constructs used.
  Therefore, all platforms are supported: Windows/Linux/Mac/bare
  metal (newlib), x86/ARM, gcc/clang/MSVC/armcc).

Have a look in the `examples` directory for further in-depth reading.
Refer to the [Doxygen documentation](https://demcon.github.io/libstored) for the C++ API.

## Table of contents

- [How to build](#build)
	- [How to integrate in your build](#integrate)
- [Syntax example](#syntax)
- [Debugging example](#debugging)
- [Embedded Debugger protocol](#protocol)
- [License](#license)

## <a name="build"></a>How to build

Run `scripts/bootstrap` (as Administrator under Windows) once to install all
build dependencies.  Then run `scripts/build` to build the project. This does
effectively:

	mkdir build
	cd build
	cmake ..
	cmake --build .

`scripts/build` takes an optional argument, which allows you to specify the
`CMAKE_BUILD_TYPE`.  If not specified, Debug is assumed.

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

See the [examples](https://demcon.github.io/libstored/examples.html) directory
for more explanation. This is just an impression of the syntax.

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

The generated store has variables that can be accessed like this:

	mystore.some_int = 10;
	int i = mystore.another_int_which_is_initialized;
	mystore.time_s.get();
	mystore.scope__b = false;
	mystore.scope__numbers_0.set(0.1);
	mystore.scope__numbers_1.set(1.1);
	mystore.scope__s.set("hello");

## <a name="debugging"></a>Debugging example

The host tools to debug your application are written in python, as the `ed2`
package, and are located the `client` directory. You can run the example below
by running python from the `client` directory, but you can also install the
`ed2` package on your system. To do this, execute the `ed2-install` cmake
target, such as:

	cd build
	make ed2-install

This builds a wheel from the `client` directory and installs it locally using
`pip`.  Now you can just fire up python and do `import ed2`.

To get a grasp how debugging feels like, try the following.

- Build the examples, as discussed above.
- If you use Windows, execute `scripts/env.cmd` to set your environment
  properly.  In the instructions below, use `python` instead of `python3`.
- Run your favorite `lognplot` instance, e.g., by running `python3 -m lognplot`.
- Run `examples/zmqserver/zmqserver`. This starts an application with a store
  with all kinds of object types, and provides a ZeroMQ server interface for
  debugging.
- Run `python3 -m ed2.gui -l` within the `client` directory. This GUI connects
  to both the `zmqserver` application via ZeroMQ, and to the `lognplot` instance.
- The GUI window will pop up and show the objects of the `zmqserver` example.
  If polling is enabled of one of the objects, the values are forwarded to
  `lognplot`.

The structure of this setup is:

	+---------+        +----------+
	| ed2.gui | -----> | lognplot |
	+---------+        +----------+
	      |
	      | ZeroMQ REQ/REP channel
	      |
	+-----------+
	| zmqserver |
	+-----------+

![zmqserver debugging screenshot](examples/zmqserver/zmqserver_screenshot.png)

The Embedded Debugger client connects via ZeroMQ.
If you application does not have it, you must implement is somehow.
The `examples/terminal/terminal` application could be debugged as follows:

- Run `python3 -m ed2.wrapper.stdio ../build/examples/terminal/terminal` from
  the `client` directory.  This starts the `terminal` example, and extracts
  escaped debugger frames from `stdout`, which are forwarded to a ZeroMQ
  interface.
- Connect a client, such as `python3 -m ed2.gui`.

The structure of this setup is:

	+---------+
	| ed2.gui |                    terminal interface
	+---------+                            |
	      |                                |
	      | ZeroMQ REQ/REP channel         |
	      |                                |
	+-------------------+                  |
	| ed2.wrapper.stdio | -----------------+
	+-------------------+
	      |
	      | stdin/stdout (mixed terminal interface
	      | with Embedded Debugger messages)
	      |
	+----------+
	| terminal |
	+----------+

There are some more ready-to-use clients, and a Python module in the
[client](https://github.com/DEMCON/libstored/tree/master/client) directory.

## <a name="protocol"></a>Embedded Debugger protocol

Communication with the debugger implementation in the application follows a
request-response pattern.  A full description of the commands can be found in
the [doxygen documentation](https://demcon.github.io/libstored/group__libstored__debugger.html).
These commands are implemented in the stored::Debugger class and ready to be
used in your application.

However, the request/response messages should be wrapped in a OSI-like protocol stack, which is described in more detail in the
[documentation too](https://demcon.github.io/libstored/group__libstored__protocol.html)).
This stack depends on your application. A few standard protocol layers are
available.

To get a grasp about the protocol, I had a short chat with the `zmqserver`
example using the `ed2.cli`.  See the transcript below. Lines starting
with `>` are requests, entered by me, lines starting with `<` are responses
from the application.

In the example below, I used the following commands:

- `?`: request capabilities of the target
- `l`: list object in the store
- `i`: return the identification of the target
- `r`: read an object
- `w`: write an object
- `v`: request versions
- `a`: define an alias

Refer to the documentation for the details about these and other commands.

	>  ?
	<  ?rwelamivRWst
	>  l
	<  0110/a blob
	201/a bool
	2b4/a float
	2f8/a double
	02f/a string
	312/a uint16
	334/a uint32
	301/a uint8
	378/a uint64
	234/a ptr32
	278/a ptr64
	392/an int16
	3b4/an int32
	381/an int8
	3f8/an int64
	7b4/compute/an int8 + an int16
	734/compute/length of /a string
	6f8/compute/circle area (r = /a double)
	734/stats/ZMQ messages
	734/stats/object writes
	778/t (us)
	6f8/rand
	 
	>  i
	<  zmqserver
	>  r/a bool
	<  0
	>  w1/a bool
	<  !
	>  r/a bool
	<  1
	>  r/s/Z
	<  14
	>  r/s/Z
	<  15
	>  r/rand
	<  3d26000000000000
	>  r/rand
	<  3f50250b79ae8000
	>  r/rand
	<  3fa550a89cb27a00
	>  v
	<  2
	>  ar/rand
	<  !
	>  rr
	<  3fc69c39e2668200
	>  rr
	<  3fd755a4ab38afc0
	>  rr
	<  3fb7617168255e00

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

