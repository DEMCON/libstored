Base classes
==============

ZmqClient
---------

.. autoclass:: ed2.ZmqClient
   :members:
   :undoc-members:

Protocol layers
---------------

.. autoclass:: ed2.protocol.ProtocolLayer
   :members:
   :undoc-members:

.. autoclass:: ed2.protocol.AsciiEscapeLayer

.. autoclass:: ed2.protocol.TerminalLayer

.. autoclass:: ed2.protocol.SegmentationLayer

.. autoclass:: ed2.protocol.DebugArqLayer

.. autoclass:: ed2.protocol.Crc8Layer

.. autoclass:: ed2.protocol.Crc16Layer

.. autoclass:: ed2.protocol.LoopbackLayer

.. autoclass:: ed2.protocol.RawLayer

Protocol stack
--------------

.. autoclass:: ed2.protocol.ProtocolStack
   :members:
   :undoc-members:

.. autofunction:: ed2.protocol.registerLayerType

.. autofunction:: ed2.protocol.buildStack

