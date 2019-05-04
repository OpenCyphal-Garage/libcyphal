#
# Builds a docker image for test and validation builds.
#
FROM uavcan/libuavcan

VOLUME /repo

WORKDIR /repo

COPY provision.sh /

RUN /provision.sh
