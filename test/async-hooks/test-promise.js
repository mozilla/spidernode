'use strict';

const common = require('../common');

const assert = require('assert');
const initHooks = require('./init-hooks');
const { checkInvocations } = require('./hook-checks');

common.crashOnUnhandledRejection();

const hooks = initHooks();

hooks.enable();

const p = (new Promise(common.mustCall(executor)));
p.then(afterresolution);

function executor(resolve, reject) {
  const as = hooks.activitiesOfTypes('PROMISE');
  assert.strictEqual(as.length, 1);
  const a = as[0];
  checkInvocations(a, { init: 1 }, 'while in promise executor');
  resolve(5);
}

function afterresolution(val) {
  assert.strictEqual(val, 5);
  const as = hooks.activitiesOfTypes('PROMISE');
  assert.strictEqual(as.length, 2);
  checkInvocations(as[0], { init: 1 }, 'after resolution parent promise');
  checkInvocations(as[1], { init: 1, before: 1 },
                   'after resolution child promise');
}

process.on('exit', onexit);
function onexit() {
  hooks.disable();
  hooks.sanityCheck('PROMISE');

  const as = hooks.activitiesOfTypes('PROMISE');
  assert.strictEqual(as.length, 2);

  const a0 = as[0];
  assert.strictEqual(a0.type, 'PROMISE');
  assert.strictEqual(typeof a0.uid, 'number');
  assert.strictEqual(a0.triggerAsyncId, 1);
  checkInvocations(a0, { init: 1 }, 'when process exits');

  const a1 = as[1];
  assert.strictEqual(a1.type, 'PROMISE');
  assert.strictEqual(typeof a1.uid, 'number');
  assert.strictEqual(a1.triggerAsyncId, a0.uid);
  // We expect a destroy hook as well but we cannot guarantee predictable gc.
  checkInvocations(a1, { init: 1, before: 1, after: 1 }, 'when process exits');
}
