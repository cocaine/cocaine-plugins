#!/bin/bash
set -e
if [ "$1" == "clean" ]
then
    rm -rf ../build/*
fi

cd ../build
cmake -DCMAKE_BUILD_TYPE=Debug \
  -DCACHE=OFF \
  -DCHRONO=OFF \
  -DDOCKER=OFF \
  -DELASTICSEARCH=OFF \
  -DIPVS=OFF \
  -DMONGO=OFF \
  -DNODE=ON \
  -DURLFETCH=OFF \
  -DGRAPHITE=OFF \
  -DUNICORN=OFF \
  -DCONDUCTOR=ON \
  ..

# -DCMAKE_CXX_COMPILER="/usr/local/bin/compiler.sh"

make VERBOSE=1 install

