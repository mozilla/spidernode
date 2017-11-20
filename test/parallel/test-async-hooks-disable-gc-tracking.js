'use strict';
// Flags: --expose_gc

// This test ensures that userland-only AsyncResources cause a destroy event to
// be emitted when they get gced.

const common = require('../common');
const async_hooks = require('async_hooks');

const hook = async_hooks.createHook({
  destroy: common.mustCall(1) // only 1 immediate is destroyed
}).enable();

new async_hooks.AsyncResource('foobar', { requireManualDestroy: true });

setImmediate(() => {
  global.gc();
  setImmediate(() => {
    hook.disable();
  });
});
