Components
==========

Components are C++ classes that use store objects for input/output/parameters/control.
This let's you easily tune and debug an application.

The instances of these classes are tuned at compile-time to reflect the objects in the store.
Especially, no resources are used for (optional) fields that do not exist in the store, and
all store lookups in the directory are done at compile-time. You need C++14 (or later), though.

Check out the ``components`` and ``control`` examples.

stored::Amplifier
-----------------

.. doxygenclass:: stored::Amplifier

stored::LowPass
---------------

.. doxygenclass:: stored::LowPass

stored::PID
-----------

.. doxygenclass:: stored::PID

stored::PinIn
-------------

.. doxygenclass:: stored::PinIn

stored::PinOut
--------------

.. doxygenclass:: stored::PinOut

stored::PulseWave
-----------------

.. doxygenclass:: stored::PulseWave

stored::Ramp
------------

.. doxygenclass:: stored::Ramp

stored::Sine
------------

.. doxygenclass:: stored::Sine

