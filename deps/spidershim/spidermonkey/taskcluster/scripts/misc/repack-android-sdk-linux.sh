#!/bin/bash
set -x -e -v

# This script is for fetching and repacking the Android SDK (for
# Linux), the tools required to produce Android packages.

WORKSPACE=$HOME/workspace
UPLOAD_DIR=$HOME/project/gecko/android-sdk

mkdir -p $HOME/artifacts $UPLOAD_DIR

# Populate /builds/worker/.mozbuild/android-sdk-linux.
cd /builds/worker/workspace/build/src
./mach python python/mozboot/mozboot/android.py --artifact-mode --no-interactive

# It's nice to have the build logs include the state of the world upon
# completion.
/builds/worker/.mozbuild/android-sdk-linux/tools/bin/sdkmanager --list

tar cf - -C /builds/worker/.mozbuild android-sdk-linux | xz > $UPLOAD_DIR/android-sdk-linux.tar.xz

ls -al $UPLOAD_DIR
