

..
   SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
   
   SPDX-License-Identifier: CC-BY-4.0

9_fpga
=======

Store definition
----------------

ExampleFpga
```````````

.. literalinclude:: ../../examples/9_fpga/ExampleFpga.st

ExampleFpga2
````````````

.. literalinclude:: ../../examples/9_fpga/ExampleFpga2.st

Application
-----------

.. literalinclude:: ../../examples/9_fpga/main.cpp
   :language: cpp

FPGA
----

.. literalinclude:: ../../examples/9_fpga/rtl/toplevel.vhd

Store reference
---------------

.. doxygenclass:: stored::ExampleFpgaObjects

.. doxygenclass:: stored::ExampleFpgaBase

.. doxygenclass:: ExampleFpga

.. doxygenclass:: stored::ExampleFpga2Objects

.. doxygenclass:: stored::ExampleFpga2Base

.. doxygenclass:: ExampleFpga2

