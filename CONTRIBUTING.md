# Libcyphal v1.0 contributor guidelines

v1.0 is a complete rewrite of libcyphal with the following, fundamental changes from v0:

1. libcyphal v1 is based on the [Cyphal v1 specification](https:/opencyphal.org).
2. libcyphal v1 requires C++14 or greater.
3. libcyphal v1 favors idiomatic C++ over defining its own utilities and helpers.
4. libcyphal v1 is a header-only library.
5. libcyphal v1 holds itself to a higher quality standard and is designed for integration into high-integrity, embedded applications.

## Design goals

Libcyphal's design goals should be mutually compatible. There is no expectation that any significant compromises are needed to achieve all goals given that the architecture should support making trade-offs at compile-time based on a user's build settings.

* **Full-Featured**

    This library provides a complete implementation of the specification including some of the application-level functions defined in section 5.3 of the specification.

* **Header-only, no dependencies, idiomatic**

    Libcyphal is provided as C++14 headers with no external dependencies. The headers do not impose styles or conventions on the user and make the maximum use of C++ templates to allow type flexibility when no specific type is optimal.

* **Deterministic by default**

    The parameterized types used should allow for deterministic time and memory complexity when used in a *default* configuration. At the same time, the user may provide non-default types that utilize unbounded memory and/or runtime complexity if desired.

