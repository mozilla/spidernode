#!/bin/bash

if [ ! -f Makefile -o ! -f node.gyp ]; then
  >&2 echo 'Usage: ./deps/spidershim/scripts/find_all_unimplemented_methods.sh'
  >&2 echo 'Note that this script needs to be run in the root of a built tree'
fi

case `uname` in
  Darwin)
    make 2>&1 | grep -E "\"v8::[A-Za-z0-9]+::" | sed 's/\"//g' | sed 's/^  //' | sed 's/, referenced from://g' | sort
    ;;
  Linux)
    make 2>&1 | grep -E "undefined reference to \`v8::[A-Za-z0-9]+::" | sed 's/^.*undefined reference to `//' | sed "s/'\$//" | sort | uniq
    ;;
esac
