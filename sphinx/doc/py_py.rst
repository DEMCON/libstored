

..
   SPDX-FileCopyrightText: 2020-2025 Jochem Rutgers
   
   SPDX-License-Identifier: CC-BY-4.0

Base classes
==============

ZmqClient
---------

.. autoclass:: libstored.ZmqClient
   :members:
   :undoc-members:

Protocol layers
---------------

.. autoclass:: libstored.protocol.ProtocolLayer
   :members:
   :undoc-members:

.. autoclass:: libstored.protocol.Aes256Layer

.. autoclass:: libstored.protocol.ArqLayer

.. autoclass:: libstored.protocol.AsciiEscapeLayer

.. autoclass:: libstored.protocol.CobsLayer

.. autoclass:: libstored.protocol.Crc16Layer

.. autoclass:: libstored.protocol.Crc32Layer

.. autoclass:: libstored.protocol.Crc8Layer

.. autoclass:: libstored.protocol.DebugArqLayer

.. autoclass:: libstored.protocol.FileLayer

.. autoclass:: libstored.protocol.LoopbackLayer

.. autoclass:: libstored.protocol.MuxLayer

.. autoclass:: libstored.protocol.PrintLayer

.. autoclass:: libstored.protocol.PubTerminalLayer

.. autoclass:: libstored.protocol.RawLayer

.. autoclass:: libstored.protocol.ReqRepCheckLayer

.. autoclass:: libstored.protocol.SegmentationLayer

.. autoclass:: libstored.protocol.SerialLayer

.. autoclass:: libstored.protocol.StdinLayer

.. autoclass:: libstored.protocol.StdioLayer

.. autoclass:: libstored.protocol.TerminalLayer

.. autoclass:: libstored.protocol.ZmqServer

.. autoclass:: libstored.protocol.ZmqSocketClient

.. autoclass:: libstored.protocol.ZmqSocketServer

Protocol stack
--------------

.. autoclass:: libstored.protocol.ProtocolStack
   :members:
   :undoc-members:

.. autofunction:: libstored.protocol.register_layer_type

.. autofunction:: libstored.protocol.unregister_layer_type

.. autofunction:: libstored.protocol.get_layer_type

.. autofunction:: libstored.protocol.get_layer_types

.. autofunction:: libstored.protocol.build_stack

.. autofunction:: libstored.protocol.stack

