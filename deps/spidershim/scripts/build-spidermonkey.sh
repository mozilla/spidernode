#!/bin/bash

source `dirname "$0"`/travis.sh

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

srcdir="$PWD"
build="$1"
shift

case "$*" in
  *--enable-address-sanitizer*)
    export ASANFLAGS="-fsanitize=address -Dxmalloc=myxmalloc -fPIC"
    export CFLAGS="$ASANFLAGS"
    export CXXFLAGS="$ASANFLAGS"
    export LDFLAGS="-fsanitize=address"
    ;;
esac

test -d $build || mkdir $build
cd $build
if [ "$TRAVIS" == "true" ]; then
make="travis_wait 60 make -s"
else
make="make -s"
fi
# First try running Make.  If configure has changed, it will fail, so
# we'll fall back to configure && make.
$make || (cd "$srcdir/spidermonkey/js/src" && $AUTOCONF && cd - && "$srcdir/spidermonkey/js/src/configure" --disable-shared-js --disable-export-js --disable-js-shell --enable-sm-promise $args $* && $make)
