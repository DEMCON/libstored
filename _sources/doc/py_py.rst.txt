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

.. autoclass:: libstored.protocol.AsciiEscapeLayer

.. autoclass:: libstored.protocol.TerminalLayer

.. autoclass:: libstored.protocol.PubTerminalLayer

.. autoclass:: libstored.protocol.SegmentationLayer

.. autoclass:: libstored.protocol.DebugArqLayer

.. autoclass:: libstored.protocol.Crc8Layer

.. autoclass:: libstored.protocol.Crc16Layer

.. autoclass:: libstored.protocol.LoopbackLayer

.. autoclass:: libstored.protocol.RawLayer

Protocol stack
--------------

.. autoclass:: libstored.protocol.ProtocolStack
   :members:
   :undoc-members:

.. autofunction:: libstored.protocol.registerLayerType

.. autofunction:: libstored.protocol.buildStack

