#!/bin/bash

test -d build || mkdir build
cd build
../spidermonkey/js/src/configure --disable-shared-js --disable-js-shell
make -j8
