'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const cbTypeError = /^TypeError: "callback" argument must be a function$/;
const callbackThrowValues = [null, true, false, 0, 1, 'foo', /foo/, [], {}];

const { sep } = require('path');
const warn = 'Calling an asynchronous function without callback is deprecated.';

common.refreshTmpDir();

function testMakeCallback(cb) {
  return function() {
    // fs.mkdtemp() calls makeCallback() on its third argument
    fs.mkdtemp(`${common.tmpDir}${sep}`, {}, cb);
  };
}

common.expectWarning('DeprecationWarning', warn);

// Passing undefined/nothing calls rethrow() internally, which emits a warning
assert.doesNotThrow(testMakeCallback());

function invalidCallbackThrowsTests() {
  callbackThrowValues.forEach((value) => {
    assert.throws(testMakeCallback(value), cbTypeError);
  });
}

invalidCallbackThrowsTests();
