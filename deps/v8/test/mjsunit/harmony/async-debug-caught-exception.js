// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --harmony-async-await --expose-debug-as debug

Debug = debug.Debug

var exception = null;
var log;

function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Exception) return;
  try {
    var line = exec_state.frame(0).sourceLineText();
    var match = /Exception (\w)/.exec(line);
    assertNotNull(match);
    log.push(match[1]);
  } catch (e) {
    exception = e;
  }
}

async function thrower() {
  throw "a";  // Exception a
}

async function caught_throw() {
  try {
    await thrower();
  } catch (e) {
    assertEquals("a", e);
  }
}


// Caught throw, events on any exception.
log = [];
Debug.setListener(listener);
Debug.setBreakOnException();
caught_throw();
%RunMicrotasks();
Debug.setListener(null);
Debug.clearBreakOnException();
assertEquals(["a"], log);
assertNull(exception);

// Caught throw, events on uncaught exception.
log = [];
Debug.setListener(listener);
Debug.setBreakOnUncaughtException();
caught_throw();
%RunMicrotasks();
Debug.setListener(null);
Debug.clearBreakOnUncaughtException();
assertEquals([], log);
assertNull(exception);

var reject = Promise.reject("b");

async function caught_reject() {
  try {
    await reject;
  } catch (e) {
    assertEquals("b", e);
  }
}

// Caught reject, events on any exception.
log = [];
Debug.setListener(listener);
Debug.setBreakOnException();
caught_reject();
%RunMicrotasks();
Debug.setListener(null);
Debug.clearBreakOnException();
assertEquals([], log);
assertNull(exception);

// Caught reject, events on uncaught exception.
log = [];
Debug.setListener(listener);
Debug.setBreakOnUncaughtException();
caught_reject();
%RunMicrotasks();
Debug.setListener(null);
Debug.clearBreakOnUncaughtException();
assertEquals([], log);
assertNull(exception);
