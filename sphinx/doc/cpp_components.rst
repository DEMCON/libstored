Components
==========

Components are C++ classes that use store objects for input/output/parameters/control.
This let's you easily tune and debug an application.

The instances of these classes are tuned at compile-time to reflect the objects in the store.
Especially, no resources are used for (optional) fields that do not exist in the store, and
all store lookups in the directory are done at compile-time. You need C++14 (or later), though.

Check out the ``components`` example.

stored::Amplifier
-----------------

.. doxygenclass:: stored::Amplifier

stored::PinIn
-------------

.. doxygenclass:: stored::PinIn

stored::PinOut
--------------

.. doxygenclass:: stored::PinOut

stored::PID
-----------

.. doxygenclass:: stored::PID

