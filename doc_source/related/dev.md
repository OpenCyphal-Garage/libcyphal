Libuavcan Library Developer Guide {#LibDevGuide}
====================================================

> This is a guide for developers of libuavcan itself. If you are a user of libuavcan you don't need to read this, but hey; Thanks for using our software.
-----------------------------------------------------
> And now...on to the sausage making!

## Build System

We require [cmake 3.6](https://cmake.org/cmake/help/v3.6/) or greater because that's what Ubuntu 18.04 ships with and Ubuntu 18.04 is the reference operating system we as our generic build and test environment. At the time of this writing we have only tested the GNU Makefile generator but there's no reason why ninja shouldn't work either.

The default cmake setup and build should always work:

```
mkdir build
cd build
cmake ..
make -j8 && echo "this is fine."
```

To run the same commands as the automated builds use the scripts under the `ci/` folder. These create subdirectories under the project root folder using the following rules:

```
build_{build_type}_{build_type qualifier}
build_ext_{build_type}_{build_type qualifier}
```

For example:
```
./ci/native-gcc-build-and-test.sh
# will create and build using two directories
build_native_gcc
build_ext_native_gcc
```

The naming for these files and the folders they create follow this pattern:

```
[build_type]-[(optional)build_type qualifier]-[build|test|report|upload].sh
```

Because of these rules, you can super-clean your package directory with `rm -rf build*` (or `git rm -dfx` but that will also blow away other things you might still want).

### build_ext

build_ext in folder prefixes stands for "external build". The cmake build will pull some dependencies from the internet including from github (for googletest) and Pypi. These downloads and the builds for these downloaded projects are stored under this "ext" folder. If you `rm -rf` just the build directory you can recreate and build without downloading anything again.

## Developer Environment

> **TODO** (hint: it'll be about vscode since it's good and it's free)

## OSX

Where we use linux SocketCAN for examples you will need to build and run on a linux machine. To make this easier on
OSX developers we provide a [Vagrantfile](https://www.vagrantup.com/) in this document that pulls an Ubuntu image
for [VirtualBox](https://www.virtualbox.org/) which has the SocketCAN kernel modules and can-utils installed. This image
is maintained by [thirtytwobits](https://app.vagrantup.com/thirtytwobits/boxes/libuavcan_v1)) so it's not necessarily
part of the libuavcan toolchain but it should work. After you provision this image (i.e. `vagrant up`) you can attach
USB CAN probes directly to it or you can setup `vcan` links which will allow the libuavcan linux examples to run normally.

### Vagrantfile

```
# -*- mode: ruby -*-
# vi: set ft=ruby :

# We use docker as our reference build environment but this Vagrant file is handy
# if you need to test real SocketCAN devices on OSX.
Vagrant.configure("2") do |config|
  
    config.vm.box = "thirtytwobits/libuavcan_v1"
    config.vm.box_version = "1.0"

    config.vm.provider :virtualbox do |v|
      v.customize [ "guestproperty", "set", :id, "/VirtualBox/GuestAdd/VBoxService/--timesync-set-threshold", 10000 ]
    end 
    config.vm.provision "shell" do |s|
      s.inline = <<-SCRIPT
        # Change directory automatically on ssh login
        echo "export CXX=\"g++-7\" CC=\"gcc-7\"" >> /home/vagrant/.bashrc
        echo "cd    /vagrant" >> /home/vagrant/.bashrc
      SCRIPT
    end
  end
```

or just do:

```bash
vagrant init thirtytwobits/libuavcan_v1 --box-version 1.0
vagrant up
```

### Vagrant Cheatsheet

```
vagrant up
vagrant ssh
```

Attach a CAN probe to the guest using VBoxManage:

```
# first, find your vagrant guest.
VBoxManage list vms

# next, find the UUID of the usb device on the host. In this example
# we look for a PCAN probe:
VBoxManage list usbhost | grep --context=7 PCAN

# now, attach the usb device to the guest.
VBoxManage controlvm my_libuavcan_vagrant_guest_3249032840 usbattach a2d153de-63f1-411d-08d2-eefc42b792fa

# You can also setup a filter so this device will be auto-captured:
VBoxManage usbfilter add 0 --target my_libuavcan_vagrant_guest_3249032840 --name PEAK --action hold --product "PCAN-USB Pro FD"
```

## Linux

> TODO: document common SocketCAN setups for running linux examples.

Libuavcan Library Developer Continuous Integration {#LibCIGuide}
====================================================================

> TODO buildkite documentation

## Pi worker

### Hardware

1. [Rasberry PI 3 B+](https://www.newark.com/raspberry-pi/rpi3-modbp/sbc-arm-cortex-a53-1gb-sdram/dp/49AC7637) with 3A power supply.
2. 16GB SDHC class 4 card.

### Software

Raspbian Stretch Lite

- Version:April 2019
- Release date:2019-04-08
- Kernel version:4.14
- SHA-256: 03ec326d45c6eb6cef848cf9a1d6c7315a9410b49a276a6b28e67a40b11fdfcf

### Post Install Steps

```bash
sudo raspi-config
```

1. Change the default password. Keep this password to yourself.
2. Enable SSH
3. Expand the filesystem to use the entire SD card.
4. Change the hostname to something useful and unique (this will appear as the agent name in buildkite. It must be changed from the default).

```bash
sudo apt update
sudo apt upgrade -y
sudo apt install git
sudo apt install -y python3-pip
git clone https://github.com/thirtytwobits/nanaimo.git
cd nanaimo
pip3 install --system .
```

#### [Segger JLink](https://www.segger.com/downloads/jlink)

Download the J-Link software for Linux ARM systems.

```bash
scp path/to/JLink_Linux_VXXX_arm.tgz pi@[ip address]:~
ssh pi@[ip address]
tar -xvf JLink_Linux_VXXX_arm.tgz
mv JLink_Linux_VXXX_arm /opt/
cd /opt/JLink_Linux_VXXX_arm
sudo cp 99-jlink.rules /etc/udev/rules.d/
sudo ln -s /opt/JLink_Linux_VXXX_arm/JLinkExe /bin/JLinkExe
```
modify `/boot/config.txt` adding the following:

```bash
max_usb_current=1
```

and restart.

```bash
sudo shutdown -r now
```

#### Build Kite Agent

(taken from the [buildkit agent for Debian instructions](https://buildkite.com/docs/agent/v3/debian))

```bash
# Add the buildkit repo
sudo apt-get install -y apt-transport-https dirmngr
sudo apt-key adv --keyserver ipv4.pool.sks-keyservers.net --recv-keys 32A37959C2FA5C3C99EFBC32A79206696452D198
ssh-keygen -t rsa -b 4096 -C "[hostname]@uavcan.org"
"echo deb https://apt.buildkite.com/buildkite-agent stable main" | sudo tee /etc/apt/sources.list.d/buildkite-agent.list

# Install the Agent
sudo apt-get update && sudo apt-get install -y buildkite-agent

```

At this point you'll need to obtain a worker token from a member of the uavcan organization. We will generate a token that is for your fleet of pi's only and reserve the right to revoke that key if we feel your workers are misbehaving.

Buildkite administators: see [this post](https://forum.buildkite.community/t/multiple-agent-tokens-per-org-with-agent-queue-restrictions/143/3) for how to generate a new token.

Once you have this token you can complete the agent installation:

```bash
# Add the libuavcan token
sudo sed -i "s/xxx/INSERT-YOUR-AGENT-TOKEN-HERE/g" /etc/buildkite-agent/buildkite-agent.cfg

# Start the service
sudo systemctl enable buildkite-agent && sudo systemctl start buildkite-agent
```

You can tail the logs using journalctl:

```bash
sudo journalctl -f -u buildkite-agent
```

Allow the agent to use serial devices:

```bash
usermod -a -G dialout buildkite-agent
```

#### Hooks

We setup bespoke PI workers which allows us to avoid key management issues for these devices since they do not need access to the source. The first thing is to tell buildkite _not_ to pull source. This is accomplished using the technique documented in [this pull request](https://github.com/buildkite/agent/pull/909). Namely, empty the `BUILDKITE_REPO` variable in the `environment` hook under `/etc/buildkite-agent/hooks`:

```bash
cd /etc/buildkite-agent/hooks
mv environment.sample environment
mv command.sample command
```

Sudo edit the `environment` file and change it to:

```bash
set -eu
echo '--- :strawberry: Setting up a no-source environment for raspberry pi.'

export BUILDKITE_REPO=
```

(vote for [this feature request](https://forum.buildkite.community/t/emoji/533) to get a :raspberry: emoji added to buildkite.)

Sudo edit the `command` file and change it to:

```bash
#!/usr/bin/env bash

# +----------------------------------------------------------+
# | BASH : Modifying Shell Behaviour
# |    (https://www.gnu.org/software/bash/manual)
# +----------------------------------------------------------+
# Treat unset variables and parameters other than the special
# parameters ‘@’ or ‘*’ as an error when performing parameter
# expansion. An error message will be written to the standard
# error, and a non-interactive shell will exit.
set -o nounset

# Exit immediately if a pipeline returns a non-zero status.
set -o errexit

# If set, the return value of a pipeline is the value of the
# last (rightmost) command to exit with a non-zero status, or
# zero if all commands in the pipeline exit successfully.
set -o pipefail

# +----------------------------------------------------------+
# | This script is one of the common set of commands run as
# | part of a continuous integration build pipeline.
# | These scrips are named using the following scheme:
# |
# |   [build_type]-[(optional)build_type qualifier]-[build|test|report|upload].sh
# |
# | Of course, libuavcan is a header-only distribution so
# | CI is used to verify and test rather than package and
# | deploy (i.e. There's really no 'I' going on).
# +----------------------------------------------------------+
mkdir -p build_ontarget_s32k
pushd build_ontarget_s32k

buildkite-agent artifact download "build_ontarget_s32k/*.hex" .
buildkite-agent artifact download "build_ontarget_s32k/*.jlink" .
ls -lAh

nait -vv \
     --port \
     /dev/serial/by-id/usb-Signoid_Kft._USB-UART_adapter_MACX98-if00-port0 \
     --port-speed 115200 \
     \*.jlink

popd

```

where the serial port is the one connected to the S32K dev kit.
