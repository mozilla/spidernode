'use strict';
const common = require('../common');

// This test checks that the semantics of `util.callbackify` are as described in
// the API docs

const assert = require('assert');
const { callbackify } = require('util');
const { execFile } = require('child_process');
const fixtures = require('../common/fixtures');

const values = [
  'hello world',
  null,
  undefined,
  false,
  0,
  {},
  { key: 'value' },
  Symbol('I am a symbol'),
  function ok() {},
  ['array', 'with', 4, 'values'],
  new Error('boo')
];

{
  // Test that the resolution value is passed as second argument to callback
  for (const value of values) {
    // Test and `async function`
    async function asyncFn() {
      return await Promise.resolve(value);
    }

    const cbAsyncFn = callbackify(asyncFn);
    cbAsyncFn(common.mustCall((err, ret) => {
      assert.ifError(err);
      assert.strictEqual(ret, value);
    }));

    // Test Promise factory
    function promiseFn() {
      return Promise.resolve(value);
    }

    const cbPromiseFn = callbackify(promiseFn);
    cbPromiseFn(common.mustCall((err, ret) => {
      assert.ifError(err);
      assert.strictEqual(ret, value);
    }));

    // Test Thenable
    function thenableFn() {
      return {
        then(onRes, onRej) {
          onRes(value);
        }
      };
    }

    const cbThenableFn = callbackify(thenableFn);
    cbThenableFn(common.mustCall((err, ret) => {
      assert.ifError(err);
      assert.strictEqual(ret, value);
    }));
  }
}

{
  // Test that rejection reason is passed as first argument to callback
  for (const value of values) {
    // Test an `async function`
    async function asyncFn() {
      return await Promise.reject(value);
    }

    const cbAsyncFn = callbackify(asyncFn);
    cbAsyncFn(common.mustCall((err, ret) => {
      assert.strictEqual(ret, undefined);
      if (err instanceof Error) {
        if ('reason' in err) {
          assert(!value);
          assert.strictEqual(err.code, 'ERR_FALSY_VALUE_REJECTION');
          assert.strictEqual(err.reason, value);
        } else {
          assert.strictEqual(String(value).endsWith(err.message), true);
        }
      } else {
        assert.strictEqual(err, value);
      }
    }));

    // test a Promise factory
    function promiseFn() {
      return Promise.reject(value);
    }

    const cbPromiseFn = callbackify(promiseFn);
    cbPromiseFn(common.mustCall((err, ret) => {
      assert.strictEqual(ret, undefined);
      if (err instanceof Error) {
        if ('reason' in err) {
          assert(!value);
          assert.strictEqual(err.code, 'ERR_FALSY_VALUE_REJECTION');
          assert.strictEqual(err.reason, value);
        } else {
          assert.strictEqual(String(value).endsWith(err.message), true);
        }
      } else {
        assert.strictEqual(err, value);
      }
    }));

    // Test Thenable
    function thenableFn() {
      return {
        then(onRes, onRej) {
          onRej(value);
        }
      };
    }

    const cbThenableFn = callbackify(thenableFn);
    cbThenableFn(common.mustCall((err, ret) => {
      assert.strictEqual(ret, undefined);
      if (err instanceof Error) {
        if ('reason' in err) {
          assert(!value);
          assert.strictEqual(err.code, 'ERR_FALSY_VALUE_REJECTION');
          assert.strictEqual(err.reason, value);
        } else {
          assert.strictEqual(String(value).endsWith(err.message), true);
        }
      } else {
        assert.strictEqual(err, value);
      }
    }));
  }
}

{
  // Test that arguments passed to callbackified function are passed to original
  for (const value of values) {
    async function asyncFn(arg) {
      assert.strictEqual(arg, value);
      return await Promise.resolve(arg);
    }

    const cbAsyncFn = callbackify(asyncFn);
    cbAsyncFn(value, common.mustCall((err, ret) => {
      assert.ifError(err);
      assert.strictEqual(ret, value);
    }));

    function promiseFn(arg) {
      assert.strictEqual(arg, value);
      return Promise.resolve(arg);
    }

    const cbPromiseFn = callbackify(promiseFn);
    cbPromiseFn(value, common.mustCall((err, ret) => {
      assert.ifError(err);
      assert.strictEqual(ret, value);
    }));
  }
}

{
  // Test that `this` binding is the same for callbackified and original
  for (const value of values) {
    const iAmThis = {
      fn(arg) {
        assert.strictEqual(this, iAmThis);
        return Promise.resolve(arg);
      },
    };
    iAmThis.cbFn = callbackify(iAmThis.fn);
    iAmThis.cbFn(value, common.mustCall(function(err, ret) {
      assert.ifError(err);
      assert.strictEqual(ret, value);
      assert.strictEqual(this, iAmThis);
    }));

    const iAmThat = {
      async fn(arg) {
        assert.strictEqual(this, iAmThat);
        return await Promise.resolve(arg);
      },
    };
    iAmThat.cbFn = callbackify(iAmThat.fn);
    iAmThat.cbFn(value, common.mustCall(function(err, ret) {
      assert.ifError(err);
      assert.strictEqual(ret, value);
      assert.strictEqual(this, iAmThat);
    }));
  }
}

{
  // Test that callback that throws emits an `uncaughtException` event
  const fixture = fixtures.path('uncaught-exceptions', 'callbackify1.js');
  execFile(
    process.execPath,
    [fixture],
    common.mustCall((err, stdout, stderr) => {
      assert.strictEqual(err.code, 1);
      assert.strictEqual(Object.getPrototypeOf(err).name, 'Error');
      assert.strictEqual(stdout, '');
      const errLines = stderr.trim().split(/[\r\n]+/);
      const errLine = errLines.find((l) => /^Error/.exec(l));
      assert.strictEqual(errLine, `Error: ${fixture}`);
    })
  );
}

{
  // Test that handled `uncaughtException` works and passes rejection reason
  const fixture = fixtures.path('uncaught-exceptions', 'callbackify2.js');
  execFile(
    process.execPath,
    [fixture],
    common.mustCall((err, stdout, stderr) => {
      assert.ifError(err);
      assert.strictEqual(stdout.trim(), fixture);
      assert.strictEqual(stderr, '');
    })
  );
}

{
  // Verify that non-function inputs throw.
  ['foo', null, undefined, false, 0, {}, Symbol(), []].forEach((value) => {
    assert.throws(() => {
      callbackify(value);
    }, common.expectsError({
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message: 'The "original" argument must be of type Function'
    }));
  });
}

{
  async function asyncFn() {
    return await Promise.resolve(42);
  }

  const cb = callbackify(asyncFn);
  const args = [];

  // Verify that the last argument to the callbackified function is a function.
  ['foo', null, undefined, false, 0, {}, Symbol(), []].forEach((value) => {
    args.push(value);
    assert.throws(() => {
      cb(...args);
    }, common.expectsError({
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message: 'The last argument must be of type Function'
    }));
  });
}
