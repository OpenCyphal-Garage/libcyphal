UAVCAN stack in C++
===================

[![Build Status](https://badge.buildkite.com/af844974c06af6406e3b2192d98298b02b30f6ebebb5f8b16c.svg)](https://buildkite.com/uavcan/libuavcan-v1)
[![Forum](https://img.shields.io/discourse/https/forum.uavcan.org/users.svg)](https://forum.uavcan.org)

Portable reference implementation of the [UAVCAN protocol stack](https://new.uavcan.org) in C++ for embedded systems, Linux, and POSIX-compliant RTOSs.

UAVCAN is a lightweight protocol designed for reliable communication in aerospace and robotic applications over robust vehicular networks.

v1.0 is a complete rewrite of this library with the following, fundamental changes from v0:

1. libuavcan v1 is based on the [UAVCAN v1 specification](https://new.uavcan.org). UAVCAN v1 is _not_ backwards compatible with v0.
1. libuavcan v1 requires C++11 or greater.
1. libuavcan v1 requires a _fairly_ complete implementation of the c++ standard library.
1. libuavcan v1 is a header-only library.

Some of this documentation in this README is temporary and relevant to the "blue sky"
implementation effort. After the first official release of libuavcan v1 sections like this will
be removed, v0 will be referred to as legacy, and "blue sky" will become simply mainline.

### About the Blue Sky effort

You should never rewrite a codebase from scratch. It's a terrible idea. We're rewriting libuavcan from scratch. We're terrible developers.

Sorry.

The reason we opted for a complete rewrite is that so much is changing. Between the updates to the specification, the abandonment of C++98 support, switching to header-only, removing the drivers from the main repository, adding CAN FD support, etc, etc, etc; it was obvious that we'd be rewriting everything anyway. The good news is that v0 exists, is fully supported, and will be liberally copy-and-pasted from as this makes sense for v1. What we don't have is any git history tracing from v1 since this would be deceiving. Futhermore, the unit tests in uavcan v0 are a bit of a mess so we'll be writing them in a way that is more sustainable.

## Documentation

* [UAVCAN website](http://uavcan.org)
* [UAVCAN forum](https://forum.uavcan.org)

### Folder Structure

**/libuavcan/include** - Contains the entire header-only libuavcan library.

**/libuavcan/include/test** – Test utilities provided to consumers of the library. These are public test fixtures and should be documented, maintained, and designed with the same care given to the rest of the library.

**/test/native** - Unit-tests that validate the libuavcan library. These tests compile and execute using the build host's native environment. They also do not require any communication interfaces, virtual or otherwise, from the operating system and have no timing constraints.

**/test/ontarget** - Tests cross-compiled for specific hardware* and run on a set of dedicated test devices. These tests may have strict timing constraints and may require specific physical or virtual busses and other test apparatuses be present. Each on-target test will fully document its requirements to enable anyone with access to the appropriate hardware to reproduce the tests. Furthermore, these tests must be inherently automateable having clear pass/fail criteria reducible to a boolean condition.

**/test/compile** – Tests that run in the compiler. Most of these will be tests that pass if they fail to compile. For example, some tests will purposefully define template parameters that will cause static_asserts to fail. Tests that pass if they do compile are less interesting here since such happy paths are normally covered by unit-tests.

**/example** - Contains a set of example applications providing real, practical, and tested uses of libuavcan.

**/commons** - Reference implementations for parts of the libuavcan library. These implementations are optional but are provided as a convenience to accelerate evaluation and integration of libuavcan.

#### Test Environments

The following list of standardized* test environments will be used to validate the libuavcan implementation**:

1. **Posix** - We will produce examples that can run on top of SocketCAN on Ubuntu 18 or newer. While expect that these examples will be generally compatible with other common linux distros or posix compliant operating systems (that also support SocketCAN) we will compile and test the examples using Ubuntu 18 as part of our CI build.
1. **Bare-metal on NXP s32K146 devkit** - We expect to produce examples and tests that run on the s32K146 MCU populated on the standard s32146 evaluation board available from NXP. This is the primary test fixture for the project and will be used as the basis for specifying on-target test rigs.
1. **Nuttx on Pixhawk4** - We expect to produce examples and possibly tests that can run on top of the latest pixhawk hardware and version of Nuttx used by the px4 software stack. This is a lower-priority for the initial development for v1 but will become a focus once we have a fully functional stack.

\* Note that libuavcan is a header only library suitable for a wide range of processors and operating systems. The targets and test environments mentioned here are chosen only as standardized test fixtures and are not considered more "supported" or "optimal" than any other platform.


## Library development

**Libuavcan development should be test-driven. Write the tests first.**

**Libuavcan source should be fluent. Comment everything in plain prose, build the docs with each change, read the docs to make sure your comments make sense.**


Despite the fact that the library itself can be used on virtually any platform that has a standard-compliant
C++11 compiler, the library development process assumes that the host OS is Linux or OSX.

Prerequisites:

* Google test library for C++ - gtest (downloaded as part of the build from [github](https://github.com/google/googletest))
* C++11 capable compiler with GCC-like interface (e.g. GCC, Clang)
* CMake 3.5+
* clang-format
* python3
* nunavut and (transitively) pydsdl

Building the debug version and running the unit tests:
```bash
mkdir build
cd build
cmake ..
make -j8
make ARGS=-VV test
```

We also support a docker-based workflow which is used for CI build automation. If you want to use this locally either to verify that the CI build will succeed or just to avoid manually installing and maintaining the above dependencies then you can do:

```bash
docker pull uavcan/libuavcan:latest

docker run --rm -v /path/to/libuavcan:/repo uavcan/libuavcan:latest /bin/sh -c ./ci.sh
```

Test outputs can be found in the build directory under `libuavcan`.

Contributors, please follow the [Zubax C++ Coding Conventions](https://kb.zubax.com/x/84Ah) and always use `clang-format` when authoring or modifying files (the build scripts will enforce but not apply the rules in .clang-format).

### Pull-Request Checklist

Reviewers, please check the following items when reviewing a pull-request:

> **NOTE:** This is just the start of this checklist. Expect it to grow and get refined as this project matures.

1. **correctness**
    * Is the code correct.
1. **clarity**
    * Is the code easily understood? 
    * It is overly complex?
1. **test coverage**
    * Were tests written to cover the changes?
1. **test effectiveness and correctness**
    * Are the tests good tests that provide some guarantee that the logic is, and will remain, correct?
1. **documentation**
    * Is the code properly documented?
    * Are there changes needed to auxillary documentation that is missing?
    * Are there good examples for how to use the code?
1. **design**
    * Is the code maintainable?
    * Are the tests maintainable?
    * Is the code in the right namespace/class/function?

### VSCode

To use visual studio code to debug ontarget tests for the S32K146EVB you'll need the [cortex debug](https://marketplace.visualstudio.com/items?itemName=marus25.cortex-debug) plugin. Here's an example configuration using JLink:

```
 {
    "cwd": "${workspaceRoot}",
    "executable": "build/test_util_math.elf",
    "name": "On-target unit test.",
    "request": "launch",
    "type": "cortex-debug",
    "servertype": "jlink",
    "device": "S32K146",
    "svdFile": "test/ontarget/S32K146EVB/S32K146.svd"
},
```
