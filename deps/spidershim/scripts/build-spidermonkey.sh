#!/bin/bash

for ac in "$AUTOCONF" autoconf213 autoconf2.13; do
  if which $ac >/dev/null; then
    AUTOCONF=`which $ac`
    break
  fi
done

if test -z "$AUTOCONF"; then
  >&2 echo 'autoconf 2.13 is required for building SpiderMonkey, but we could not find it.'
  >&2 echo 'Please set the AUTOCONF environment variable before running this script.'
  exit 1
fi

case "$BUILDTYPE" in
  Debug)
    args="--enable-debug --disable-optimize"
    ;;
  Release)
    args="--disable-debug --enable-optimize"
    ;;
esac

build="$BUILT_PRODUCTS_DIR/spidermonkey"
srcdir="$PWD"
test -d $build || mkdir $build
cd $build
# First try running Make.  If configure has changed, it will fail, so
# we'll fall back to configure && make.
make -s || (cd "$srcdir/spidermonkey/js/src" && $AUTOCONF && cd - && "$srcdir/spidermonkey/js/src/configure" --disable-shared-js --disable-export-js --disable-js-shell $args $* && make -s)
