.. _bt_mesh_thingy52_srv_readme:

Thingy:52 Server Model
######################

The Thingy:52 Server represents a single Thingy:52 device.

States
======

The Thingy:52 Server model contains the following state:

Time to Live: ``uint16_t``
	The Time to Live state determines how many hops a single message has left before it should be disregarded.

Delay: ``uint16_t``
	The Delay state determines how long the effects of a message shall last within a mesh node.

Color: :cpp:type:`bt_mesh_thingy52_rgb`
	The Color state determines the RBG Color assosiated with a message.

Speaker On: ``boolean``
	The Speaker On state determines if a message should support speaker output.

All state is assosiated with a single message, and is accessed by your application through the :cpp:type:`rgb_set_handler`.
It is expected that your application holds the state of each induvidual message, and handles them according
to your applications spesification.


Extended models
===============

None.

Persistent storage
==================

None.

API documentation
=================

| Header file: :file:`demos/bluetooth/mesh/thingy52/common/thingy52_srv.h`
| Source file: :file:`demos/bluetooth/mesh/thingy52/common/thingy52_srv.c`

.. doxygengroup:: bt_mesh_thingy52_srv
   :project: nrf
   :members:
