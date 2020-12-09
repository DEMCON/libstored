Protocol
========

Protocol layers, to be wrapped around a stored::Debugger or
stored::Synchronizer instance.

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
  stdin/stdout stream. ``client/ed2.wrapper.stdio`` is provided to inject/extract
  these messages from those streams and prove a ZeroMQ interface.
- Application over CAN: like a ``client/ed2.wrapper.stdio``, a CAN extractor to
  ZeroMQ bridge is required.

Then, the client_ can be connected to the ZeroMQ interface.

Test it using the ``terminal`` example, started using the
``client/ed2.wrapper.stdio``. Then connect one of the clients to it.

libstored suggests to use the protocol layers below, where applicable.
Standard layer implementations can be used to construct the following stacks (top-down):

- Lossless UART: stored::Debugger, `stored::AsciiEscapeLayer`_, `stored::TerminalLayer`_, `stored::StdioLayer`_
- Lossy UART: stored::Debugger, `stored::DebugArqLayer`_, `stored::Crc16Layer`_, `stored::AsciiEscapeLayer`_, `stored::TerminalLayer`_, `stored::StdioLayer`_
- CAN: stored::Debugger, `stored::SegmentationLayer`_, `stored::DebugArqLayer`_, `stored::BufferLayer`_, CAN driver
- ZMQ: stored::Debugger, `stored::DebugZmqLayer`_
- VHDL simulation: stored::Synchronizer, `stored::AsciiEscapeLayer`_, `stored::TerminalLayer`_, `stored::NamedPipeLayer`_

.. _client: py.html


stored::ArqLayer
----------------

.. doxygenclass:: stored::ArqLayer

stored::AsciiEscapeLayer
------------------------

.. doxygenclass:: stored::AsciiEscapeLayer

stored::BufferLayer
-------------------

.. doxygenclass:: stored::BufferLayer

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

stored::FileLayer
-----------------

.. doxygenclass:: stored::FileLayer

stored::Loopback
----------------

.. doxygenclass:: stored::Loopback

stored::NamedPipeLayer
----------------------

.. doxygenclass:: stored::NamedPipeLayer

stored::Poller
--------------

.. doxygenclass:: stored::Poller

stored::PrintLayer
------------------

.. doxygenclass:: stored::PrintLayer

stored::ProtocolLayer
---------------------

.. doxygenclass:: stored::ProtocolLayer

stored::SegmentationLayer
-------------------------

.. doxygenclass:: stored::SegmentationLayer

stored::StdioLayer
------------------

.. doxygenclass:: stored::StdioLayer

stored::SyncZmqLayer
--------------------

.. doxygenclass:: stored::SyncZmqLayer

stored::TerminalLayer
---------------------

.. doxygenclass:: stored::TerminalLayer

