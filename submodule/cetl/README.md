![OpenCyphal](cetlvast/suites/docs/images/html/cetl_logo.svg#gh-light-mode-only) \
![OpenCyphal](cetlvast/suites/docs/images/html/cetl_logo_dark.svg#gh-dark-mode-only) \
Cyphal Embedded Template Library
===================

> We like to pronounce CETL as "settle"

[![Forum](https://img.shields.io/discourse/https/forum.opencyphal.org/users.svg)](https://forum.opencyphal.org)
[![Documentation](https://opencyphal.github.io/CETL/docs-ok-green.svg)](https://opencyphal.github.io/CETL/)

[![CodeQL](https://github.com/OpenCyphal/CETL/actions/workflows/codeql.yml/badge.svg)](https://github.com/OpenCyphal/CETL/actions/workflows/codeql.yml)
[![CETLVaSt](https://github.com/OpenCyphal/CETL/actions/workflows/cetlvast.yml/badge.svg)](https://github.com/OpenCyphal/CETL/actions/workflows/cetlvast.yml)
[![Open Bugs](https://img.shields.io/github/issues/OpenCyphal/CETL/bug?label=bugs&logo=github)](https://github.com/OpenCyphal/CETL/issues?q=is%3Aopen+is%3Aissue+label%3Abug)

[![Quality Gate Status](https://sonarcloud.io/api/project_badges/measure?project=OpenCyphal_CETL&metric=alert_status)](https://sonarcloud.io/summary/new_code?id=OpenCyphal_CETL)
[![Coverage](https://sonarcloud.io/api/project_badges/measure?project=OpenCyphal_CETL&metric=coverage)](https://sonarcloud.io/summary/new_code?id=OpenCyphal_CETL)
[![Security Rating](https://sonarcloud.io/api/project_badges/measure?project=OpenCyphal_CETL&metric=security_rating)](https://sonarcloud.io/summary/new_code?id=OpenCyphal_CETL)


## include

The include directory contains the CETL headers all within a folder, "cetl". Types found under "cetl/pfXX" folders and
within `cetl::pfxx` namespaces are ["polyfill"](https://en.wikipedia.org/wiki/Polyfill_(programming)) types that adhere
to the XX standard. For example, `cetl::pf20::span` is a type that adheres, as closely as possible, to the C++20
specification for the `std::span` type.

> Any type found directly under the cetl folder does *not* adhere to a known standard and is specific only to CETL.

Also under each "cetl/pfXX" folder will be a "cetlpf.hpp" header. These headers enable automatic polyfill behavior but do
so by violating certain AUTOSAR-14 rules. We recommend using the CETL polyfill types directly in code that adheres
to AUTOSAR-14.

There are currently three ways to take a dependency on CETL (see [issue #9](https://github.com/OpenCyphal/CETL/issues/9)
for a future 4th way):

1. **Download the [release zip/tar from github](https://github.com/OpenCyphal/CETL/releases)** – This, of course, provides
no strong link back to the CETL source and test artifacts associated with it but you have full control over it and can
do whatever you like within the terms of the [LICENSE](./LICENSE) including deleting everything except the one or two
types you wanted to use.

2. **Git Submodules** – Including CETL as a submodule is perfectly acceptable however you'll also get cetlvast which
might be annoying if you are looking for the bare minimum set files from CETL. It does provide a seamless update mechanism
and is a fairly well-supported practice.

3. **Filtering** – By creating a fork of CETL and using [git-filter-repo](https://github.com/newren/git-filter-repo/)
you can create a one-way transform from CETL to a branch in your fork that is filtered to include only the files and
history you want to depend on:
    ```bash
    git-filter-repo --path-rename include/: --path-regex "^include/LICENSE|^include/cetl/cetl.hpp|^include/cetl/pf20/span.hpp"
    ```
    While this may seem the same as just downloading the source and picking the files you want to use the technique
    does maintain the relevant history for the remaining files and you can repeat the process in an automated fashion
    for any subsequent CETL patches or releases.

## CETLVaSt

> Staying with the theme, you can call this "settle-vast"

We recommend you build the CETL VerificAtion SuiTe using your target toolchain and run the suite on
your target hardware to ensure it is fully compatible. The suite is designed to work with minimal
platform I/O and does not require a filesystem. On thicker platforms, like linux, CETLVaSt can be
used to generate more robust output and even coverage reports to verify that the test suite is
covering the entire set of CETL types.

## Project Design Tenets

- **CETL supports C++14 and newer** – It is not a C++98 compatibility library, it is not a C++11 compatibility library,
and it reserves the right to increase the base support over the years as the C++ language evolves.
- **CETL does not supplant STL, ETL, boost, or any other full-featured C++ support library** – It is not a general-purpose
C++ support library and is limited to the minimum set of types needed to ensure OpenCyphal C++ projects are agnostic to
these larger projects and are easy to integrate with.
- **Where CETL types provide backwards compatibility, they should support direct replacement by newer concepts** – Where
CETL types provide functionality found in newer C++ standards the CETL version will prefer mimicking the standard over
mimicking other support libraries like Boost.
- **CETL types will never _require_ use of the default STL heap** – They may allow use of generalized heap memory and/or
the default STL allocator but will always support an alternative way to manage their memory.
- **CETL minimizes type aliasing and never injects typedef or macros into external namespaces.** – If an `std::uint8_t`
will suffice CETL uses that explicitly. If a function should be constexpr the constexpr keyword will be used. etc.
- **CETL tries really, really hard to not use macros** – Except where AUTOSAR-14 Rule A16-0-1 permits, CETL does
not use any C macros where a C++ template or other construct will suffice.
- **CETL is [Autosar C++14](https://www.autosar.org/fileadmin/standards/adaptive/20-11/AUTOSAR_RS_CPP14Guidelines.pdf)
compliant** – Where it violates Autosar rules comments will provide a clear rationale.
- **CETL headers have minimal dependencies** – While there is a `cetl/cetl.hpp` it is minimal and does not drag a large
set of conventions, typedefs, and other constructs that will pollute your code. Each type provided is isolated as much
as practical and users that want to copy and paste one of the CETL headers into their project can easily elide cetl.hpp
with minimal effort.


![OpenCyphal](cetlvast/suites/docs/images/html/opencyphal_logo_dark.svg#gh-dark-mode-only)\
![OpenCyphal](cetlvast/suites/docs/images/html/opencyphal_logo.svg#gh-light-mode-only)
