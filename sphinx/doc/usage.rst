..
   SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
   
   SPDX-License-Identifier: CC-BY-4.0

Usage
=====

On this page follow some notes on how to use `libstored` in your application.

Consider also the listed :ref:`Examples` and the :ref:`Cpp` API.


Store Access
------------

Store :cpp:class:`variables <stored::Variable>` can be read or set directly most of the time.
There is also the :cpp:func:`~stored::Variable::get` and :cpp:func:`~stored::Variable::set` methods.

.. code-block::

   // Store definition
   uint32     my integer

.. code-block:: cpp

   store.my_integer = 1;
   // ...implies:
   store.my_integer.set(1);

   unsigned int x;
   x = store.my_integer;
   // ...implies:
   x = store.my_integer.get();


Arrays
^^^^^^

Array elements appear as regular flat variables after code generation, with the addition of an extra ``_a(int)`` method.
This accessor provides element access based on a variable index:

.. code-block::

   // Store definition
   bool[10]     my array

.. code-block:: cpp

   store.my_array_a(0).set(true);
   bool value = store.my_array_a(1).get();

Strings
^^^^^^^

Strings (and blobs) cannot be set or retrieved through regular assignment, but require the buffered accessors: :cpp:func:`~size_t stored::Variant::get(void*, size_t) const` and :cpp:func:`~size_t stored::Variant::set(void const*, size_t)`.

Note that store strings end with a null-terminator.
And when reading from a store string to a local buffer it will stop on the first null-terminator it encounters.

.. code-block::

   // Store definition
   string:16     my string

.. code-block:: cpp

   char txt[16];
   store.my_string.get(txt, 16);

   const char new_txt[16] = "hi again";
   store.my_string.set(new_txt, 16);
