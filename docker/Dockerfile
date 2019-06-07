#
# Builds a docker image for test and validation builds.
#
FROM uavcan/libuavcan:1.0

VOLUME /repo

WORKDIR /repo

COPY provision.sh /
COPY coveralls /coveralls

RUN /provision.sh
