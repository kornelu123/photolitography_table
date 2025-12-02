#!/bin/bash

DOCKER_NAME=j00r_yocto

del_docker() {
docker rm -f \
    $DOCKER_NAME:latest
}

print_usage() {
    echo "Builds image pgraphy table"
    echo ""
    echo "-i         interactive, builds docker, and starts shell, delete after usage with -d"
    echo "-d         deletes docker image built with -i"
    echo "-b         runs docker, builds linux image, and deletes docker image"
}

if [ $# != 1 ]; then
    print_usage
    exit -1
fi

if [ $1 == "-b" ]; then
    if [ -z "$(docker images -q $DOCKER_NAME:latest 2> /dev/null)" ]; then
        docker build . -t j00r_yocto
    fi

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
        --rm \
        $DOCKER_NAME \
        ./build.sh

    exit 0
fi

if [ $1 == "-i" ]; then
    if [ -z "$(docker images -q $DOCKER_NAME:latest 2> /dev/null)" ]; then
        docker build . -t j00r_yocto
    fi

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
        --rm \
        $DOCKER_NAME

    exit 0
fi

if [ $1 == "-d" ]; then
    del_docker

    exit 0
fi

print_usage
