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
  Debugger protocol.
- There are sufficient hooks by the store to implement an application-specific
  synchronization method, other than the Embedded Debugger.
- All code is normal C++, there are no platform-dependent constructs used.
  Therefore, all platforms are (soon) supported (Windows/Linux/Mac/bare
  metal/x86/ARM/gcc/clang).

Have a look in the `examples` directory for further in-depth reading.

## How to build

Build out-of-tree:

	mkdir build
	cd build
	cmake ..
	cmake --build .

By default, all examples are built.  For example, notice that sources are
generated under `examples/1_hello`, while the example itself is built in the
`build` directory.

## Syntax example

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

## Embedded Debugger commands

A store can be queried using the following commands. These can be extended by
an application.

### Capabilities

Request:
	?
Response:
	?LRWAME

### List

	L/
	/b /t

	L/b
	/b/a /b/b[0 /b/b[1

### echo

	Eabc
	abc

### read/write

	R/bla/asdf
	123abc

	R/b/a
	123abc

	W/b/a 10
	!

### alias
	A0/bla/a
	!

	A1/z/z/z
	?

	A1/b/b[0]
	!

	At/t
	!

	Rt10
	234847 0 42

	W010
	!

	A1
	!

### macro

	MsRt10
	!

	s
	23866 0 42

	s
	23970 0 42

## Protocol stack

### Application layer

See above:

	request: <cmd>[<args...>]
	response: <response>|!|?[<error>]

### Presentation layer:
For UART: In case of binary data, escape all bytes < 0x20 as follows: the
sequence DEL (0x7f) removes the 3 MSb of the successive byte. For example, the
sequence 'DEL ;' (0x7f 0x3b) decodes as ESC (0x1b).

### Session layer:
(no session support, only one-to-one link)

### Transport layer
In case of lossless channels (UART/TCP), nothing to add.
In lossly channels, a CRC may be appended, and message id/retransmit.

### Network layer
In multi-core systems, a src/dst header may be appended.

### Datalink layer
In case of lossless UART: out-of-band message are captured using `ESC _` (APC) and `ESC \` (ST).
A message consists of the bytes in between these sequences.

### Physical layer
(not specified, depends on device)

