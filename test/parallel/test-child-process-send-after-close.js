'use strict';
const common = require('../common');
const assert = require('assert');
const cp = require('child_process');
const path = require('path');
const fixture = path.join(common.fixturesDir, 'empty.js');
const child = cp.fork(fixture);

child.on('close', common.mustCall((code, signal) => {
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);

  const testError = common.expectsError({
    type: Error,
    message: 'Channel closed',
    code: 'ERR_IPC_CHANNEL_CLOSED'
  });

  child.on('error', common.mustCall(testError));

  {
    const result = child.send('ping');
    assert.strictEqual(result, false);
  }

  {
    const result = child.send('pong', common.mustCall(testError));
    assert.strictEqual(result, false);
  }
}));
