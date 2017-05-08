#!/usr/bin/env python
# Copyright 2017 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""\
Convenience wrapper for compiling V8 with gn/ninja and running tests.
Sets up build output directories if they don't exist.
Produces simulator builds for non-Intel target architectures.
Uses Goma by default if it is detected (at output directory setup time).
Expects to be run from the root of a V8 checkout.

Usage:
    gm.py [<arch>].[<mode>].[<target>] [testname...]

All arguments are optional. Most combinations should work, e.g.:
    gm.py ia32.debug x64.release d8
    gm.py x64 mjsunit/foo cctest/test-bar/*
"""
# See HELP below for additional documentation.

import os
import subprocess
import sys

BUILD_OPTS_DEFAULT = ""
BUILD_OPTS_GOMA = "-j1000 -l50"
BUILD_TARGETS_TEST = ["d8", "cctest", "unittests"]
BUILD_TARGETS_ALL = ["all"]

# All arches that this script understands.
ARCHES = ["ia32", "x64", "arm", "arm64", "mipsel", "mips64el", "ppc", "ppc64",
          "s390", "s390x", "x87"]
# Arches that get built/run when you don't specify any.
DEFAULT_ARCHES = ["ia32", "x64", "arm", "arm64"]
# Modes that this script understands.
MODES = ["release", "debug", "optdebug"]
# Modes that get built/run when you don't specify any.
DEFAULT_MODES = ["release", "debug"]
# Build targets that can be manually specified.
TARGETS = ["d8", "cctest", "unittests", "v8_fuzzers"]
# Build targets that get built when you don't specify any (and specified tests
# don't imply any other targets).
DEFAULT_TARGETS = ["d8"]
# Tests that run-tests.py would run by default that can be run with
# BUILD_TARGETS_TESTS.
DEFAULT_TESTS = ["cctest", "debugger", "intl", "message", "mjsunit",
                 "preparser", "unittests"]
# These can be suffixed to any <arch>.<mode> combo, or used standalone,
# or used as global modifiers (affecting all <arch>.<mode> combos).
ACTIONS = {
  "all": {"targets": BUILD_TARGETS_ALL, "tests": []},
  "tests": {"targets": BUILD_TARGETS_TEST, "tests": []},
  "check": {"targets": BUILD_TARGETS_TEST, "tests": DEFAULT_TESTS},
  "checkall": {"targets": BUILD_TARGETS_ALL, "tests": ["ALL"]},
}

HELP = """<arch> can be any of: %(arches)s
<mode> can be any of: %(modes)s
<target> can be any of:
 - cctest, d8, unittests, v8_fuzzers (build respective binary)
 - all (build all binaries)
 - tests (build test binaries)
 - check (build test binaries, run most tests)
 - checkall (build all binaries, run more tests)
""" % {"arches": " ".join(ARCHES),
       "modes": " ".join(MODES)}

TESTSUITES_TARGETS = {"benchmarks": "d8",
              "cctest": "cctest",
              "debugger": "d8",
              "fuzzer": "v8_fuzzers",
              "intl": "d8",
              "message": "d8",
              "mjsunit": "d8",
              "mozilla": "d8",
              "preparser": "d8",
              "test262": "d8",
              "unittests": "unittests",
              "webkit": "d8"}

OUTDIR = "out"

IS_GOMA_MACHINE = (os.path.exists(os.path.expanduser("~/goma")) or
                   os.environ.get('GOMADIR'))

USE_GOMA = "true" if IS_GOMA_MACHINE else "false"
BUILD_OPTS = BUILD_OPTS_GOMA if IS_GOMA_MACHINE else BUILD_OPTS_DEFAULT

RELEASE_ARGS_TEMPLATE = """\
is_component_build = false
is_debug = false
%s
use_goma = {GOMA}
v8_enable_backtrace = true
v8_enable_disassembler = true
v8_enable_object_print = true
v8_enable_verify_heap = true
""".replace("{GOMA}", USE_GOMA)

DEBUG_ARGS_TEMPLATE = """\
gdb_index = true
is_component_build = true
is_debug = true
symbol_level = 2
%s
use_goma = {GOMA}
v8_enable_backtrace = true
v8_enable_slow_dchecks = true
v8_optimized_debug = false
""".replace("{GOMA}", USE_GOMA)

OPTDEBUG_ARGS_TEMPLATE = """\
gdb_index = false
is_component_build = true
is_debug = true
symbol_level = 1
%s
use_goma = {GOMA}
v8_enable_backtrace = true
v8_enable_verify_heap = true
v8_optimized_debug = true
""".replace("{GOMA}", USE_GOMA)

ARGS_TEMPLATES = {
  "release": RELEASE_ARGS_TEMPLATE,
  "debug": DEBUG_ARGS_TEMPLATE,
  "optdebug": OPTDEBUG_ARGS_TEMPLATE
}

def PrintHelpAndExit():
  print(__doc__)
  print(HELP)
  sys.exit(0)

def _Call(cmd, silent=False):
  if not silent: print("# %s" % cmd)
  return subprocess.call(cmd, shell=True)

def _Write(filename, content):
  print("# echo > %s << EOF\n%sEOF" % (filename, content))
  with open(filename, "w") as f:
    f.write(content)

def GetPath(arch, mode):
  subdir = "%s.%s" % (arch, mode)
  return os.path.join(OUTDIR, subdir)

class Config(object):
  def __init__(self, arch, mode, targets, tests=[]):
    self.arch = arch
    self.mode = mode
    self.targets = set(targets)
    self.tests = set(tests)

  def Extend(self, targets, tests=[]):
    self.targets.update(targets)
    self.tests.update(tests)

  def GetTargetCpu(self):
    cpu = "x86"
    if self.arch.endswith("64") or self.arch == "s390x":
      cpu = "x64"
    return "target_cpu = \"%s\"" % cpu

  def GetV8TargetCpu(self):
    if self.arch in ("arm", "arm64", "mipsel", "mips64el", "ppc", "ppc64",
                     "s390", "s390x"):
      return "\nv8_target_cpu = \"%s\"" % self.arch
    return ""

  def GetGnArgs(self):
    template = ARGS_TEMPLATES[self.mode]
    arch_specific = self.GetTargetCpu() + self.GetV8TargetCpu()
    return template % arch_specific

  def Build(self):
    path = GetPath(self.arch, self.mode)
    args_gn = os.path.join(path, "args.gn")
    if not os.path.exists(path):
      print("# mkdir -p %s" % path)
      os.makedirs(path)
    if not os.path.exists(args_gn):
      _Write(args_gn, self.GetGnArgs())
      code = _Call("gn gen %s" % path)
      if code != 0: return code
    targets = " ".join(self.targets)
    return _Call("ninja -C %s %s %s" % (path, BUILD_OPTS, targets))

  def RunTests(self):
    if not self.tests: return 0
    if "ALL" in self.tests:
      tests = ""
    else:
      tests = " ".join(self.tests)
    return _Call("tools/run-tests.py --arch=%s --mode=%s %s" %
                 (self.arch, self.mode, tests))

def GetTestBinary(argstring):
  for suite in TESTSUITES_TARGETS:
    if argstring.startswith(suite): return TESTSUITES_TARGETS[suite]
  return None

class ArgumentParser(object):
  def __init__(self):
    self.global_targets = set()
    self.global_tests = set()
    self.global_actions = set()
    self.configs = {}

  def PopulateConfigs(self, arches, modes, targets, tests):
    for a in arches:
      for m in modes:
        path = GetPath(a, m)
        if path not in self.configs:
          self.configs[path] = Config(a, m, targets, tests)
        else:
          self.configs[path].Extend(targets, tests)

  def ProcessGlobalActions(self):
    have_configs = len(self.configs) > 0
    for action in self.global_actions:
      impact = ACTIONS[action]
      if (have_configs):
        for c in self.configs:
          self.configs[c].Extend(**impact)
      else:
        self.PopulateConfigs(DEFAULT_ARCHES, DEFAULT_MODES, **impact)

  def ParseArg(self, argstring):
    if argstring in ("-h", "--help", "help"):
      PrintHelpAndExit()
    arches = []
    modes = []
    targets = []
    actions = []
    tests = []
    words = argstring.split('.')
    if len(words) == 1:
      word = words[0]
      if word in ACTIONS:
        self.global_actions.add(word)
        return
      if word in TARGETS:
        self.global_targets.add(word)
        return
      maybe_target = GetTestBinary(word)
      if maybe_target is not None:
        self.global_tests.add(word)
        self.global_targets.add(maybe_target)
        return
    for word in words:
      if word in ARCHES:
        arches.append(word)
      elif word in MODES:
        modes.append(word)
      elif word in TARGETS:
        targets.append(word)
      elif word in ACTIONS:
        actions.append(word)
      else:
        print("Didn't understand: %s" % word)
        sys.exit(1)
    # Process actions.
    for action in actions:
      impact = ACTIONS[action]
      targets += impact["targets"]
      tests += impact["tests"]
    # Fill in defaults for things that weren't specified.
    arches = arches or DEFAULT_ARCHES
    modes = modes or DEFAULT_MODES
    targets = targets or DEFAULT_TARGETS
    # Produce configs.
    self.PopulateConfigs(arches, modes, targets, tests)

  def ParseArguments(self, argv):
    if len(argv) == 0:
      PrintHelpAndExit()
    for argstring in argv:
      self.ParseArg(argstring)
    self.ProcessGlobalActions()
    for c in self.configs:
      self.configs[c].Extend(self.global_targets, self.global_tests)
    return self.configs

def Main(argv):
  parser = ArgumentParser()
  configs = parser.ParseArguments(argv[1:])
  return_code = 0
  for c in configs:
    return_code += configs[c].Build()
  for c in configs:
    return_code += configs[c].RunTests()
  if return_code == 0:
    _Call("notify-send 'Done!' 'V8 compilation finished successfully.'",
          silent=True)
  else:
    _Call("notify-send 'Error!' 'V8 compilation finished with errors.'",
          silent=True)
  return return_code

if __name__ == "__main__":
  sys.exit(Main(sys.argv))
