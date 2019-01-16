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
C++11 compiler, the library development process assumes that the host OS is Linux.

Prerequisites:

* Google test library for C++ - gtest (downloaded as part of the build from [github](https://github.com/google/googletest))
* C++11 capable compiler with GCC-like interface (e.g. GCC, Clang)
* CMake 2.8+

Building the debug version and running the unit tests:
```bash
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
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
mkdir build && cd build && cmake .. -DCMAKE_BUILD_TYPE=Debug
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