* **Portable**

  * The library should not present any specific requirements to the underlying hardware or OS, and it must be coded in standard C++14 and be immediately forward compatible to at least C++20 while minimizing use of any deprecated constructs that may limit its compatibility with future versions of C++.
  * The library should make conservative use of C++ with an eye towards compatibility with coding standards like [Autosar](https://www.autosar.org/fileadmin/user_upload/standards/adaptive/17-03/AUTOSAR_RS_CPP14Guidelines.pdf), HIC++, and the [ISOCpp Core Guidelines](http://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines).
  * The library must build on as many different, standards-compliant, C++ compilers as is reasonable. At the time of this writing this includes gcc and clang.
  * The library must build for as many different architectures as is reasonable. At the time of this writing this includes arm, arm64, x86, and x86_64.

* **Powerful and Flexible**

    This library should enable the full power of Cyphal scaling up in capability with the target platform but remaining functional for even deeply embedded targets. Furthermore, the code should enable the full power of a given system allowing for use in high-performance and/or hard-realtime systems.

* **Clear/Fluent**

    While maximal in implementing the specification, libcyphal should be clearly structured and well-documented. It should prefer additional verbosity over compressed syntax and should utilize object-oriented principles to their greatest effect by creating logically named objects using well-known patterns.

* **Modular**

    The library should allow sequential adoption of its four layers: platform, media, transport, and application such that users can implement and test each layer in that sequence. The headers should allow compilers to elide the maximum amount of unused code such that the actual cost of the library in ROM is smaller when using fewer application features.

## Documentation

* [OpenCyphal website](http://opencyphal.org)
* [OpenCyphal forum](https://forum.opencyphal.org)

## Architecture

### High Level

The architecture of v1 is composed of three primary layers and one portability layer:

![libcyphal v1 block diagram](doc_source/images/html/block_diagram.png)

#### Util(ity)

Because of the design goal to provide "idiomatic C++," utilities used throughout the library should be minimal and non-trivial (i.e. avoid syntactic sugar). This layer shall contain purely logical constructs and *shall not* contain any abstract objects or anything requiring porting to a given platform. Ideally utilities are used only by libcyphal internals allowing users to write their own syntactic sugar using idioms natural to their codebase.

#### Platform

This layer must remain as minimal as possible ideally being implemented solely using C++ standard library functionality. This layer contains the objects and types used by all other layers when accessing system resources like memory, threads, or filesystems. Additionally this layer provides abstractions where architectural optimizations may be available for some conceivable target platform. For example, were the library to use memory in a way that could be optimized using SIMD instructions this layer would provide an abstraction such that SIMD optimizations could be enabled for an operation while providing a fallback where the optimization is not provided or is not available.

#### Media

*(In V0 this was known as the "driver" layer)* The media layer provides an abstraction on top of networking peripherals. It is distinct from the platform layer since this is a networking protocol that should make minimal, direct use of the underlying platform.

#### Transport

The OSI layer 4 implementation of Cyphal per section 4 of the specification. Uses types and serialization support provided by [Nunavut](https://github.com/OpenCyphal/nunavut). Note that this is where the "node" objects will be mapped to.

#### Application

Implementation of section 5 "Application" of the Cyphal specification.


### Threading model

The library should be single-threaded, not thread-aware.
Hence the API will be not thread-safe, which is OK as most applications will likely be running all of the Cyphal-related logic in one thread.

The documentation should provide advices about how to integrate the library in a multithreaded environment.

### Endianess

Cyphal is little-endian on the wire. Because of this big-endian platforms may use more CPU than the same code on a little-endian platform.

### Code generation

All code generation is performed by [Nunavut](https://github.com/OpenCyphal/nunavut).

### Folder Structure

**/libcyphal/include** - Contains the entire header-only libcyphal library.

**/libcyphal_validation_suite** – Test utilities provided to consumers of the library. These are public test fixtures and should be documented, maintained, and designed with the same care given to the rest of the library.

**/test/cmake** - A CMake meta-build system used to verify libcyphal. This is *not* a generalized build system provided for the library. *Libcyphal does not come with a build-system* as it is header-only and has minimal opinions about how it should be built.

**/test/native** - Unit-tests that validate the libcyphal library. These tests compile and execute using the build host's native environment. They also do not require any communication interfaces, virtual or otherwise, from the operating system and have no timing constraints.

**/test/ontarget** - Tests cross-compiled for specific hardware* and run on a set of dedicated test devices. These tests may have strict timing constraints and may require specific physical or virtual busses and other test apparatuses be present. Each on-target test will fully document its requirements to enable anyone with access to the appropriate hardware to reproduce the tests. Furthermore, these tests must be inherently automateable having clear pass/fail criteria reducible to a boolean condition.

**/test/compile** – Tests that run in the compiler. Most of these will be tests that pass if they fail to compile. For example, some tests will purposefully define template parameters that will cause static_asserts to fail. Tests that pass if they do compile are less interesting here since such happy paths are normally covered by unit-tests.

**/test/linux** - Tests that run on SocketCAN/Linux. These are provided partially as examples and partially to prove the library functions on a real platform.

### Test Environments

The following list of standardized* test environments will be used to validate the libcyphal implementation**:

1. **Posix** - We will produce examples that can run on top of SocketCAN on a recent version of an Ubuntu-based distro. While we expect that these examples will be generally compatible with other common GNU/Linux distros or POSIX compliant operating systems (that also support SocketCAN) we will compile and test the examples using Ubuntu as part of our CI build.
1. **Bare-metal on NXP S32K344** - The S32K344 provides a modern MCU with both Ethernet and CAN-FD support. While libcyphal should fit on smaller targets this is an ideal platform for developing and debugging.

> \* Libcyphal is a header only library suitable for a wide range of processors and operating systems. The targets and test environments mentioned here are chosen only as standardized test fixtures and are not considered more "supported" or "optimal" than any other platform.

## Library development

**Libcyphal development should be test-driven. Write the tests first.**

**Libcyphal source should be fluent. Comment everything in plain prose, build the docs with each change, read the docs to make sure your comments make sense.**

Despite the fact that the library itself can be used on virtually any platform that has a standard-compliant
C++14 compiler, the library development process assumes that the host OS is Linux or OSX.

### Prerequisites

We do provide toolchains-as-docker-containers (see next section) but if you want to support building and running the tests without docker you can install the following prerequisites on your development system:

* C++14 capable compiler (e.g. GCC, Clang)
* CMake 3.16+
* clang-format
* clang-tidy
* python3
* GNU make 4.1+

> *note* The first time you run cmake .. you will need an internet connection. The cmake build will cache all external dependencies in an `ext` folder for each build target and flavor. Subsequent builds can be performed offline.

Building the debug version and running the unit tests:

```bash
cd test
mkdir build
cd build
cmake ..
make -j8
make ARGS=-VV test
```

### Toolchains

We provide docker-based toolchains. If you want to use this locally either to verify that the CI build will succeed or just to avoid manually installing and maintaining the above dependencies then you can do:

```bash
docker pull uavcan/c_cpp:ubuntu-20.04

docker run --rm -v ${PWD}:/repo uavcan/c_cpp:ubuntu-20.04 /bin/sh -c ./test/ci/verify.py
```

To launch into an interactive shell in the container do:

```bash
docker run --rm -it -v ${PWD}:/repo uavcan/c_cpp:ubuntu-20.04
```

### Standards

Please adhere to [Autosar C++ guidelines](https://www.autosar.org/fileadmin/user_upload/standards/adaptive/17-03/AUTOSAR_RS_CPP14Guidelines.pdf).

### Style

Contributors, please follow the [Zubax C++ Coding Conventions](https://kb.zubax.com/x/84Ah) and always use `clang-format` when authoring or modifying files (the build scripts will enforce but not apply the rules in .clang-format).


## CAN bus Physical Layer Notes

This section is to help people new to working with physical CAN busses setup tools to verify and debug Cyphal at the physical layer.

### SocketCAN and CAN utils

On linux systems that support SocketCan you can install [CAN-utils](https://github.com/linux-can/can-utils) to get some nifty debug and test pattern generation tools. If you are using a probe that is supported by your kernel via SocketCAN (e.g. [Kvaser Leaf Pro HS v2](https://www.kvaser.com/product/kvaser-leaf-pro-hs-v2/) or [Peak Systems, PCAN-USB FD](https://www.peak-system.com/PCAN-USB-FD.365.0.html?L=1)) you can configure it using the `ip` and `tc` commands. For example, given a device `can0` you can configure it for 1Mb/4Mb using commands like thus:

```
sudo ip link set can0 down
sudo ip link set can0 type can bitrate 1000000 sample-point 0.875 dbitrate 4000000 dsample-point 0.75 fd on
sudo tc qdisc replace dev can0 root pfifo_fast
sudo ip link set can0 up
sudo ip -details link show can0
```
