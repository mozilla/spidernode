#!/bin/bash

mydir=`dirname $0`

source "$mydir/travis.sh"

for ac in "$AUTOCONF" autoconf213 autoconf2.13 autoconf-2.13; do
  if which $ac >/dev/null; then
    AUTOCONF=`which $ac`
    break
  fi
done

if test -z "$AUTOCONF" -o `$AUTOCONF --version | head -1 | sed -e 's/[^0-9\.]//g'` != "2.13"; then
  >&2 echo 'autoconf 2.13 is required for building SpiderMonkey, but we could not find it.'
  >&2 echo 'Please set the AUTOCONF environment variable to the path of autoconf 2.13 on your system before running this script.'
  exit 1
fi

config="$BUILDTYPE"
if [ -z "$config" ]; then
  config="$CONFIGURATION"
fi

case "$config" in
  Debug)
    args="--enable-debug --disable-optimize"
    ;;
  Release)
    args="--disable-debug --enable-optimize"
    ;;
esac

srcdir=`cd "$mydir/.." && pwd`
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
  if [ "$OSTYPE" == "msys" ]; then
    make="mozmake -s"
  else
    make="make -s"
  fi
fi

ccache_arg=""
if echo "$CC" | grep -wq ccache; then
  ccache_arg="--with-ccache=`echo "$CC" | cut -f 1 -d ' '`"
  export CC=`echo "$CC" | cut -f 2 -d ' '`
  export CXX=`echo "$CXX" | cut -f 2 -d ' '`
fi

# First try running Make.  If configure has changed, it will fail, so
# we'll fall back to configure && make.
$make || (cd "$srcdir/spidermonkey/js/src" && $AUTOCONF && cd - && "$srcdir/spidermonkey/js/src/configure" --disable-shared-js --disable-export-js --disable-js-shell $ccache_arg $args $* && $make)
