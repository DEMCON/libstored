Pipes
=====

.. uml::

   abstract PipeEntry
   abstract PipeExit
   abstract Pipe
   PipeEntry <|-- Pipe
   PipeExit <|-- Pipe

   class PipeInstance
   class Segments
   Pipe <|-- PipeInstance
   Segments <|-- PipeInstance : private

stored::pipes::Pipe
-------------------

.. doxygenclass:: stored::pipes::Pipe

stored::pipes::PipeEntry
------------------------

.. doxygenclass:: stored::pipes::PipeEntry

stored::pipes::PipeExit
-----------------------

.. doxygenclass:: stored::pipes::PipeExit

stored::pipes::Buffer
---------------------

.. doxygenclass:: stored::pipes::Buffer

stored::pipes::Call
-------------------

.. doxygenclass:: stored::pipes::Call

stored::pipes::Cast
-------------------

.. doxygentypedef:: stored::pipes::Cast

stored::pipes::Changes
----------------------

.. doxygenclass:: stored::pipes::Changes
.. doxygentypedef:: stored::pipes::similar_to

stored::pipes::Constrained
--------------------------

.. doxygenclass:: stored::pipes::Constrained
.. doxygenclass:: stored::pipes::Bounded

stored::pipes::Get
------------------

.. doxygenclass:: stored::pipes::Get

stored::pipes::Identity
-----------------------

.. doxygenclass:: stored::pipes::Identity

stored::pipes::Log
------------------

.. doxygenclass:: stored::pipes::Log

stored::pipes::Map
------------------

.. doxygenclass:: stored::pipes::IndexMap
.. doxygenclass:: stored::pipes::OrderedMap
.. doxygenclass:: stored::pipes::RandomMap
.. doxygenfunction:: stored::pipes::make_random_map

.. doxygenclass:: stored::pipes::Mapped
.. doxygenfunction:: stored::pipes::Map(std::pair<Key, Value> const (&kv)[N], CompareKey compareKey, CompareValue compareValue)
.. doxygenfunction:: stored::pipes::Map(T const (&values)[N], CompareValue compareValue)
.. doxygenfunction:: stored::pipes::Map(T0 &&v0, T1 &&v1, T&&... v)

stored::pipes::Mux
------------------

.. doxygenclass:: stored::pipes::Mux

stored::pipes::RateLimit
------------------------

.. doxygenclass:: stored::pipes::RateLimit

stored::pipes::Set
------------------

.. doxygenclass:: stored::pipes::Set

stored::pipes::Tee
------------------

.. doxygenclass:: stored::pipes::Tee

stored::pipes::Transistor
-------------------------

.. doxygenclass:: stored::pipes::Transistor
