#!/bin/bash

docker run \
    --device=/dev/kvm:/devb/kvm \
    --device=/dev/net/tun:/dev/net/tun \
    --cap-add NET_ADMIN \
    --hostname buildserver \
    -it \
    -v /tftpboot:/tftpboot \
    -v `pwd`:/home/build/work:rw \
    yocto
