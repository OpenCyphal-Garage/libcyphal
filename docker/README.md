# Libuavcan Docker

The `uavcan/libuavcan` docker image provides a consistent build and test environment
for development, continuous-integration, and test automation.

## Use locally

(these instructions assume you have docker installed and running)

```bash
cd /path/to/libuavcan

docker pull uavcan/libuavcan:latest

mkdir /path/to/libuavcan/build_docker

docker run --rm -t -v /path/to/libuavcan:/repo uavcan/libuavcan:latest /bin/sh -c 'cd build_docker && cmake ..'

docker run --rm -t -v /path/to/libuavcan/build:/repo uavcan/libuavcan:latest /bin/sh -c 'cd build_docker && mkdir make -j8'
```

On macintosh you'll probably want to optimize osxfs with something like cached or delegated:

```bash
docker run --rm -t -v /path/to/libuavcan/build:/repo:delegated uavcan/libuavcan:latest /bin/sh -c 'cd build_docker && mkdir make -j8'
```

See ["Performance tuning for volume mounts"](https://docs.docker.com/docker-for-mac/osxfs-caching/) for details.

Finally, to enter an interactive shell in this container something like this should work:

```bash
docker run --rm -it -v /path/to/libuavcan:/repo uavcan/libuavcan:latest
```

## Travis CI

You can use this in your .travis.yml like this:

```bash
language: cpp

services:
  - docker

before_install:
  - docker pull uavcan/libuavcan:latest

script:
  - docker run --rm -v $TRAVIS_BUILD_DIR:/repo uavcan/libuavcan:latest /bin/sh -c ci.sh

```

## Build and Push

These instructions are for maintainers with permissions to push to the [uavcan organization on Docker Hub](https://cloud.docker.com/u/uavcan).

```bash
docker build .
```
```bash
docker images

REPOSITORY   TAG      IMAGE ID       CREATED              SIZE
<none>       <none>   736647481ad3   About a minute ago   1GB
```
```bash
docker tag 736647481ad3 uavcan/libuavcan:latest
docker login --username=yourhubusername
docker push uavcan/libuavcan:latest
```
