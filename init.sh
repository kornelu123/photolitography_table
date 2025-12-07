#!/bin/bash

git submodule update --init
pushd tisdk
git checkout ccb3361f088963654df1a03b5db3405472b2b36c
/bin/bash ./oe-layertool-setup.sh -f configs/processor-sdk/processor-sdk-09.03.05.02-legacy-config.txt
popd
