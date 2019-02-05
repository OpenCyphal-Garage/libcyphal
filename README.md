UAVCAN stack in C++
===================

[![Travis CI](https://travis-ci.org/UAVCAN/libuavcan.svg?branch=uavcan-v1.0-bluesky)](https://travis-ci.org/UAVCAN/libuavcan)

Portable reference implementation of the [UAVCAN protocol stack](http://uavcan.org) in C++ for embedded systems, Linux, and POSIX-compliant RTOSs.

UAVCAN is a lightweight protocol designed for reliable communication in aerospace and robotic applications over robust vehicular networks.

v1.0 is a complete rewrite of this library with the following, fundamental changes from v0:

1. libuavcan v1 is based on the UAVCAN v1 specification. UAVCAN v1 is _not_ backwards compatible with v0.
2. libuavcan v1 requires C++11 or greater.
3. libuavcan v1 is a header-only library.

Some of this documentation in this README is temporary and relevant to the "blue sky" reimplementation effort. After the first official release of libuavcan v1 sections like this will be removed, v0 will be referred to as legacy, and "blue sky" will become simply mainline.

### About the Blue Sky effort

You should never rewrite a codebase from scratch. It's a terrible idea. We're rewriting libuavcan from scratch. We're terrible developers.

Sorry.

The reason we opted for a complete rewrite is that so much is changing. Between the updates to the specification, the abandonment of C++98 support, switching to header-only, removing the drivers from the main repository, adding CAN-FD support, etc, etc, etc; it was obvious that we'd be rewriting everything anyway. The good news is that v0 exists, is fully supported, and will be liberally copy-and-pasted from as this makes sense for v1. What we don't have is any git history tracing from v1 since this would be deceiving. Futhermore, the unit tests in uavcan v0 are a bit of a mess so we'll be writing them in a way that is more sustainable.

## Documentation

* [UAVCAN website](http://uavcan.org)
* [UAVCAN forum](https://forum.uavcan.org)

## Library development

Despite the fact that the library itself can be used on virtually any platform that has a standard-compliant
C++11 compiler, the library development process assumes that the host OS is Linux or OSX.

Prerequisites:

* Google test library for C++ - gtest (downloaded as part of the build from [github](https://github.com/google/googletest))
* C++11 capable compiler with GCC-like interface (e.g. GCC, Clang)
* CMake 3.5+
* python3
* virtualenv

Building the debug version and running the unit tests:
```bash
mkdir build
cd build
cmake ..
make -j8
make ARGS=-VV test
```

Test outputs can be found in the build directory under `libuavcan`.

Contributors, please follow the [Zubax C++ Coding Conventions](https://kb.zubax.com/x/84Ah).

### Vagrant

Vagrant can be used to setup a compatible Ubuntu virtual image. Follow the instructions on [Vagrantup](https://www.vagrantup.com/) to install virtualbox and vagrant then do:

```bash
vagrant up
vagrant ssh
mkdir build && cd build && cmake ..
```

Note that you may (probably will) have to increase the virtual memory available to the virtual machine created by Vagrant.

You can build using commands like:

```bash
vagrant ssh -c "cd /vagrant/build && make -j4 && make test"
```

or to run a single test:

```bash
vagrant ssh -c "cd /vagrant/build && make libuavcan_test && ./libuavcan/libuavcan_test --gtest_filter=Node.Basic"
```

### Submitting a Coverity Scan build

We'll update this section when we enable Coverity builds for the blue-sky implementation.

### Folder Structure

**/libuavcan/include** - Contains the entire header-only libuavcan library.

**/test/native** - Unit-tests that validate the libuavcan library. These tests compile and execute using the build host's native environment. They also do not require any communication interfaces, virtual or otherwise, from the operating system and have no timing constraints.

**/test/ontarget** - Tests cross-compiled for specific hardware* and run on a set of dedicated test devices. These tests may have strict timing constraints and may require specific physical or virtual busses and other test apparatuses be present. Each on-target test will fully document its requirements to enable anyone with access to the appropriate hardware to reproduce the tests. Furthermore, these tests must be inherently automateable having clear pass/fail criteria reducible to a boolean condition.

**/example** - Contains a set of example applications providing real, practical, and tested uses of libuavcan.

**/submodules** - Because there is generally no indication on most platforms that a given folder is the root of a git submodule libuavcan puts all of its submodule dependencies directly under this folder. Each folder under this one represents a single git submodule repository that may have their own submodules (i.e. recursive). The project's CMakeLists.txt has logic to synchronize these modules as part of the build so no manual git operations should be required.

**/commons** - Reference implementations for parts of the libuavcan library. These implementations are optional but are provided as a convenience to accelerate evaluation and integration of libuavcan.

#### Test Environments

The following list of standardized* test environments will be used to validate the libuavcan implementation**:

1. **Posix** - We will produce examples that can run on top of SocketCAN on Ubuntu 18 or newer. While expect that these examples will be generally compatible with other common linux distros or posix compliant operating systems (that also support SocketCAN) we will compile and test the examples using Ubuntu 18 as part of our CI build.
1. **Bare-metal on NXP s32K146 devkit** - We expect to produce examples and tests that run on the s32K146 MCU populated on the standard s32146 evaluation board available from NXP. This is the primary test fixture for the project and will be used as the basis for specifying on-target test rigs.
1. **Nuttx on Pixhawk4** - We expect to produce examples and possibly tests that can run on top of the latest pixhawk hardware and version of Nuttx used by the px4 software stack. This is a lower-priority for the initial development for v1 but will become a focus once we have a fully functional stack.

\* Note that libuavcan is a header only library suitable for a wide range of processors and operating systems. The targets and test environments mentioned here are chosen only as standardized test fixtures and are not considered more "supported" or "optimal" than any other platform.

\*\* Libuavcan development should be test-driven. Write the tests first (well, first write the specification, then the APIs, then the tests).
