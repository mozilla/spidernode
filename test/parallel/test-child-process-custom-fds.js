'use strict';
const common = require('../common');
const assert = require('assert');

// Verify that customFds is used if stdio is not provided.
{
  const msg = 'child_process: options.customFds option is deprecated. ' +
              'Use options.stdio instead.';
  common.expectWarning('DeprecationWarning', msg);

  const customFds = [-1, process.stdout.fd, process.stderr.fd];
  const child = common.spawnSyncPwd({ customFds });

  assert.deepStrictEqual(child.options.customFds, customFds);
  assert.deepStrictEqual(child.options.stdio, [
    { type: 'pipe', readable: true, writable: false },
    { type: 'fd', fd: process.stdout.fd },
    { type: 'fd', fd: process.stderr.fd }
  ]);
}

// Verify that customFds is ignored when stdio is present.
{
  const customFds = [0, 1, 2];
  const child = common.spawnSyncPwd({ customFds, stdio: 'pipe' });

  assert.deepStrictEqual(child.options.customFds, customFds);
  assert.deepStrictEqual(child.options.stdio, [
    { type: 'pipe', readable: true, writable: false },
    { type: 'pipe', readable: false, writable: true },
    { type: 'pipe', readable: false, writable: true }
  ]);
}
