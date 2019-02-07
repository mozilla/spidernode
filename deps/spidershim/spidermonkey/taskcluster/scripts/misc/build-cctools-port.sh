#!/bin/bash

# cctools sometimes needs to be rebuilt when clang is modified.
# Until bug 1471905 is addressed, increase the following number
# when a forced rebuild of cctools is necessary: 1

set -x -e -v

# This script is for building cctools (Apple's binutils) for Linux using
# cctools-port (https://github.com/tpoechtrager/cctools-port).
WORKSPACE=$HOME/workspace
UPLOAD_DIR=$HOME/artifacts

# Repository info
: CROSSTOOL_PORT_REPOSITORY    ${CROSSTOOL_PORT_REPOSITORY:=https://github.com/tpoechtrager/cctools-port}
: CROSSTOOL_PORT_REV           ${CROSSTOOL_PORT_REV:=8e9c3f2506b51cf56725eaa60b6e90e240e249ca}

# Set some crosstools-port directories
CROSSTOOLS_SOURCE_DIR=$WORKSPACE/crosstools-port
CROSSTOOLS_CCTOOLS_DIR=$CROSSTOOLS_SOURCE_DIR/cctools
CROSSTOOLS_BUILD_DIR=$WORKSPACE/cctools
CLANG_DIR=$WORKSPACE/build/src/clang

# Create our directories
mkdir -p $CROSSTOOLS_BUILD_DIR

git clone --no-checkout $CROSSTOOL_PORT_REPOSITORY $CROSSTOOLS_SOURCE_DIR
cd $CROSSTOOLS_SOURCE_DIR
git checkout $CROSSTOOL_PORT_REV
# Cherry pick two fixes for LTO.
git cherry-pick -n 82381f5038a340025ae145745ae5b325cd1b749a
git cherry-pick -n 328c7371008a854af30823adcd4ec1e763054a1d
echo "Building from commit hash `git rev-parse $CROSSTOOL_PORT_REV`..."

# Fetch clang from tooltool
cd $WORKSPACE/build/src
. taskcluster/scripts/misc/tooltool-download.sh

# Configure crosstools-port
cd $CROSSTOOLS_CCTOOLS_DIR
export CC=$CLANG_DIR/bin/clang
export CXX=$CLANG_DIR/bin/clang++
export LDFLAGS="-lpthread -Wl,-rpath-link,$CLANG_DIR/lib"
./autogen.sh
./configure --prefix=$CROSSTOOLS_BUILD_DIR --target=x86_64-darwin11 --with-llvm-config=$CLANG_DIR/bin/llvm-config

# Build cctools
make -j `nproc --all` install
strip $CROSSTOOLS_BUILD_DIR/bin/*
# cctools-port doesn't include dsymutil but clang will need to find it.
cp $CLANG_DIR/bin/dsymutil $CROSSTOOLS_BUILD_DIR/bin/x86_64-darwin11-dsymutil
# various build scripts based on cmake want to find `lipo` without a prefix
cp $CROSSTOOLS_BUILD_DIR/bin/x86_64-darwin11-lipo $CROSSTOOLS_BUILD_DIR/bin/lipo

# Put a tarball in the artifacts dir
mkdir -p $UPLOAD_DIR
tar cJf $UPLOAD_DIR/cctools.tar.xz -C $CROSSTOOLS_BUILD_DIR/.. `basename $CROSSTOOLS_BUILD_DIR`
