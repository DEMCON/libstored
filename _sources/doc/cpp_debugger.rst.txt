

..
   SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
   
   SPDX-License-Identifier: CC-BY-4.0

Debugger
========

Embedded Debugger message handling.

Protocol
--------

The default set of commands that is processed by :cpp:class:`stored::Debugger`
is listed below.  A subclass of :cpp:class:`stored::Debugger` may extend the
set of capabilities, for application-specific purposes.

The protocol is a request-response mechanism; for every request, there must be
a response.  Requests are processed in order.  This is the OSI application
layer of the protocol stack.  For other layers, see `Protocol`_.

Requests always start with an ASCII character, which is the command.  The
response can be actual data, or with ack ``!`` or nack ``?``.  All requests and
responses are usually plain ASCII (or UTF-8 for strings), to simplify
processing it over a terminal or by humans. However, it does not have to be the
case.

Capabilities
````````````

Request: ``?``

::

   ?

Response: a list of command characters.

::

   ?rwe

This command is mandatory for every debugging target.

Echo
````

Request: ``e`` <any data>

::

   eHello World

Response: <the same data>

::

   Hello World

Read
````

Request: ``r`` <name of object>

(Every scope within) the name of the object may be abbreviated, as long as it
is unambiguous.  In case there is a alias created for the object (see Alias
below), the alias character can be used instead of the name.

::

   r/bla/asdf

Response: ``?`` | <ASCII hex value of object>

For values with fixed length (int, float), the byte order is big/network
endian.  For ints, the initial zeros can be omitted. For other data, all bytes
are encoded.

::

   123abc

Write
`````

Request: ``w`` <value in ASCII hex> <name of object>

See Read for details about the hex value and object name.

::

   w10/b/a

Response: ``!`` | ``?``

::

   !

List
````

Requests a full list of all objects of all registered stores to the current
Embedded Debugger.

Request: ``l``

::

   l

Response: ( <type byte in hex> <length in hex> <name of object> ``\n`` ) * | ``?``

::

   3b4/b/i8
   201/b/b

See :cpp:class:`stored::Type` for the type byte. The type byte is always two hex
characters.  The number of characters of the length of the object depends on
the value; leading zeros may be removed.

Alias
`````

Assigns a character to a object path.  An alias can be everywhere where an
object path is expected.  Creating aliases skips parsing the object path
repeatedly, so makes debugging more efficient.  If no object is specified, the
alias is removed.  The number of aliases may be limited. If the limit is hit,
the response will be ``?``.  The alias name can be any char in the range 0x20
(``␣``) - 0x7e (``~``), except for 0x2f (``/``).

Request: ``a`` <char> ( <name of object> ) ?

::

   a0/bla/a

Response: ``!`` | ``?``

::

   !

Macro
`````

Saves a sequence of commands and assigns a name to it.  The macro name can be
any char in the range 0x20 (``␣``) - 0x7e (``~``).  In case of a name clash
with an existing non-macro command, the command is executed; the macro cannot
hide or replace the command.

The separator can be any char, as long as it is not used within a command of
the macro definition. Using ``\r``, ``\n``, or ``\t`` is usually safe, as it
cannot occur inside a name.

Without the definition after the macro name, the macro is removed. The system
may be limited in total definition length. The macro string is reinterpreted
every time it is invoked.

The responses of the commands are merged into one response frame, without
separators. The Echo command can be used to inject separators in the output.

Request: ``m`` <char> ( <separator> <command> ) *

::

   mZ r/bla/a e; r/bla/z

Response: ``!`` | ``?``

::

   !

If the ``Z`` command is now executed, the result could be something like:

::

   123;456

Identification
``````````````

Returns a fixed string that identifies the application.

Request: ``i``

::

   i

Response: ``?`` | <UTF-8 encoded application name>

::

   libstored

Version
```````

Returns a list of versions.

Request: ``v``

::

   v

Response: ``?`` | <protocol version> ( ``␣`` <application-specific version> ) *

::

   2 r243+trunk beta

Read memory
```````````

Read a memory via a pointer instead of the store.  Returns the number of
requested bytes. If no length is specified, a word is returned.

Request: ``R`` <pointer in hex> ( ``␣`` <length> ) ?

::

   R1ffefff7cc 4

Response: ``?`` | <bytes in hex>

::

   efbe0000

Bytes are just concatenated as they occur in memory, having the byte at the
lowest address first.

Write memory
````````````

Write a memory via a pointer instead of the store.

Request: ``W`` <pointer in hex> ``␣`` <bytes in hex>

::

   W1ffefff7cc 0123

Response: ``?`` | ``!``

::

   !

Streams
```````

