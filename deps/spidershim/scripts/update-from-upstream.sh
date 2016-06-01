#!/bin/bash

set -e

if ! test -d "$1"; then
  >&2 echo 'Please pass in the path to the Mozilla source tree as the first argument.'
  exit 1
fi

if test -d spidermonkey; then
  rm -rf spidermonkey
fi

SM_DIR="$1"

rsync -av --delete "$SM_DIR"/nsprpub spidermonkey/
rsync -av --delete "$SM_DIR"/mfbt spidermonkey/
rsync -av --delete "$SM_DIR"/memory spidermonkey/
rsync -av --delete "$SM_DIR"/mozglue spidermonkey/
rsync -av --delete "$SM_DIR"/python spidermonkey/
rsync -av --delete "$SM_DIR"/moz.configure spidermonkey/
rsync -av --delete "$SM_DIR"/moz.build spidermonkey/
rsync -av --delete "$SM_DIR"/Makefile.in spidermonkey/
rsync -av --delete "$SM_DIR"/configure.py spidermonkey/
rsync -av --delete "$SM_DIR"/build spidermonkey/
rsync -av --delete "$SM_DIR"/testing/mozbase spidermonkey/testing/
rsync -av --delete "$SM_DIR"/config spidermonkey/
rsync -av --delete "$SM_DIR"/intl/icu spidermonkey/intl/
test -d spidermonkey/layout/tools/reftest || mkdir -p spidermonkey/layout/tools/reftest
test -d spidermonkey/dom/bindings || mkdir -p spidermonkey/dom/bindings
test -d spidermonkey/taskcluster || mkdir -p spidermonkey/taskcluster
rsync -av --delete "$SM_DIR"/taskcluster/moz.build spidermonkey/taskcluster/
rsync -av --delete "$SM_DIR"/modules/fdlibm spidermonkey/modules/
rsync -av --delete "$SM_DIR"/js/src spidermonkey/js/
rsync -av --delete "$SM_DIR"/js/public spidermonkey/js/
rsync -av --delete "$SM_DIR"/js/moz.configure spidermonkey/js/

for patch in `ls spidermonkey-patches/* | sort`; do
  (cd spidermonkey && patch -p1 < "../$patch")
done

git add spidermonkey
# The following will fail if there are no deleted files, so || with true.
git rm -r `git ls-files --deleted spidermonkey` || true
# The following will fail if there are no added files, so || with true.
git add -f `git ls-files --others spidermonkey` || true

scripts/build-spidermonkey-files.py && git add spidermonkey-files.gypi

rev=`(cd "$SM_DIR" && git rev-parse HEAD)`
git commit -m "Syncing SpiderMonkey from Mozilla upstream revision $rev"
