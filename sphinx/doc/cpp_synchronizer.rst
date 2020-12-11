Synchronizer
============

Distributed store synchronizer.

In a distributed system, every process has its own instance of a store.
Synchronization between these instances is implemented by the stored::Synchronizer.
The Synchronizer can be seen as a service, usually one per process,
which knows all stores in that process and all communication channels to other
processes. At regular intervals, it sends updates of locally modified data
to the other Synchronizers.

The topology can be configured at will. In principle, a process can have any
number of stores, any number of synchronizers (which all handle any subset of the stores),
any number of connections to any other process in the system.

There are a few rules to keep in mind:

- Only stored::Synchronizable stores can be handled by the Synchronizer.
  This has to be used correctly when the store is instantiated.
- To synchronize a store, one must define which store is the one that
  provides the initial value. Upon connection between Synchronizers, the
  store's content is synchronized at once from one party to the other.
  Afterwards, updates are sent in both directions.
- Writes to different objects in the same store by the same process are
  observed by every other process in the same order. All other write orders
  are undefined (like writes to objects of different stores by the same
  process, or writes to the same store by different processes), and can be
  observed to happen in a different order by different processes at the same
  time.
- Writes to one object should only be done by one process. So, every process owns
  a subset of a store. If multiple processes write to the same object, behavior
  is undefined. That would be a race-condition anyway.
- The communication is done in the store's endianness. If a distributed
  system have processors with different endianness, they should be configured
  to all-little or all-big endian. Accessing the store by the processor that
  has a store in a non-native endianness, might be a bit more expensive, but
  synchronization is cheaper.
- Stores are identified by their (SHA-1) hash. This hash is computed over the full
  source code of the store (the .st file). So, only stores with the exact same
  definition, and therefore layout, can be synchronized.

Protocol
--------

The protocol for synchronization consists of four messages. These are sent
when appropriate, not in a request-response paradigm. There is no acknowledge.
Invalid messages are just ignored.

Hello
`````

"I would like to have the full state and future changes
of the given store (by hash). All updates, send to me
using this reference."

(``h`` | ``H``) <hash> <id>

The hash is returned by the ``hash()`` function of the store, including the
null-terminator. The id is arbitrary chosen by the Synchronizer, and is 16-bit
in the store's endianness (``h`` indicates little endian, ``H`` is big).

Welcome
```````

This is a response to a Hello.

"You are welcome. Here is the full buffer state, upon your request, of the
store with given reference. Any updates to the store at your side,
provide them to me with my reference."

(``w`` | ``W``) <hello id> <welcome id> <buffer>

The hello id is the id as received in the hello message (by the other party).
The welcome id is chosen by this Synchronizer, in the same manner.

Update
``````

"Your store, with given reference, has changed.
The changes are attached."

(``u`` | ``U``) <id> <updates>

The updates are a sequence of the triplet: <key> <length> <data>.  The key and
length have the most significant bytes stripped, which would always be 0.  All
values are in the store's endianness (``u`` is little, ``U`` is big endian).

Proposal: The updates are a sequence defined as follows:
<5 MSb key offset, 3 LSb length> <additional key bytes> <additional length bytes> <data>.

The key offset + 1 is the offset from the previous entry in the updates
sequence.  Updates are sent in strict ascending key offset.  The initial key is
-1. For example, if the previous key was 10 and the 5 MSb indicate 3, then the
next key is 10 + 3 + 1, so 14. If the 5 MSb are all 1 (so 31), an additional
key byte is added after the first byte (which may be 0). This value is added to
the key offset.  If that value is 255, another key byte is added, etc.

The 3 LSb bits of the first byte are decoded according to the following list:

- 0: data length is 1
- 1: data length is 2
- 2: data length is 3
- 3: data length is 4
- 4: data length is 5
- 5: data length is 6
- 6: data length is 7, and an additional length byte follows (like the key offset)
- 7: data length is 8

Using this scheme, when all variables change within the store, the overhead is
always one byte per variable (plus additional length bytes, but this is
uncommon and fixed for a given store). This is also the upper limit of the
update message.  If less variables change, the key offset may be larger, but
the total size is always less.

The asymmetry of having 6 as indicator for additional length bytes is because
this is an unlikely value (7 bytes data), and at least far less common than
having 8 bytes of data.

The data is sent in the store's endianness (``u`` is little, ``U`` is big
endian).

Bye
```

"I do not need any more updates of the given store (by hash, by id or all)."

| (``b`` | ``B``) <hash>
| (``b`` | ``B``) <id>
| (``b`` | ``B``)

A bye using the id can be used to respond to another message that has an unknown id.
Previous communication sessions remnants can be cleaned up in this way.

``b`` indicates that the id is as little endian, ``B`` indicates big endian.
For the other two variants, there is no difference in endianness, but both
versions are defined for symmetry.



stored::StoreJournal
--------------------

.. doxygenclass:: stored::StoreJournal

stored::SyncConnection
----------------------

.. doxygenclass:: stored::SyncConnection

stored::Synchronizable
----------------------

.. doxygenclass:: stored::Synchronizable

stored::Synchronizer
--------------------

.. doxygenclass:: stored::Synchronizer

