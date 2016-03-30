#!/bin/bash

test -d build || mkdir build
cd build
../spidermonkey/js/src/configure
make -j8