Read all available data from a stream. Streams are application-defined
sequences of bytes, like stdout and stderr. They may contain binary data.
There are an arbitrary number of streams, with an arbitrary single-char name,
except for ``?``, as it makes the response ambiguous.

To list all streams with data:

Request: ``s``

To request all data from a stream, where the optional suffix is appended to the
response:

Request: ``s`` <char> <suffix> ?

::

   sA/

Response: ``?`` | <data> <suffix>

::

   Hello World!!1/

Once data has been read from the stream, it is removed. The next call will
return new data.  If a stream was never used, ``?`` is returned. If it was
used, but it is empty now, the stream char does not show up in the ``s`` call,
but does respond with the suffix. If no suffix was provided, and there is no
data, the response is empty.

The number of streams and the maximum buffer size of a stream may be limited.

Depending on :cpp:var:`stored::Config::CompressStreams`, the data returned by ``s``
is compressed using heatshrink (window=8, lookahead=4). Every chunk of data
returned by ``s`` is part of a single stream, and must be decompressed as
such. As (de)compression is stateful, all data from the start of the stream is
required for decompression. Moreover, stream data may remain in the compressor
before retrievable via ``s``.

To detect if the stream is compressed, and to forcibly flush out and reset
the compressed stream, use the Flush (``f``) command. A flush will terminate
the current stream, push out the last bit of data from the compressor's buffers
and restart the compressor's state. So, a normal startup sequence of a
debug client would be:

- Check if ``f`` capability exists. If not, done; no compression is used on streams.
- Flush out all streams: execute ``f``.
- Drop all streams, as the start of the stream is possibly missing: execute ``sx``
  for every stream returned by ``s``.

Afterwards, pass all data received from ``s`` through the heatshrink decoder.

Flush
`````

Flush out and reset a stream (see also Streams). When this capability does not
exist, streams are not compressed. If it does exist, all streams are compressed.
Use this function to initialize the stream if the (de)compressing state is unknown,
or to force out the last data (for example, the last trace data).

Request: ``f`` <char> ?

Response: ``!``

The optional char is the stream name. If omitted, all streams are flushed and reset.
The response is always ``!``, regardless of whether the stream existed or had data.

The stream is blocked until it is read out by ``s``. This way, the last data
is not lost, but new data could be dropped if this takes too long. If you want
an atomic flush-retrieve, use a macro.

Tracing
```````

Executes a macro every time the application invokes :cpp:func:`stored::Debugger::trace()`.
A stream is filled with the macro output.

Request: ``t`` ( <macro> <stream> ( <decimate in hex> ) ? ) ?

::

   tms64

This executes macro output of ``m`` to the stream ``s``, but only one in every
100 calls to trace().  If the output does not fit in the stream buffer, it is
silently dropped.

``t`` without arguments disables tracing. If the decimate argument is omitted,
1 is assumed (no decimate).  Only one tracing configuration is supported;
another ``t`` command with arguments overwrites the previous configuration.

Response: ``?`` | ``!``

::

   !

The buffer collects samples over time, which is read out by the client possibly
at irregular intervals.  Therefore, you probably want to know the time stamp of
the sample. For this, include reading the time in the macro definition. By
convention, the time is a top-level variable ``t`` with the unit between braces.
It is implementation-defined what the offset is of ``t``, which can be since the
epoch or since the last boot, for example.  For example, your store can have
one of the following time variables:

::

   // Nice resolution, wraps around after 500 millennia.
   (uint64) t (us)
   // Typical ARM systick counter, wraps around after 49 days.
   (uint32) t (ms)
   // Pythonic time. Watch out with significant bits.
   (double) t (s)

The time is usually a function type, as it is read-only and reading it should
invoke some time-keeping functions.

Macro executions are just concatenated in the stream buffer.
Make sure to use the Echo command to inject proper separators to allow parsing
the stream content afterwards.

For example, the following requests are typical to setup tracing:

::

   # Define aliases to speed up macro processing.
   at/t (us)
   a1/some variable
   a2/some other variable
   # Save the initial start time offset to relate it to our wall clock.
   rt
   # Define a macro for tracing
   mM rt e, r1 e, r2 e;
   # Setup tracing
   tMT
   # Repeatedly read trace buffer
   sT
   sT
   sT
   ...

Now, the returned stream buffer (after decompressing) contains triplets of
time, /some variable, /some other variable, like this:

::

   101,1,2;102,1,2;103,1,2;

Depending on the buffer size, reading the buffer may be orders of magnitude slower
than the actual tracing speed.


stored::Debugger
----------------

.. doxygenclass:: stored::Debugger

stored::DebugStoreBase
----------------------

.. doxygenclass:: stored::DebugStoreBase

stored::DebugVariant
--------------------

.. doxygenclass:: stored::DebugVariant

.. _Protocol: cpp_protocol.html

