#!/bin/bash

git submodule update --init
docker build . -t j00r_yocto
pushd tisdk
git checkout ccb3361f088963654df1a03b5db3405472b2b36c
popd
