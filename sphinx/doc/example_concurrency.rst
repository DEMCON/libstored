concurrency
===========

Store definition
----------------

ExampleConcurrencyMain
``````````````````````

.. literalinclude:: ../../examples/concurrency/ExampleConcurrencyMain.st

ExampleConcurrencyControl
`````````````````````````

.. literalinclude:: ../../examples/concurrency/ExampleConcurrencyControl.st

Application
-----------

.. literalinclude:: ../../examples/concurrency/main.cpp
   :language: cpp

Output
------

When started like: ``concurrency 3``

.. literalinclude:: ../../examples/concurrency/output.txt


.. only:: threads

   Store reference
   ---------------

   .. doxygenclass:: stored::ExampleConcurrencyMainObjects

   .. doxygenclass:: stored::ExampleConcurrencyMainBase

   .. doxygenclass:: stored::ExampleConcurrencyMain

   .. doxygenclass:: stored::ExampleConcurrencyControlObjects

   .. doxygenclass:: stored::ExampleConcurrencyControlBase

   .. doxygenclass:: stored::ExampleConcurrencyControl

