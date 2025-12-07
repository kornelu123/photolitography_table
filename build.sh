#!/bin/bash

export MACHINE=am335x-evm

pushd tisdk
pushd build

cp ../../meta-pholit/conf/*.conf ./conf

source conf/setenv
bitbake core-image-pgraphy -f

popd
popd

cp ./tisdk/build/deploy-ti/images/am335x-evm/core-image-pgraphy-am335x-evm.wic.xz ./
