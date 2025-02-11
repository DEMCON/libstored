

..
   SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
   
   SPDX-License-Identifier: CC-BY-4.0

Protocol
========

Protocol layers, to be wrapped around a :cpp:class:`stored::Debugger` or
:cpp:class:`stored::Synchronizer` instance.

Every embedded device is different, so the required protocol layers are too.
What is common, is the Application layer, but as the Transport and Physical
layer are often different, the layers in between are often different too.  To
provide a common Embedded Debugger interface, the client (e.g., GUI, CLI,
python scripts), we standardize on ZeroMQ REQ/REP over TCP.

Not every device supports ZeroMQ, or even TCP. For this, several bridges are
required. Different configurations may be possible:

- In case of a Linux/Windows application: embed ZeroMQ server into the
  application, such that the application binds to a REP socket.  A client can
  connect to the application directly.
- Terminal application with only stdin/stdout: use escape sequences in the
  stdin/stdout stream. ``python/libstored.wrapper.stdio`` is provided to
  inject/extract these messages from those streams and prove a ZeroMQ
  interface.
- Application over CAN: like a ``python/libstored.wrapper.stdio``, a CAN
  extractor to ZeroMQ bridge is required.

Then, the client_ can be connected to the ZeroMQ interface.

Test it using the ``terminal`` example, started using the
``python/libstored.wrapper.stdio``. Then connect one of the clients to it.

libstored suggests to use the protocol layers below, where applicable.
Standard layer implementations can be used to construct the following stacks (top-down):

- Lossless UART:
  :cpp:class:`stored::Debugger`,
  :cpp:class:`stored::AsciiEscapeLayer`,
  :cpp:class:`stored::TerminalLayer`,
  :cpp:class:`stored::StdioLayer`
- Lossy UART:
  :cpp:class:`stored::Debugger`,
  :cpp:class:`stored::DebugArqLayer`,
  :cpp:class:`stored::Crc16Layer`,
  :cpp:class:`stored::AsciiEscapeLayer`,
  :cpp:class:`stored::TerminalLayer`,
  :cpp:class:`stored::StdioLayer`
- CAN:
  :cpp:class:`stored::Debugger`,
  :cpp:class:`stored::SegmentationLayer`,
  :cpp:class:`stored::DebugArqLayer`,
  :cpp:class:`stored::BufferLayer`,
  CAN driver
- ZMQ:
  :cpp:class:`stored::Debugger`,
  :cpp:class:`stored::DebugZmqLayer`
- VHDL simulation:
  :cpp:class:`stored::Synchronizer`,
  :cpp:class:`stored::AsciiEscapeLayer`,
  :cpp:class:`stored::TerminalLayer`,
  :cpp:class:`stored::XsimLayer`

Protocol layers make an onion shape either around a debugger or a synchronizer.
Following the example of the lossless UART:

.. uml::

   left to right direction

   component "StdioLayer" {
      () "Decode" as decode3
      () "Encode" as encode3

      component "TerminalLayer" {
         () "Decode" as decode2
         () "Encode" as encode2

         component "AsciiEscapeLayer" {
            () "Decode" as decode1
            () "Encode" as encode1

            component "Synchronizer / Debugger" as core {
            }
         }
      }
   }

   [In] --> decode3
   decode3 --> decode2
   decode2 --> decode1
   decode1 --> core

   core --> encode1
   encode1 --> encode2
   encode2 --> encode3
   encode3 --> [Out]

If you have to implement you own protocol layer, start with
:cpp:class:`stored::ProtocolLayer`. Especially, override
:cpp:func:`stored::ProtocolLayer::encode()` for messages passed down the stack
towards the hardware, and :cpp:func:`stored::ProtocolLayer::decode()` for
messages from the hardware up.

.. _client: py.html

The inheritance of the layers is shown below.

