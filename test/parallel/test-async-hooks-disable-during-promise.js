'use strict';
const common = require('../common');
const async_hooks = require('async_hooks');
common.crashOnUnhandledRejection();

const hook = async_hooks.createHook({
  init: common.mustCall(2),
  before: common.mustCall(1),
  after: common.mustNotCall()
}).enable();

Promise.resolve(1).then(common.mustCall(() => {
  hook.disable();

  Promise.resolve(42).then(common.mustCall());

  process.nextTick(common.mustCall());
}));
