// Flags: --expose-internals
'use strict';

const common = require('../common');
const errors = require('internal/errors');
const assert = require('assert');

errors.E('TEST_ERROR_1', 'Error for testing purposes: %s');
errors.E('TEST_ERROR_2', (a, b) => `${a} ${b}`);

const err1 = new errors.Error('TEST_ERROR_1', 'test');
const err2 = new errors.TypeError('TEST_ERROR_1', 'test');
const err3 = new errors.RangeError('TEST_ERROR_1', 'test');
const err4 = new errors.Error('TEST_ERROR_2', 'abc', 'xyz');
const err5 = new errors.Error('TEST_ERROR_1');

assert(err1 instanceof Error);
assert.strictEqual(err1.name, 'Error [TEST_ERROR_1]');
assert.strictEqual(err1.message, 'Error for testing purposes: test');
assert.strictEqual(err1.code, 'TEST_ERROR_1');

assert(err2 instanceof TypeError);
assert.strictEqual(err2.name, 'TypeError [TEST_ERROR_1]');
assert.strictEqual(err2.message, 'Error for testing purposes: test');
assert.strictEqual(err2.code, 'TEST_ERROR_1');

assert(err3 instanceof RangeError);
assert.strictEqual(err3.name, 'RangeError [TEST_ERROR_1]');
assert.strictEqual(err3.message, 'Error for testing purposes: test');
assert.strictEqual(err3.code, 'TEST_ERROR_1');

assert(err4 instanceof Error);
assert.strictEqual(err4.name, 'Error [TEST_ERROR_2]');
assert.strictEqual(err4.message, 'abc xyz');
assert.strictEqual(err4.code, 'TEST_ERROR_2');

assert(err5 instanceof Error);
assert.strictEqual(err5.name, 'Error [TEST_ERROR_1]');
assert.strictEqual(err5.message, 'Error for testing purposes: %s');
assert.strictEqual(err5.code, 'TEST_ERROR_1');

assert.throws(
  () => new errors.Error('TEST_FOO_KEY'),
  /^AssertionError: An invalid error message key was used: TEST_FOO_KEY\.$/);
// Calling it twice yields same result (using the key does not create it)
assert.throws(
  () => new errors.Error('TEST_FOO_KEY'),
  /^AssertionError: An invalid error message key was used: TEST_FOO_KEY\.$/);
assert.throws(
  () => new errors.Error(1),
  /^AssertionError: 'number' === 'string'$/);
assert.throws(
  () => new errors.Error({}),
  /^AssertionError: 'object' === 'string'$/);
assert.throws(
  () => new errors.Error([]),
  /^AssertionError: 'object' === 'string'$/);
assert.throws(
  () => new errors.Error(true),
  /^AssertionError: 'boolean' === 'string'$/);
assert.throws(
  () => new errors.TypeError(1),
  /^AssertionError: 'number' === 'string'$/);
assert.throws(
  () => new errors.TypeError({}),
  /^AssertionError: 'object' === 'string'$/);
assert.throws(
  () => new errors.TypeError([]),
  /^AssertionError: 'object' === 'string'$/);
assert.throws(
  () => new errors.TypeError(true),
  /^AssertionError: 'boolean' === 'string'$/);
assert.throws(
  () => new errors.RangeError(1),
  /^AssertionError: 'number' === 'string'$/);
assert.throws(
  () => new errors.RangeError({}),
  /^AssertionError: 'object' === 'string'$/);
assert.throws(
  () => new errors.RangeError([]),
  /^AssertionError: 'object' === 'string'$/);
assert.throws(
  () => new errors.RangeError(true),
  /^AssertionError: 'boolean' === 'string'$/);


// Tests for common.expectsError
assert.doesNotThrow(() => {
  assert.throws(() => {
    throw new errors.TypeError('TEST_ERROR_1', 'a');
  }, common.expectsError({ code: 'TEST_ERROR_1' }));
});

assert.doesNotThrow(() => {
  assert.throws(() => {
    throw new errors.TypeError('TEST_ERROR_1', 'a');
  }, common.expectsError({ code: 'TEST_ERROR_1',
                           type: TypeError,
                           message: /^Error for testing/ }));
});

assert.doesNotThrow(() => {
  assert.throws(() => {
    throw new errors.TypeError('TEST_ERROR_1', 'a');
  }, common.expectsError({ code: 'TEST_ERROR_1', type: TypeError }));
});

assert.doesNotThrow(() => {
  assert.throws(() => {
    throw new errors.TypeError('TEST_ERROR_1', 'a');
  }, common.expectsError({ code: 'TEST_ERROR_1', type: Error }));
});

assert.throws(() => {
  assert.throws(() => {
    throw new errors.TypeError('TEST_ERROR_1', 'a');
  }, common.expectsError({ code: 'TEST_ERROR_1', type: RangeError }));
}, /^AssertionError: .+ is not the expected type \S/);

assert.throws(() => {
  assert.throws(() => {
    throw new errors.TypeError('TEST_ERROR_1', 'a');
  }, common.expectsError({ code: 'TEST_ERROR_1',
                           type: TypeError,
                           message: /^Error for testing 2/ }));
}, /AssertionError: .+ does not match \S/);

assert.doesNotThrow(() => errors.E('TEST_ERROR_USED_SYMBOL'));
assert.throws(
  () => errors.E('TEST_ERROR_USED_SYMBOL'),
  /^AssertionError: Error symbol: TEST_ERROR_USED_SYMBOL was already used\.$/
);

// // Test ERR_INVALID_ARG_TYPE
assert.strictEqual(errors.message('ERR_INVALID_ARG_TYPE', ['a', 'b']),
                   'The "a" argument must be of type b');
assert.strictEqual(errors.message('ERR_INVALID_ARG_TYPE', ['a', ['b']]),
                   'The "a" argument must be of type b');
assert.strictEqual(errors.message('ERR_INVALID_ARG_TYPE', ['a', ['b', 'c']]),
                   'The "a" argument must be one of type b, or c');
assert.strictEqual(errors.message('ERR_INVALID_ARG_TYPE',
                                  ['a', ['b', 'c', 'd']]),
                   'The "a" argument must be one of type b, c, or d');
assert.strictEqual(errors.message('ERR_INVALID_ARG_TYPE', ['a', 'b', 'c']),
                   'The "a" argument must be of type b. Received type string');
assert.strictEqual(errors.message('ERR_INVALID_ARG_TYPE',
                                  ['a', 'b', undefined]),
                   'The "a" argument must be of type b. Received type ' +
                   'undefined');
assert.strictEqual(errors.message('ERR_INVALID_ARG_TYPE',
                                  ['a', 'b', null]),
                   'The "a" argument must be of type b. Received type null');
