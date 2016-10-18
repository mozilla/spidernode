#!/usr/bin/env python

import subprocess
import os
import sys

def main():
    base_dir = get_base_dir()
    if not os.path.isdir(base_dir):
        print >> sys.stderr, 'Please run this script in the SpiderNode checkout directory.'
        return 1

    ran_test = False
    targets, external_sm = get_targets(base_dir)
    matrix = [(test, config) for test in targets for config in ['Debug', 'Release']]
    lib_path = 'DYLD_LIBRARY_PATH' if sys.platform == 'darwin' else 'LD_LIBRARY_PATH'

    for test, config in matrix:
        cmd = "%s/out/%s/%s" % (base_dir, config, test)
        env = ({lib_path: '%s/dist/bin' % (external_sm[config])}) if external_sm[config] else None
        ran, success = run_test([cmd], env)
        if not success:
            return 1
        ran_test = ran or ran_test

    for node in get_node_binaries(base_dir):
        config = 'Debug' if node.find('node_g') > 0 else 'Release'
        env = ({lib_path: '%s/dist/bin' % (external_sm[config])}) if external_sm[config] else None
        ran, success = run_test([node, '-e', 'console.log("hello, world")'], env)
        if not success:
            return 1
        ran_test = ran or ran_test

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
    sys.path.insert(0, "%s/tools/gyp/pylib" % base_dir)
    import gyp

    path = "deps/spidershim/tests.gyp"
    params = {
      b'parallel': False,
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

    external_sm = {
      'Release': data['deps/spidershim/tests.gyp']['variables']['external_spidermonkey_release'],
      'Debug': data['deps/spidershim/tests.gyp']['variables']['external_spidermonkey_debug'],
    }
    targets = [target['target_name'] for target in data['deps/spidershim/tests.gyp']['targets']]
    return (targets, external_sm)


def get_node_binaries(base_dir):
    nodes = []
    for name in ['node', 'node_g']:
        path = "%s/%s" % (base_dir, name)
        if is_executable(path):
            nodes.append(path)

    return nodes

def run_test(cmd, env):
    if is_executable(cmd[0]):
        try:
            subprocess.check_call(cmd, env=env)
        except:
            print >> sys.stderr, '%s failed, see the log above' % (" ".join(cmd))
            return (True, False)
        return (True, True)
    else:
        print >> sys.stderr, 'Skipping %s since it does not exist' % (" ".join(cmd))
    return (False, True)

if __name__ == '__main__':
    sys.exit(main())
