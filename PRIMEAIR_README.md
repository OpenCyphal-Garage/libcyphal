Copyright 2023 Amazon.com Inc. and its affiliates. All Rights Reserved.

# AdnLibcyphal
This package is meant for internal development of the third party package libcyphal. For a more detailed overview of what libcyphal is and does, check out [this link](https://github.com/OpenCyphal-Garage/libcyphal);

## Branches

### Mainline Branch
Mainline is intended to be the staging area for commits that will be done via pull request to the Github repo. This should be a scrubbed code area where any internal references and internal tools are stripped away from the brazil branch.

### Brazil Branch
The brazil branch is the internal repo that other packages currently take a dependency on. There should be future talks whether to move this to AdnExLibcyphal like many other packages once the upstream Github repo has the stability and libcyphal isn't going through constant changes. i.e. a good time to do the switch over is when libcyphal is promoted out of the Cyphal Garage and into the Cyphal open source repo.

### Development Branch
This branch is intended to be used for development and to try out new features and deployments. Cut CR's against this branch and do merges to the brazil branch as needed. 

## Layout

### libcyphal/include
This folder has the source code for libcyphal all contained as header files

### libcyphal/include/libcyphal/build_config.hpp
https://code.amazon.com/packages/AdnLibcyphal/blobs/heads/brazil/--/libcyphal/include/libcyphal/build_config.hpp

Several parameters are very device specific, and thus for now I placed them as "build_config" parameters you can set in CMake. Here is a list of the build_config parameters and examples of them being used:

[*LIBCYPHAL_TRANSPORT_MAX_MESSAGE_SIZE_BYTES*](https://code.amazon.com/packages/AdnLibcyphal/blobs/97560ea95f5905f7d064c69eb245904832b96a4e/--/libcyphal/include/libcyphal/transport/message.hpp#L17) - Controls the max size of a message expected which currently sets the buffer size of a message, and also  for Linux, we are setting the heap size based on max message size.

[*LIBCYPHAL_TRANSPORT_MAX_BROADCASTS*](https://code.amazon.com/packages/AdnLibcyphal/blobs/97560ea95f5905f7d064c69eb245904832b96a4e/--/libcyphal/include/libcyphal/transport/udp/cyphal_udp_transport.hpp#L33-L34) - Buffer that holds maximum number of broadcast messages

[*LIBCYPHAL_TRANSPORT_MAX_SUBSCRIPTIONS*](https://code.amazon.com/packages/AdnLibcyphal/blobs/97560ea95f5905f7d064c69eb245904832b96a4e/--/libcyphal/include/libcyphal/transport/udp/cyphal_udp_transport.hpp#L35-L36) - Buffer that holds maximum number of subscriptions

[*LIBCYPHAL_TRANSPORT_MAX_RESPONSES*](https://code.amazon.com/packages/AdnLibcyphal/blobs/97560ea95f5905f7d064c69eb245904832b96a4e/--/libcyphal/include/libcyphal/transport/udp/cyphal_udp_transport.hpp#L37-L38) - Buffer that holds maximum number of service request responses

[*LIBCYPHAL_TRANSPORT_MAX_REQUESTS*](https://code.amazon.com/packages/AdnLibcyphal/blobs/97560ea95f5905f7d064c69eb245904832b96a4e/--/libcyphal/include/libcyphal/transport/udp/cyphal_udp_transport.hpp#L39-L40) - Buffer that holds maximum number of service request

[*LIBCYPHAL_TRANSPORT_MAX_FIFO_QUEUE_SIZE*](https://code.amazon.com/packages/AdnLibcyphal/blobs/97560ea95f5905f7d064c69eb245904832b96a4e/--/libcyphal/include/libcyphal/transport/udp/cyphal_udp_transport.hpp#L450) - This should probably be renamed to TX_FIFO_QUEUE... But this is the buffer size of how many frames it can hold when transmitting a message. So if you have 1MB message, with a MTU of 1400, it'll get split into ~715 frames which means you'll need a minimum of 715 for this value. 

### libcyphal/include/libcyphal/media
This folder contains the media layer for libcyphal. This has the frame definitions for UDP and CAN. There have been talks to move this as a subset of the "transport" namespace and used as a part of transport instead of as a top level media layer.

### libcyphal/include/libcyphal/node
This still needs to be implemented and contains the application layer for libcyphal. It has the Cyphal node relevant components.

### libcyphal/include/libcyphal/platform
This still needs to be integrated into libcyphal and currently has "memory.hpp" which can be used as a generic memory pool for libcyphal

### libcyphal/include/libcyphal/templates
I have not done enough research as to how to use the files in this folder

### libcyphal/include/libcyphal/transport
Has all the transport layer libcyphal work. Most notably:

### libcyphal/include/libcyphal/transport/can/cyphal_can_transport.hpp
### libcyphal/include/libcyphal/transport/udp/cyphal_udp_transport.hpp
These contain the bulk of the logic for the libcyphal transport layer handling all the transport level tasks for libcyphal.

### libcyphal/include/libcyphal/transport/can/session
### libcyphal/include/libcyphal/transport/udp/session
Contains the session layer APIs for CAN/UDP. This keeps track of things like sockets, source/destination addresses/ports, information.

### libcyphal_validation_suite
Unit tests for "libcyphal" should all go here. This is different than the "test" folder which contains tests and platform specific implementations for libcyphal, and also tests for running on CI.

### test/linux/libcyphal
Contains the source files for a Linux implementation of libcyphal. This contains OS specific implementation for things like UDP using socket.h, CAN using socketcan.h. 

This currently gets built automatically on the "brazil" branch of libcyphal under libcyphal_posix and can be imported to currently run wrappers around the transport layer for use in Linux systems.

### test/linux/libcyphal/include/transport/ip/v4/connection.hpp
Contains UDP implementation using socket.h. This is the file that does the actually interfacing to the Linux available APIs for UDP including creating a socket, binding, multicasting etc.

### test/linux/libcyphal/include/transport/can/connection.hpp
Contains CAN implementation using socketcan.h. This is the file that does the actually interfacing to the Linux available APIs for CAN including creating a socket, binding, multicasting etc.

### test/linux/libcyphal/include/wrappers
Contains wrappers around the transport layer for libcyphal. This allows the user to send CAN/UDP without having to setup a Cyphal node. This is a somewhat pseduo application layer for users and abstracts away things like providing the Linux Timer, Heap, Transport objects, etc. If finer control is needed, you can use this as a reference and initialize these objects directly. Some of this code may end up being a part of the libcyphal application layer later on.

### test/linux/libcyphal/build_config.hpp
POSIX specific build configurations and compile time control. Currently the following variables are available to configure:

[*LIBCYPHAL_TRANSPORT_MAX_HEAP_SIZE*](https://code.amazon.com/packages/AdnLibcyphal/blobs/97560ea95f5905f7d064c69eb245904832b96a4e/--/test/linux/libcyphal/include/posix/libcyphal/wrappers/can/base.hpp#L32) - Currently set to 2 * the Max message size. This is the total HEAP available for libcyphal. Adjust according to available HEAP and reference https://github.com/OpenCyphal-Garage/libudpard/blob/main/libudpard/udpard.h#L502-L539 for size calculations

[*POSIX_LIBCYPHAL_UDP_RX_BUFFER_SIZE_BYTES*](https://code.amazon.com/packages/AdnLibcyphal/blobs/534ada359e25f0fdf494c20e04bc7aa968d74b05/--/test/linux/libcyphal/include/posix/libcyphal/transport/udp/session/input_session.hpp#L163) - The UDP buffer size that allows the collection of transmission frames to wait for a receive call to drain the buffer. This is in direct correlation and should be set to the OS RX buffer defined in /etc/sysctl.conf under net.core.rmem_max

[*POSIX_LIBCYPHAL_TRANSMIT_TIMEOUT_US*](https://code.amazon.com/packages/AdnLibcyphal/blobs/534ada359e25f0fdf494c20e04bc7aa968d74b05/--/test/linux/libcyphal/include/posix/libcyphal/transport/ip/v4/connection.hpp#L38) - How much time to wait to transmit a UDP frame. 0 for non-blocking

[*POSIX_LIBCYPHAL_RECEIVE_TIMEOUT_US*](https://code.amazon.com/packages/AdnLibcyphal/blobs/534ada359e25f0fdf494c20e04bc7aa968d74b05/--/test/linux/libcyphal/include/posix/libcyphal/transport/ip/v4/connection.hpp#L40) - How much time to wait to receive a UDP frame. 0 for non-blocking

### test/linux/libcyphal/can.hpp
Helper header for the CAN interface to import to access user facing APIs

### test/linux/libcyphal/udp.hpp
Helper header for the UDP interface to import to access user facing APIs

### test/linux/libcyphal/types
OS specific implementations of generic types such as Timer, Heap, Time.

# Tracking Work
## Previous Work For HILSIM First Flight
https://jira.adninfra.net/browse/AF-2602

## Work Remaining
https://jira.adninfra.net/browse/AF-2389



# Merging with Mainline

## Checklist
* Make sure to use the open source CMakeLists.txt and not the one in the `brazil` branch. The brazil branch CMakeLists.txt contains the brazil macros which are not available outside of Amazon.

* Remove internal only files such as:
  * ContainerBuild.env
  * test/linux/libcyphal/CMakeLists.txt
  * PRIMEAIR_README.md

* Grep for any JIRA todo mentions. Example: AF-#### in code
* Run some sort of keyword checker. i.e. we wouldn't want any mentions of prime air or AF or VSDK in this codebase
* Make sure to run the `.clang-format` from open source

# Formatting
Use the [.clang-format](https://github.com/OpenCyphal-Garage/libcyphal/blob/main/.clang-format) included in GitHub
To run:
```
find . -name "*.hpp" -exec clang-format -style=file -i {} \;
```

# Testing
A good way to test libcyphal is to use the wrongly named [AdnLibudpardIntegrationTester](https://code.amazon.com/packages/AdnLibudpardIntegrationTester/trees/brazil).

## Setup
```
brazil ws create --name LibcyphalWorkspace
cd LibcyphalWorkspace
brazil ws use -p AdnLibcyphal -p AdnLibudpardIntegrationTester --branch brazil --versionset AdnCX3Pilot/cyphal-migration
cd src/AdnLibcyphal
brazil-build release
cd ../AdnLibudpardIntegrationTester
brazil-build release
```
## Run Test
### UDP
**Terminal #1:**

`cd LibcyphalWorkspace/src/AdnLibudpardIntegrationTester && LD_LIBRARY_PATH=$LD_LIBRARY_PATH:~/workspace/LibcyphalWorkspace/src/AdnLibcyphal/lib ./build/bin/udp_broadcaster`

**Terminal #2:**

`cd LibcyphalWorkspace/src/AdnLibudpardIntegrationTester && LD_LIBRARY_PATH=$LD_LIBRARY_PATH:~/workspace/LibcyphalWorkspace/src/AdnLibcyphal/lib ./build/bin/udp_receiver 127.0.1.2`


### CAN

#### Recommended Setup
Virtual CAN is useful when testing CAN related work. It's a fairly simple setup.

**Dev Desktop:**
```
git clone https://github.com/linux-can/can-utils.git
cd can-utils
./autogen.sh
./configure
make
make install (with root privileges)
```

**Ubuntu**

`sudo apt-get install can-utils`

#### Virtual Can Setup
```
sudo ip link set up vcan0
sudo ip link add dev vcan0 type vcan
```

#### Quick Test
**Terminal #1:**

`candump vcan0`

**Terminal #2:**

`cansend "vcan0" "123#1122334455667788"`

#### Run Test

**Terminal #1:**

`cd LibcyphalWorkspace/src/AdnLibudpardIntegrationTester && LD_LIBRARY_PATH=$LD_LIBRARY_PATH:~/workspace/LibcyphalWorkspace/src/AdnLibcyphal/lib ./build/bin/can_broadcaster vcan0`

**Terminal #2:**

`cd LibcyphalWorkspace/src/AdnLibudpardIntegrationTester && LD_LIBRARY_PATH=$LD_LIBRARY_PATH:~/workspace/LibcyphalWorkspace/src/AdnLibcyphal/lib ./build/bin/can_receiver vcan0`

## Other Testing Options
There are other test setups like LIO you can use to test: (example on LIO)[https://code.amazon.com/packages/AdnVehicleLioEdgeNode/trees/mainline]


