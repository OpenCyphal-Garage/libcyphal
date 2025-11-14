![Cyphal](docs/images/html/opencyphal_logo.svg) Cyphal stack in C++
===================

[![Build Status](https://github.com/OpenCyphal-Garage/libcyphal/actions/workflows/tests.yml/badge.svg)](https://github.com/OpenCyphal-Garage/libcyphal)
[![Forum](https://img.shields.io/discourse/https/forum.opencyphal.org/users.svg)](https://forum.opencyphal.org)
[![Sonarqube Badge](https://sonarcloud.io/api/project_badges/measure?project=OpenCyphal-Garage_libcyphal&metric=alert_status)](https://sonarcloud.io/project/overview?id=OpenCyphal-Garage_libcyphal)
[![Sonarqube Coverage](https://sonarcloud.io/api/project_badges/measure?project=OpenCyphal-Garage_libcyphal&metric=coverage)](https://sonarcloud.io/project/overview?id=OpenCyphal-Garage_libcyphal)
[![Documentation](https://img.shields.io/badge/docs-passing-green.svg)](https://opencyphal.org/libcyphal/)

> **WARNING** libcyphal v1 is not yet complete. This is a work-in-progress.

Portable reference implementation of the [Cyphal protocol stack](https://opencyphal.org) in C++ for embedded systems, Linux, and POSIX-compliant RTOSs.

Cyphal is a lightweight protocol designed for reliable communication in aerospace and robotic applications over robust vehicular networks.

## Building

You don't need to build LibCyphal to use it since this is a header-only library. You will need to build the transport libraries though (libcanard, libudpard, etc), which is covered in their respective documentation.

If you want to build libcyphal for development purposes, you may use containerized toolchains as covered in CONTRIBUTING.md. Otherwise, you may want to disable static analysis:

```shell
mkdir build
cd build
cmake .. -DNO_STATIC_ANALYSIS=1
make -j16
```
