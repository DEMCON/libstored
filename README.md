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

Run `scripts/bootstrap` once to install all build dependencies.
Then run `scripts/build` to build the project. This does effectively:

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

Requests always start with a ASCII characters are command.
Every request gets a response. Either with actual data, or with ack `!` or nack `?`.
Requests are processed in order.

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


## License

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