.. uml::

   abstract ProtocolLayer
   ProtocolLayer <|-- AsciiEscapeLayer
   ProtocolLayer <|-- TerminalLayer
   AsciiEscapeLayer -[hidden]--> TerminalLayer
   ProtocolLayer <|-- SegmentationLayer
   ProtocolLayer <|-- Crc8Layer
   ProtocolLayer <|-- Crc16Layer
   Crc8Layer -[hidden]--> Crc16Layer
   ProtocolLayer <|-- BufferLayer
   ProtocolLayer <|-- PrintLayer
   ProtocolLayer <|-- IdleCheckLayer
   ProtocolLayer <|-- CallbackLayer

   abstract ArqLayer
   SegmentationLayer -[hidden]--> ArqLayer
   ProtocolLayer <|-- ArqLayer
   ArqLayer <|-- DebugArqLayer

   abstract PolledLayer
   abstract PolledFileLayer
   abstract PolledSocketLayer
   ProtocolLayer <|-- PolledLayer
   PolledLayer <|-- PolledFileLayer
   PolledFileLayer <|-- FileLayer
   FileLayer <|-- NamedPipeLayer
   PolledFileLayer <|-- DoublePipeLayer
   DoublePipeLayer <|-- XsimLayer
   XsimLayer --> NamedPipeLayer
   PolledLayer <|-- PolledSocketLayer : Windows
   PolledFileLayer <|-- PolledSocketLayer : POSIX
   PolledFileLayer <|-- StdioLayer : Windows
   FileLayer <|-- StdioLayer : POSIX
   ProtocolLayer <|-- CompressLayer
   PolledLayer <|-- FifoLoopback1
   FileLayer <|-- SerialLayer

   ProtocolLayer <|-- Stream
   Debugger --> Stream
   Stream --> CompressLayer
   ProtocolLayer <|-- Debugger
   ProtocolLayer <|-- SyncConnection

   abstract ZmqLayer
   PolledSocketLayer <|-- ZmqLayer
   ZmqLayer <|-- DebugZmqLayer
   ZmqLayer <|-- SyncZmqLayer

   class Loopback
   FifoLoopback --> FifoLoopback1


stored::AsciiEscapeLayer
------------------------

.. doxygenclass:: stored::AsciiEscapeLayer

stored::BufferLayer
-------------------

.. doxygenclass:: stored::BufferLayer

stored::CallbackLayer
---------------------

.. doxygenclass:: stored::CallbackLayer

stored::CompressLayer
---------------------

.. doxygenclass:: stored::CompressLayer

stored::Crc16Layer
------------------

.. doxygenclass:: stored::Crc16Layer

stored::Crc8Layer
-----------------

.. doxygenclass:: stored::Crc8Layer

stored::DebugArqLayer
---------------------

.. doxygenclass:: stored::DebugArqLayer

stored::DebugZmqLayer
---------------------

.. doxygenclass:: stored::DebugZmqLayer

stored::DoublePipeLayer
-----------------------

.. doxygenclass:: stored::DoublePipeLayer

stored::FifoLoopback
---------------------

.. doxygenclass:: stored::FifoLoopback

stored::FifoLoopback1
---------------------

.. doxygenclass:: stored::FifoLoopback1

stored::FileLayer
-----------------

.. doxygenclass:: stored::FileLayer

stored::IdleCheckLayer
----------------------

.. doxygenclass:: stored::IdleCheckLayer

stored::Loopback
----------------

.. doxygenclass:: stored::Loopback

stored::NamedPipeLayer
----------------------

.. doxygenclass:: stored::NamedPipeLayer

stored::PrintLayer
------------------

.. doxygenclass:: stored::PrintLayer

stored::SegmentationLayer
-------------------------

.. doxygenclass:: stored::SegmentationLayer

stored::SerialLayer
-------------------------

.. doxygenclass:: stored::SerialLayer

stored::StdioLayer
------------------

.. doxygenclass:: stored::StdioLayer

stored::SyncZmqLayer
--------------------

.. doxygenclass:: stored::SyncZmqLayer

stored::TerminalLayer
---------------------

.. doxygenclass:: stored::TerminalLayer

stored::XsimLayer
-----------------------

.. doxygenclass:: stored::XsimLayer



Abstract classes
----------------

stored::ArqLayer
````````````````

.. doxygenclass:: stored::ArqLayer


stored::PolledFileLayer
```````````````````````

.. doxygenclass:: stored::PolledFileLayer

stored::PolledLayer
```````````````````

.. doxygenclass:: stored::PolledLayer

stored::ProtocolLayer
`````````````````````

.. doxygenclass:: stored::ProtocolLayer

