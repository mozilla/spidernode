#!/usr/bin/env python
# This script reads the SpiderShim tests gyp file, and prints out
# the names of all of the targets defined there.

import gyp
import subprocess
import os
import sys

def main():
    base_dir = get_base_dir()
    if not os.path.isdir(base_dir):
        print >> sys.stderr, 'Please run this script in the SpiderNode checkout directory.'
        return 1

    ran_test = False
    targets = get_targets(base_dir)
    matrix = [(test, config) for test in targets for config in ['Debug', 'Release']]

    for test, config in matrix:
        cmd = "%s/out/%s/%s" % (base_dir, config, test)
        if is_executable(cmd):
            try:
                subprocess.Popen(cmd).communicate()
            except e:
                print >> sys.stderr, '%s failed, see the log above' % cmd
                return e.returncode
            ran_test = True
        else:
            print >> sys.stderr, 'Skipping %s since it does not exist' % cmd

    if not ran_test:
        print >> sys.stderr, 'Did not run any tests! Did you forget to build?'
    return 0 if ran_test else 1

def get_base_dir():
    try:
        process = subprocess.Popen("git rev-parse --show-toplevel".split(" "),
                                   stdout=subprocess.PIPE)
        return process.communicate()[0].strip()
    except e:
        return None

def is_executable(f):
    return os.path.isfile(f) and os.access(f, os.X_OK)

def get_targets(base_dir):
    sys.path.append("%s/tools/gyp/pylib" % base_dir)

    path = "deps/spidershim/tests.gyp"
    params = {
      b'parallel': True,
      b'root_targets': None,
    }
    includes = [
      "config.gypi",
      "common.gypi",
    ]
    default_variables = {
      b'OS': 'linux',
      b'STATIC_LIB_PREFIX': '',
      b'STATIC_LIB_SUFFIX': '',
    }

    # Use the gypd generator as very simple generator, since we're only
    # interested in reading the target names.
    generator, flat_list, targets, data = \
        gyp.Load([path], format='gypd', \
                 params=params, includes=includes, \
                 default_variables=default_variables)

    return [target['target_name'] for target in data['deps/spidershim/tests.gyp']['targets']]

if __name__ == '__main__':
    sys.exit(main())
