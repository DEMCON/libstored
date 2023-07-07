Pipes
=====

Pipes can be used to compose functionality, such that data streams through a pipe and is modified on the go.
Pipes are sequence of objects, which inspect and/or modify the data that is passed through it.

A pipe consists of pipe segments, which are combined into a single pipe object.
Pipe segments cannot be accessed or split in run-time, they are absorbed into some complex template-based recursive type, implementing the ``Pipe`` interface.
Pipes, however, can be connected dynamically.

Pipes always have the following form, for a given type ``T``:

.. highlight:: cpp

.. code-block::
   :linenos:

   auto pipe =
      Entry<T>{} >>
      // pipe segments...
      Exit{};

``pipe`` is a pipe instance, taking data of type ``T``, and producing it, such as ``1 >> pipe;``, or ``pipe.extract();``.
It can be connected to other pipe instances dynamically.

.. code-block::
   :linenos:

   auto pipe =
      Entry<T>{} >>
      // pipe segments...
      Cap{};

In this case, ``pipe`` is an instance, with a cap at the end; it does not allow dynamic binding to other pipes.

.. code-block::
   :linenos:

   auto& pipe =
      Entry<T>{} >>
      // pipe segments...
      Ref{};

Now, ``pipe`` is a reference, like the capped pipe above.
It is automatically destroyed at shutdown (by the ``stored::pipes::gc`` ``Group``).
When a group reference is passed to ``Ref{...}``, the pipe is still allocated using the default allocator, but added to the provided group, instead of ``gc``.

Pipe segments can be connected, such as:

.. code-block::
   :linenos:

   auto& pipe =
      Entry<int>{} >>
      Tee{
            Entry<int>{} >>
            Log<int>{"tee 1"} >>
            Ref{},

            Entry<int>{} >>
            Call{[](int x) { printf("tee 2 = %d\n", x); }} >>
            Ref{}
      } >>
      Buffer<int>{} >>
      Cap{};

A pipe segment has to adhere to the following rules:

- It must be a ``struct`` or ``class``.
  No specific base classes have to be used.
  No specific (virtual) interface is defined.
- It must implement a ``B inject(A)`` member function, where the types ``A`` and ``B`` can be freely chosen.
  In fact, having ``inject()`` makes a class a pipe segment.
  The ``inject()`` function is used to extract the pipe segment's input type ``A`` and output type ``B``.
  You are free to choose if the type is a value or reference, or cv-qualified.
- It may implement ``B_ extract()``, where ``B_`` must compatible with ``B``.
  However, ``B`` could be ``const&``, while ``B_`` is just a value, for example.
- It may implement ``A_ entry_cast(B_) const`` and/or ``B_ exit_cast(A_) const``, where ``A_`` and ``B_`` for both functions must be compatible with ``A`` and ``B``.
- It may implement ``B_ trigger(bool*)``, where ``B_`` must be compatible with ``B``.
- It must be move-constructable.
- Preferably, keep pipe segments simple and small.
  Create complexity by composition.

For examples, check ``include/libstored/pipes.h``, and especially the ``Identity`` class implementation for the simplest form.

The following functions are available for pipes, and can be available for pipe segments:

``B inject(A)``
   Inject a value into the pipe (segment).
   The function returns the output of the pipe (segment).
   This is the normal direction of the data flow.

``B extract()``
   This function tries to find data from the exit of the pipe back through the segments.
   Usually, it returns the value of the last ``Buffer`` in the pipe.
   If there is no such segment, a default constructed ``B`` is returned.
   If a pipe segment does not support extraction, it can omit the function.

``A entry_cast(B) const``
   Type-cast the given pipe (segment) output type to the input type.
   It should not modify the state of the pipe (segment).
   When omitted, it is assumed that the input and output types are assignable.

``B exit_cast(A) const``
   Type-cast the given pipe (segment) input type to the output type.
   It should not modify the state of the pipe (segment).
   When omitted, it is assumed that the input and output types are assignable.

``B trigger(bool*)``
   Some pipe segments have a special side-effect, such as reading external data.
   It may implement the trigger function to perform this side-effect.
   The parameter can be used to indicate if the pipe (segment) has returned any data by writing ``true`` to the provided pointer.
   The first pipe segment that provides data by a trigger, injects this data into the remainder of the pipe.



.. uml::

   abstract PipeEntry
   abstract PipeExit
   abstract Pipe
   abstract PipeBase
   PipeBase <|-- Pipe
   PipeEntry <|-- Pipe
   PipeExit <|-- Pipe

   class PipeInstance
   class Segments
   Pipe <|-- PipeInstance
   Segments <|-- PipeInstance : private

..  <|    fix highlighting...

stored::pipes::PipeBase
-----------------------

.. doxygenclass:: stored::pipes::PipeBase

stored::pipes::Pipe
-------------------

.. doxygenclass:: stored::pipes::Pipe

stored::pipes::PipeEntry
------------------------

.. doxygenclass:: stored::pipes::PipeEntry

stored::pipes::PipeExit
-----------------------

.. doxygenclass:: stored::pipes::PipeExit

stored::pipes::Group
--------------------

.. doxygenclass:: stored::pipes::Group

There is one special group: ``stored::pipes::gc``, which destroys pipes created with default ``Ref``.

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

stored::pipes::Convert
----------------------

.. doxygenclass:: stored::pipes::Convert
.. doxygenclass:: stored::pipes::Scale


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

stored::pipes::Signal
---------------------

.. doxygenclass:: stored::pipes::Signal

stored::pipes::Tee
------------------

.. doxygenclass:: stored::pipes::Tee

stored::pipes::Transistor
-------------------------

.. doxygenclass:: stored::pipes::Transistor

stored::pipes::Triggered
------------------------

.. doxygenclass:: stored::pipes::Triggered
