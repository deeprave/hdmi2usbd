Installation from source
========================

This document describes the process of building hdmi2usbd from source.

Prerequisites
~~~~~~~~~~~~~

In order to build this software you will require:

-  a contemporary UNIX-like system such as Linux, FreeBSD or MacOS X.
   Other environments are untested, but may accidentally work.
-  a modern ansi C compiler supporting *at least **C99*** (some C99
   language features are used), including make
-  **cmake** version 3.4 or later, available from your distribution (or
   homebrew with OS X)

Obtaining the software
~~~~~~~~~~~~~~~~~~~~~~

Clone the git repository:

::

    $ git clone https://github.com/deeprave/hdmi2usbd
    $ cd hdmi2usbd

Building ``hdmi2usb``
~~~~~~~~~~~~~~~~~~~~~

The following will create the build files for make in
the\ ``build``\ directory under the directory where hdmi2usbd was
cloned:

::

    $ mkdir build
    $ cd build
    $ cmake ..
    $ make

The ``hdmi2usbd`` executable will be in the
aforementioned\ ``build``\ directory (current one) when the build
completes.

Building and running tests
~~~~~~~~~~~~~~~~~~~~~~~~~~

Building the test executables requires the `googletest
library <https://github.com/google/googletest>`__, version 1.7 or
greater.

::

    $ git submodule init
    $ git submodule update

To build with tests, clear out and rebuild the\ ``build``\ directory, change to it and run:

::

    $ rm -rf build/*
    $ cd build
    $ cmake -Dtests=ON ..
    $ make

The\ ``runUnitTests``\ executable will be in the\ ``build``\ directory.
Use\ ``./runUnitTests --help``\ command for ussage information, or run
with no arguments to run all of the unit tests.
