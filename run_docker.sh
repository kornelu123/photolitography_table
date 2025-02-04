#!/bin/bash

DOCKER_NAME=j00r_yocto

del_docker() {
docker rmi \
    $DOCKER_NAME:latest
}

print_usage() {
    echo "Builds image pgraphy table"
    echo ""
    echo "-i         interactive, builds docker, and starts shell, delete after usage with -d"
    echo "-d         deletes docker image built with -i"
    echo "-b         runs docker, builds linux image, and deletes docker image"
}

if [ $# != 2 ]; then
    print_usage

    exit -1
fi

if [ $1 == -b ]; then
    docker run \
        --device=/dev/kvm:/dev/kvm \
        --device=/dev/net/tun:/dev/net/tun \
        --cap-add NET_ADMIN \
        --hostname buildserver \
        -it \
        -v /tftpboot:/tftpboot \
        -v `pwd`:/home/build:rw \
        --dns=8.8.8.8 \
        --network=host \
        $DOCKER_NAME \
        ./build.sh

    exit 0
fi

if [ $1 == "-i" ]; then
docker run \
    --device=/dev/kvm:/dev/kvm \
    --device=/dev/net/tun:/dev/net/tun \
    --cap-add NET_ADMIN \
    --hostname buildserver \
    -it \
    -v /tftpboot:/tftpboot \
    -v `pwd`:/home/build:rw \
    --dns=8.8.8.8 \
    --network=host \
    $DOCKER_NAME

    exit 0
fi

if [ $1 == "-d" ]; then
    del_docker

    exit 0
fi

print_usage
