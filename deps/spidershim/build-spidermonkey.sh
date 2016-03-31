#!/bin/bash

test -d build || mkdir build
cd build
# First try running Make.  If configure has changed, it will fail, so
# we'll fall back to configure && make.
make -j8 || (../spidermonkey/js/src/configure --disable-shared-js --disable-js-shell && make -j8)
