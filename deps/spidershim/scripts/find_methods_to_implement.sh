#!/bin/bash

if [ -z "$1" -o ! -f Makefile -o ! -f node.gyp ]; then
  >&2 echo 'Usage: ./deps/spidershim/scripts/find_methods_to_implement.sh CLASSNAME'
  >&2 echo 'Note that this script needs to be run in the root of a built tree'
fi

case `uname` in
  Darwin)
    make 2>&1 | grep "\"v8::$1::" | sed 's/\"//g' | sed 's/^  //' | sed 's/, referenced from://g' | sort
    ;;
  Linux)
    make 2>&1 | grep "undefined reference to \`v8::Isolate::" | sed 's/^.*undefined reference to `//' | sed "s/'\$//" | sort | uniq
    ;;
esac
