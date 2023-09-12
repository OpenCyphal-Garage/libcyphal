# Libcyphal v1.0 contributor guidelines



## Design goals

As much as is reasonable, libcyphal mirrors [pycyphal](https://pycyphal.readthedocs.io/en/stable/pages/architecture.html) in the design of its APIs but its runtime is quite different. Specifically, where pycyphal makes excellent use of coroutines libcyphal does not require any specific execution model. Software utilizing coroutines, threads, super-loops, and even parallel execution models should all find libcyphal equally useful. This is achieved by an extreme lack of opinion on the matter by the implementation. For example, threading primitives are not used intrinsically but concurrency concerns are considered and documented. Coroutines are not required but providing flexibility in how and when functions are executed is built-in to the design. Finally, demonstrations ~~are~~ *will be* provided for some of these execution models to prove and illustrate this design tenet.

More abstractly, libcyphal's design goals should be mutually compatible. There is no expectation that any significant compromises are needed to achieve all goals given that the architecture should support making trade-offs at compile-time based on a user's build settings.

* **Full-Featured**

    This library provides a complete implementation of the specification including some of the application-level functions defined in section 5.3 of the specification.

* **Header-only, minimal dependencies, idiomatic**

    Libcyphal is provided as C++14 headers with external dependencies limited to sister, OpenCyphal projects like CETL, nunavut, libcanard, and libudpard. Any generic types or utilities are taken from the C++ standard library or CETL and every effort is made to minimize the need to use libcyphal itself outside of the messaging layers of a dependant program.

* **Deterministic by default**

    The parameterized types used should allow for deterministic time and memory complexity when used in a *default* configuration. At the same time, the user may provide non-default types that utilize unbounded memory and/or runtime complexity if desired.

* **Portable**

  * The library should not present any specific requirements to the underlying hardware or OS, it must be coded in standard C++14, and must be immediately forward compatible to at least C++20 while minimizing use of any deprecated constructs that may limit its compatibility with future versions of C++.
  * The library should make conservative use of C++ with an eye towards compatibility with coding standards like [Autosar](https://www.autosar.org/fileadmin/user_upload/standards/adaptive/17-03/AUTOSAR_RS_CPP14Guidelines.pdf), HIC++, and the [ISOCpp Core Guidelines](http://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines).

    > :orange_book: **NOTE**
    >
    > The use of templates is a contentious one in the field of high-criticality C++ as it is difficult to demonstrate appropriate test coverage of meta-code for a given target system. A possible compromise shall be explored where LLVM is extended to provide a template instantiation phase, somewhat like a code generator, that would yield concrete C++ source derived from the use of libcyphal and C++ standard library templates and the input to object file compilation. This future research may be part of libcyphal or may be provided by the CETL project which libcyphal utilizes.

  * The library must build on as many different, standards-compliant, C++ compilers as is reasonable. At the time of this writing this includes gcc, clang, and MSVC.
  * The library must build for as many different architectures as is reasonable. At the time of this writing this includes arm, arm64, x86, x86_64, and risc V.

* **Powerful and Flexible**

    This library should enable the full power of Cyphal scaling up in capability with the target platform but remaining functional for even deeply embedded targets. Furthermore, the code should enable the full power of a given system allowing for use in high-performance and/or hard-realtime systems.

* **Clear/Fluent**

    While maximal in implementing the specification, libcyphal should be clearly structured and well-documented. It should prefer additional verbosity over compressed syntax and should utilize object-oriented principles to their greatest effect by creating logically named objects using well-known patterns.

* **Modular**

    The library should allow sequential adoption of its four layers: platform, media, transport, and application such that users can implement and test each layer in that sequence. The headers should allow compilers to elide the maximum amount of unused code such that the actual cost of the library in ROM is smaller when using fewer application features.

    The transport layer of libcyphal is implemented by "the 'ards"; libcanard for CAN, libudpard for UDP, libserard for serial, etc. Ards are C implementations that provide the smallest possible amount of code needed to create a Cyphal end system. Libcyphal builds on top of these to provide the code needed to support most, if not all, of the features defined by the Cyphal specification. Because of this, and in keeping with the modular design tenet, any user that has a functional 'ard should be able to utilize it along with their existing media layer (the layer below the 'ard's transport layer) underneath libcyphal presentation layer objects.

## Documentation

* [OpenCyphal website](http://opencyphal.org)
* [OpenCyphal forum](https://forum.opencyphal.org)

# Developer Environment

## Folder Structure

```
+ build*/                   <-- reserved (and git-ignored) for build output.
|
+ cmake/                    <-- cmake build system stuff common to all sub-directories.
|
+ demonstration/            <-- fully functional applications to demonstrate use of
|                               libcyphal on various target systems.
|
+ docs/                     <-- doxygen and example code source
|   + CMakeLists.txt        <-- doxygen and example code build
|   + examples/             <-- Example code to include in doxygen (also built to verify
|                               its validity)
|
+ external/                 <-- external project mount-point. Sub-folders are not
|                               included in git history.
|
+ libcyphal/
|   + CMakeLists.txt        <-- builds a cmake module to support inclusion as an
|                               ExternalProject. Also supports building various
|                               release formats.
|   + include/              <-- where the magic happens!
|   + validation/           <-- parametrized googletest tests for building a
|                               platform-specific validation suite for libcyphal.
|
| test/
|   + unittest/             <-- googletest unit tests of libcyphal code.
|   + compile/              <-- compile-time tests of libcyphal code.
|
+ .github/                  <-- github actions CI
+ .devcontainer/            <-- vscode/github developer container integration
|                               (OpenCyphal toolshed image)
+ .vscode/                  <-- common vscode development settings
```

### Demonstrations

This is a list that will move to an external project and road-map at some point. We capture it here and now to help guide the design of the project when anticipating possible use cases by end-systems. While most of this list is speculative, `linux-posix` will be the first demonstration developed and will be completed along with the first release of libcyphal.

```
+ demonstration/            <-- Platform-specific demonstration software. In the future
|   |                           this will be external and will stay in the garage. We'll
|   |                           keep it here for now to accelerate development.
|   |
|   + linux-posix/          <-- Demonstrates using libcyphal in a single, multi-threaded
|   |                           linux application.
|   |   + CMakeLists.txt
|   |   + validation/       <-- reification of libcyphal/validation for linux
|   |
|   + linux-coroutine/      <-- Demonstrates using libcyphal with C++20 coroutines.
|   |   + CMakeLists.txt
|   |
|   + linux-amp/            <-- Demonstrates using libcyphal on a multi-core system using
|   |                           openAMP to coordinate work between cores.
|   |   + CMakeLists.txt
|   |
|   + mac/                  <-- Love to have this. not sure what the priority is though.
|   |   + CMakeLists.txt
|   |   + validation/       <-- reification of libcyphal/validation for mac
|   |
|   + windows/              <-- contributions welcome!
|   |   + CMakeLists.txt
|   |   + validation/       <-- reification of libcyphal/validation for windows
|   |
|   + freeRTOS/             <-- at some point this is important given how many MCUs it
|   |                           unlocks.
|   |   + CMakeLists.txt
|   |
|   + [on-metal]/           <-- TDB bare-metal firmware demonstration to show that
|   |                           libcyphal works on a resource-constrained target without
|   |                           preemption.
|   |   + CMakeLists.txt
|   |   + validation/       <-- reification of libcyphal/validation for [TBD]
|   |
|   + [other]/ contributions welcome!
|       + CMakeLists.txt
|       + validation/       <-- reification of libcyphal/validation for [other]
|

```
## Cmake Variables

 * **`LIBCYPHAL_ROOT`** - The absolute path to the root of the libcyphal project. This is different then `CMAKE_SOURCE_DIR` when invoking a CMakeLists.txt file in a subdirectory of libcyphal.
 * **`LIBCYPHAL_FLAG_SET`** - The absolute path to the cmake defining a set of compiler flags for the build. These are located under `{CMAKE_SOURCE_DIR}/cmake/compiler_flag_sets`.
 * **`LIBCYPHAL_INCLUDE`** - Convenience path that is just `${LIBCYPHAL_ROOT}/libcyphal/include` but formed using [`cmake_path`](https://cmake.org/cmake/help/latest/command/cmake_path.html).
 * **`LIBCYPHAL_VERSION`** - [Semantic versioning](https://semver.org/) number for the libcyphal build. Set to `0.0.0` where the version cannot be determined.
 * **`LIBCYPHAL_KIT_NAME`** - The name of a build-kit in effect. This is currently only set by `.vscode/cmake-kits.json` and is for logging purposes only.
 * **`LIBCYPHAL_TEST_CPP_STANDARD`** - One of `14`, `17`, or `20` to allow switching the C++ standard in use for a given build regardless of the `LIBCYPHAL_FLAG_SET` in use.
 * **`LIBCYPHAL_TEST_DISABLE_CPP_EXCEPTIONS`** - If `on`, and if the toolchain supports it, the build will ask the compiler to disable all C++ exception support.
    > :orange_book: **NOTE**
    >
    > There is no switch for RTTI because libcyphal does not utilize RTTI for any configuration. When exceptions are enabled libcyphal may use them to integrate with applications that expect them but, internally, libcyphal objects are fully functional without exceptions.

 * **`LIBCYPHAL_INTROSPECTION_TRACE_ENABLE`** - When `on` macros in the `libcyphal/introspection.hpp` header are enabled to produce output to standard out.
* **`CMAKE_BUILD_TYPE`** - Three types are supported: "Release", "Debug", and "Coverage.

 ## Defines
 * **`LIBCYPHAL_INTROSPECTION_ENABLE_ASSERT`** -Enables runtime assertions within libcyphal headers that should be enabled in unittests.
 * **`CETL_ENABLE_DEBUG_ASSERT`** - Inherited from [CETL](https://github.com/OpenCyphal/CETL), defining this enables runtime assertions within the CETL library that should be enabled in unittests.

## ![visual-studio code](.vscode/vscode-alt.svg#gh-dark-mode-only) ![visual-studio code](.vscode/vscode.svg#gh-light-mode-only)
We support the vscode IDE using
[cmake](https://github.com/microsoft/vscode-cmake-tools/blob/main/docs/README.md) and
[development containers](https://containers.dev/). Simply clone libcyphal, open the
repo in vscode and do "reopen in container" to automatically pull and relaunch
vscode in the provided devcontainer.

## Command-Line Workflow

If you don't want to use vscode you can pull our [Toolshed devcontainer](https://github.com/OpenCyphal/docker_toolchains/pkgs/container/toolshed)
and manually run it.

### TLDR
```
docker pull ghcr.io/opencyphal/toolshed:ts22.4.3
git clone {this repo}
cd {this repo}
docker run --rm -it -v ${PWD}:/repo ghcr.io/opencyphal/toolshed:ts22.4.3
mkdir build
cd build
cmake ..
cmake --build .
```

> :orange_book: **NOTE**
>
> While cmake isn't required to use libcyphal it will be supported as a first-class way to consume the library including full support for `cmake --install` and for using the libcyphal repository as a cmake external project.

### Standards

Please adhere to [Autosar C++ guidelines](https://www.autosar.org/fileadmin/user_upload/standards/adaptive/17-03/AUTOSAR_RS_CPP14Guidelines.pdf).

### Style

Contributors, please follow the [Zubax C++ Coding Conventions](https://kb.zubax.com/x/84Ah) and always use `clang-format` when authoring or modifying files (the build scripts will enforce but not apply the rules in .clang-format).

### Pull-Request Checklist

Reviewers, please check the following items when reviewing a pull-request:

> **NOTE:** This is just the start of this checklist. Expect it to grow and get refined as this project matures.

1. **correctness**
    * Is the code correct.
2. **clarity**
    * Is the code easily understood?
    * It is overly complex?
3. **test coverage**
    * Were tests written to cover the changes?
4. **test effectiveness and correctness**
    * Are the tests good tests that provide some guarantee that the logic is, and will remain, correct?
5. **documentation**
    * Is the code properly documented?
    * Are there changes needed to auxillary documentation that is missing?
    * Are there good examples for how to use the code?
6. **design**
    * Is the code maintainable?
    * Are the tests maintainable?
    * Is the code in the right namespace/class/function?


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
