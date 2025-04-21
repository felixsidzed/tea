Quickstart
==========

This section helps you get started with Tea quickly.

Requirements
------------

- Python 3.11+
- Microsoft Visual Studio Build Tools (for `link.exe`)

Installation
------------

Download `tea.exe` and place it in your working directory or system path.

Example Directory Structure
---------------------------

.. code-block:: none

    .
    ├── tea.exe           # Tea CLI
    ├── test.tea          # Tea source file
    ├── build/
    │   └── test.o        # Compiled object file
    ├── lib/
    │   └── teastd.lib    # Tea standard library

Compiling a Tea Program
-----------------------

To compile a Tea program into a native object file, use:

.. code-block:: bash

    tea test.tea -o build/test.o
