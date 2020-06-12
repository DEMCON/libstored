# libstored - Store for Embedded Debugger

## How to build

Build out-of-tree:

	mkdir build
	cd build
	cmake ..
	cmake --build .

By default, all examples are built.
For example, notice that sources are generated under `examples/1_hello`,
while the example itself is built in the `build` directory.

## Syntax

	uint32_t /t;
	int32_t /bla/asdf = 42;
	bool /bla/b[0] = 1;
	bool /bla/b[1];

	uint32_t t;
	{
		int32_t asdf;
		bool[2] b;
		string[16] s;
	} bla;

## Embedded Debugger commands

### help

	?
	LRWAME

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

