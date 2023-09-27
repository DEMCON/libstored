

..
   SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
   
   SPDX-License-Identifier: CC-BY-4.0

5_debug
=======

Store definition
----------------

ExampleDebugSomeStore
`````````````````````

.. literalinclude:: ../../examples/5_debug/ExampleDebugSomeStore.st

ExampleDebugAnotherStore
````````````````````````

.. literalinclude:: ../../examples/5_debug/ExampleDebugAnotherStore.st

Application
-----------

.. literalinclude:: ../../examples/5_debug/main.cpp
   :language: cpp

Output
------

.. literalinclude:: ../../examples/5_debug/output.txt

Store reference
---------------

.. doxygenclass:: stored::ExampleDebugSomeStoreObjects

.. doxygenclass:: stored::ExampleDebugSomeStoreBase

.. doxygenclass:: stored::ExampleDebugSomeStore

.. doxygenclass:: stored::ExampleDebugAnotherStoreObjects

.. doxygenclass:: stored::ExampleDebugAnotherStoreBase

.. doxygenclass:: stored::ExampleDebugAnotherStore
