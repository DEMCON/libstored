Poller
======

Basically, these classes provide ``poll()`` in a platform-independent way.  To
use the poller, create one or more Pollables, and register them to the
:cpp:class:`stored::Poller`, and call its ``poll()`` member function. It
returns a list of pollables that have an event to be processed.

The inheritance of the Poller classes is shown below.

.. uml::

   abstract Pollable
   abstract TypedPollable
   Pollable <|-- TypedPollable
   TypedPollable <|-- PollableCallback
   TypedPollable <|-- PollableFd
   TypedPollable <|-- PollableFileLayer
   TypedPollable <|-- PollableHandle
   TypedPollable <|-- PollableSocket
   TypedPollable <|-- PollableZmqLayer
   TypedPollable <|-- PollableZmqSocket

   abstract InheritablePoller
   InheritablePoller <|-- Poller

   Pollable <.. Poller

.. dummy|


Pollables
---------

stored::Pollable
````````````````

.. doxygenstruct:: stored::Pollable

stored::PollableCallback
````````````````````````

.. doxygenclass:: stored::PollableCallback

.. doxygenfunction:: stored::pollable(PollableCallback<>::f_type f, Pollable::Events const &events, void *user = nullptr)
.. dummy*

.. doxygenfunction:: stored::pollable(F &&f, Pollable::Events const &events, void *user = nullptr)
.. dummy*


stored::PollableFd
``````````````````

.. doxygenclass:: stored::PollableFd

.. doxygenfunction:: stored::pollable(int fd, Pollable::Events const &events, void *user = nullptr)
.. dummy*

stored::PollableFileLayer
`````````````````````````

.. doxygenclass:: stored::PollableFileLayer

.. doxygenfunction:: stored::pollable(PolledFileLayer &l, Pollable::Events const &events, void *user = nullptr)
.. dummy*

stored::PollableHandle
``````````````````````

.. doxygenclass:: stored::PollableHandle

stored::PollableSocket
``````````````````````

.. doxygenclass:: stored::PollableSocket

.. doxygenfunction:: stored::pollable(SOCKET s, Pollable::Events const &events, void *user = nullptr)
.. dummy*

stored::PollableZmqLayer
````````````````````````

.. doxygenclass:: stored::PollableZmqLayer

.. doxygenfunction:: stored::pollable(ZmqLayer &l, Pollable::Events const &events, void *user = nullptr)
.. dummy*

stored::PollableZmqSocket
`````````````````````````

.. doxygenclass:: stored::PollableZmqSocket

.. doxygenfunction:: stored::pollable(void *s, Pollable::Events const &events, void *user = nullptr)
.. dummy*


stored::InheritablePoller
-------------------------

.. doxygenclass:: stored::InheritablePoller

stored::Poller
--------------

.. doxygenclass:: stored::Poller
