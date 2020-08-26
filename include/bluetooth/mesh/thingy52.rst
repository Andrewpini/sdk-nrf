.. _bt_mesh_thingy52_readme:

Thingy52 mesh models
######################

The Thingy52 models are a set of vendor spesific models that are designed for applications
involving LED and speaker functionality.

The following Thingy52 models are supported:

.. toctree::
   :maxdepth: 1
   :glob:

   thingy52_srv.rst
   thingy52_cli.rst

These models are well suited for applications where the designer wants to create applications
that propagates light and sound effects between nodes inside a bluetooth mesh. One clear
advatage is that the propagation behaviour between nodes can be reconfigured at runtime due
to the pub/sub configuration capabilities of bluetooth mesh, making it easy to alter the
effects on the fly without having to flash new firmware on the devices.

Common types
============

| Header file: :file:`demos/bluetooth/mesh/thingy52/common/thingy52.h`

.. doxygengroup:: bt_mesh_thingy52
   :project: nrf
   :members:
