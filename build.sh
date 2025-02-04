#!/bin/bash

export MACHINE=am335x-evm

pushd tisdk

/bin/bash ./oe-layertool-setup.sh -f configs/processor-sdk/processor-sdk-09.03.05.02-legacy-config.txt

pushd build

cp ../../meta-pholit/conf/*.conf ./conf

source conf/setenv
bitbake core-image-pgraphy -f


popd
popd

cp ./tisdk/build/deploy-ti/images/am335x-evm/core-image-pgraphy-am335x-evm.wic.xz ./
