#!/bin/bash

set -e

BASE_DIR=`git rev-parse --show-toplevel`

if ! test -d "$BASE_DIR"; then
  >&2 echo 'Please run this script in the SpiderNode checkout directory.'
  exit 1
fi

for test in "$BASE_DIR"/out/{Debug,Release}/{hello-world,exception,persistent,trycatch,value}; do
  if ! "$test"; then
    >&2 echo "$test failed, see the log above"
    exit 1;
  fi;
done
