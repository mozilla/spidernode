#!/bin/bash
set -x -e -v

# This script is for fetching and repacking the Android NDK (for
# Linux), the tools required to produce native Android programs.

WORKSPACE=$HOME/workspace
UPLOAD_DIR=$HOME/project/gecko/android-ndk

mkdir -p $HOME/artifacts $UPLOAD_DIR

# Populate /builds/worker/.mozbuild/android-ndk-$VER.
cd /builds/worker/workspace/build/src
./mach python python/mozboot/mozboot/android.py --ndk-only --no-interactive

# Don't generate a tarball with a versioned NDK directory.
mv $HOME/.mozbuild/android-ndk-* $HOME/.mozbuild/android-ndk
tar cf - -C /builds/worker/.mozbuild android-ndk | xz > $UPLOAD_DIR/android-ndk.tar.xz

ls -al $UPLOAD_DIR
