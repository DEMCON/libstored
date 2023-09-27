

..
   SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
   
   SPDX-License-Identifier: CC-BY-4.0

Directory
=========

Directory with names, types and buffer offsets.

The directory is a description in binary. While parsing the pointer starts at
the beginning of the directory and scans over the bytes. While scanning, a name
is searched.  In principle, the directory is a binary tree of characters the
name must match.

It is using the following grammar:

::

   directory ::= expr

   expr ::=
        # Hierarchy separator: skip all characters of the name until a '/' is encountered.
        '/' expr |
        # Match current char in the name. If it compress less or greater, add the jmp_l or
        # jmp_g to the pointer. Otherwise continue with the first expression.
        # If there is no object for a specific jump, jmp_* can be 0, in which case the
        # expr_* is omitted.
        char jmp_l jmp_g expr expr_l ? expr_g ? |
        # Skip the next n non-/ characters of the name.
        skip expr |
        # A variable has been reached for the given name.
        var |
        # No variable exists with the given name.
        end
        # Note that expr does never start with \x7f (DEL).
        # Let's say, it is reserved for now.

   char ::= [\x20..\x2e,\x30..\x7e]     # printable ASCII, except '/'
   int ::= bytehigh * bytelow           # Unsigned VLQ
   byte ::= [0..0xff]
   bytehigh ::= [0x80..0xff]            # 7 lsb carry data
   bytelow ::= [0..0x7f]                # 7 lsb carry data

   # End of directory marker.
   end ::= 0

   # The jmp is added to the pointer at the position of the last byte of the int.
   # So, a jmp value of 0 effectively results in end.
   jmp ::= int

   var ::= (String | Blob) size offset | type offset
   type ::= [0x80..0xff]                # This is stored::Type::type with 0x80 or'ed into it.
   size ::= int
   offset ::= int

   skip ::= [1..0x1f]



stored::find()
--------------

.. doxygenfunction:: stored::find

stored::list()
--------------

.. doxygenfunction:: stored::list(void *container, void *buffer, uint8_t const *directory, ListCallbackArg *f, void *arg, char const *prefix)

