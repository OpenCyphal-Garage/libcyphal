#!/usr/bin/env sh
# First-time usage: docker build . && docker tag <id> libudpard && ./run-docker.sh

dockerimage=libudpard

docker run -it --rm -v $(pwd):/home/user/libudpard:z -e LOCAL_USER_ID=`id -u` $dockerimage "$@"
