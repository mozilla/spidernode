'use strict';

const errors = require('internal/errors');
const async_wrap = process.binding('async_wrap');
/* async_hook_fields is a Uint32Array wrapping the uint32_t array of
 * Environment::AsyncHooks::fields_[]. Each index tracks the number of active
 * hooks for each type.
 *
 * async_id_fields is a Float64Array wrapping the double array of
 * Environment::AsyncHooks::async_id_fields_[]. Each index contains the ids for
 * the various asynchronous states of the application. These are:
 *  kExecutionAsyncId: The async_id assigned to the resource responsible for the
 *    current execution stack.
 *  kTriggerAsyncId: The trigger_async_id of the resource responsible for
 *    the current execution stack.
 *  kAsyncIdCounter: Incremental counter tracking the next assigned async_id.
 *  kInitTriggerAsyncId: Written immediately before a resource's constructor
 *    that sets the value of the init()'s triggerAsyncId. The order of
 *    retrieving the triggerAsyncId value is passing directly to the
 *    constructor -> value set in kInitTriggerAsyncId -> executionAsyncId of
 *    the current resource.
 */
const { async_hook_fields, async_id_fields } = async_wrap;
// Store the pair executionAsyncId and triggerAsyncId in a std::stack on
// Environment::AsyncHooks::ids_stack_ tracks the resource responsible for the
// current execution stack. This is unwound as each resource exits. In the case
// of a fatal exception this stack is emptied after calling each hook's after()
// callback.
const { pushAsyncIds, popAsyncIds } = async_wrap;
// For performance reasons, only track Proimses when a hook is enabled.
const { enablePromiseHook, disablePromiseHook } = async_wrap;
// Properties in active_hooks are used to keep track of the set of hooks being
// executed in case another hook is enabled/disabled. The new set of hooks is
// then restored once the active set of hooks is finished executing.
const active_hooks = {
  // Array of all AsyncHooks that will be iterated whenever an async event
  // fires. Using var instead of (preferably const) in order to assign
  // active_hooks.tmp_array if a hook is enabled/disabled during hook
  // execution.
  array: [],
  // Use a counter to track nested calls of async hook callbacks and make sure
  // the active_hooks.array isn't altered mid execution.
  call_depth: 0,
  // Use to temporarily store and updated active_hooks.array if the user
  // enables or disables a hook while hooks are being processed. If a hook is
  // enabled() or disabled() during hook execution then the current set of
  // active hooks is duplicated and set equal to active_hooks.tmp_array. Any
  // subsequent changes are on the duplicated array. When all hooks have
  // completed executing active_hooks.tmp_array is assigned to
  // active_hooks.array.
  tmp_array: null,
  // Keep track of the field counts held in active_hooks.tmp_array. Because the
  // async_hook_fields can't be reassigned, store each uint32 in an array that
  // is written back to async_hook_fields when active_hooks.array is restored.
  tmp_fields: null
};


// Each constant tracks how many callbacks there are for any given step of
// async execution. These are tracked so if the user didn't include callbacks
// for a given step, that step can bail out early.
const { kInit, kBefore, kAfter, kDestroy, kPromiseResolve,
        kCheck, kExecutionAsyncId, kAsyncIdCounter,
        kInitTriggerAsyncId } = async_wrap.constants;

// Used in AsyncHook and AsyncResource.
const init_symbol = Symbol('init');
const before_symbol = Symbol('before');
const after_symbol = Symbol('after');
const destroy_symbol = Symbol('destroy');
const promise_resolve_symbol = Symbol('promiseResolve');
const emitBeforeNative = emitHookFactory(before_symbol, 'emitBeforeNative');
const emitAfterNative = emitHookFactory(after_symbol, 'emitAfterNative');
const emitDestroyNative = emitHookFactory(destroy_symbol, 'emitDestroyNative');
const emitPromiseResolveNative =
    emitHookFactory(promise_resolve_symbol, 'emitPromiseResolveNative');

// TODO(refack): move to node-config.cc
const abort_regex = /^--abort[_-]on[_-]uncaught[_-]exception$/;

// Setup the callbacks that node::AsyncWrap will call when there are hooks to
// process. They use the same functions as the JS embedder API. These callbacks
// are setup immediately to prevent async_wrap.setupHooks() from being hijacked
// and the cost of doing so is negligible.
async_wrap.setupHooks({ init: emitInitNative,
                        before: emitBeforeNative,
                        after: emitAfterNative,
                        destroy: emitDestroyNative,
                        promise_resolve: emitPromiseResolveNative });

// Used to fatally abort the process if a callback throws.
function fatalError(e) {
  if (typeof e.stack === 'string') {
    process._rawDebug(e.stack);
  } else {
    const o = { message: e };
    Error.captureStackTrace(o, fatalError);
    process._rawDebug(o.stack);
  }
  if (process.execArgv.some((e) => abort_regex.test(e))) {
    process.abort();
  }
  process.exit(1);
}


function validateAsyncId(asyncId, type) {
  // Skip validation when async_hooks is disabled
  if (async_hook_fields[kCheck] <= 0) return;

  if (!Number.isSafeInteger(asyncId) || asyncId < -1) {
    fatalError(new errors.RangeError('ERR_INVALID_ASYNC_ID', type, asyncId));
  }
}

// Emit From Native //

// Used by C++ to call all init() callbacks. Because some state can be setup
// from C++ there's no need to perform all the same operations as in
// emitInitScript.
function emitInitNative(asyncId, type, triggerAsyncId, resource) {
  active_hooks.call_depth += 1;
  // Use a single try/catch for all hook to avoid setting up one per iteration.
  try {
    for (var i = 0; i < active_hooks.array.length; i++) {
      if (typeof active_hooks.array[i][init_symbol] === 'function') {
        active_hooks.array[i][init_symbol](
          asyncId, type, triggerAsyncId,
          resource
        );
      }
    }
  } catch (e) {
    fatalError(e);
  } finally {
    active_hooks.call_depth -= 1;
  }

  // Hooks can only be restored if there have been no recursive hook calls.
  // Also the active hooks do not need to be restored if enable()/disable()
  // weren't called during hook execution, in which case active_hooks.tmp_array
  // will be null.
  if (active_hooks.call_depth === 0 && active_hooks.tmp_array !== null) {
    restoreActiveHooks();
  }
}


function emitHookFactory(symbol, name) {
  // Called from native. The asyncId stack handling is taken care of there
  // before this is called.
  // eslint-disable-next-line func-style
  const fn = function(asyncId) {
    active_hooks.call_depth += 1;
    // Use a single try/catch for all hook to avoid setting up one per
    // iteration.
    try {
      for (var i = 0; i < active_hooks.array.length; i++) {
        if (typeof active_hooks.array[i][symbol] === 'function') {
          active_hooks.array[i][symbol](asyncId);
        }
      }
    } catch (e) {
      fatalError(e);
    } finally {
      active_hooks.call_depth -= 1;
    }

    // Hooks can only be restored if there have been no recursive hook calls.
    // Also the active hooks do not need to be restored if enable()/disable()
    // weren't called during hook execution, in which case
    // active_hooks.tmp_array will be null.
    if (active_hooks.call_depth === 0 && active_hooks.tmp_array !== null) {
      restoreActiveHooks();
    }
  };

  // Set the name property of the anonymous function as it looks good in the
  // stack trace.
  Object.defineProperty(fn, 'name', {
    value: name
  });
  return fn;
}

// Manage Active Hooks //

function getHookArrays() {
  if (active_hooks.call_depth === 0)
    return [active_hooks.array, async_hook_fields];
  // If this hook is being enabled while in the middle of processing the array
  // of currently active hooks then duplicate the current set of active hooks
  // and store this there. This shouldn't fire until the next time hooks are
  // processed.
  if (active_hooks.tmp_array === null)
    storeActiveHooks();
  return [active_hooks.tmp_array, active_hooks.tmp_fields];
}


function storeActiveHooks() {
  active_hooks.tmp_array = active_hooks.array.slice();
  // Don't want to make the assumption that kInit to kDestroy are indexes 0 to
  // 4. So do this the long way.
  active_hooks.tmp_fields = [];
  active_hooks.tmp_fields[kInit] = async_hook_fields[kInit];
  active_hooks.tmp_fields[kBefore] = async_hook_fields[kBefore];
  active_hooks.tmp_fields[kAfter] = async_hook_fields[kAfter];
  active_hooks.tmp_fields[kDestroy] = async_hook_fields[kDestroy];
  active_hooks.tmp_fields[kPromiseResolve] = async_hook_fields[kPromiseResolve];
}


// Then restore the correct hooks array in case any hooks were added/removed
// during hook callback execution.
function restoreActiveHooks() {
  active_hooks.array = active_hooks.tmp_array;
  async_hook_fields[kInit] = active_hooks.tmp_fields[kInit];
  async_hook_fields[kBefore] = active_hooks.tmp_fields[kBefore];
  async_hook_fields[kAfter] = active_hooks.tmp_fields[kAfter];
  async_hook_fields[kDestroy] = active_hooks.tmp_fields[kDestroy];
  async_hook_fields[kPromiseResolve] = active_hooks.tmp_fields[kPromiseResolve];

  active_hooks.tmp_array = null;
  active_hooks.tmp_fields = null;
}


function enableHooks() {
  enablePromiseHook();
  async_hook_fields[kCheck] += 1;
}

function disableHooks() {
  disablePromiseHook();
  async_hook_fields[kCheck] -= 1;
}

// Internal Embedder API //

// Increment the internal id counter and return the value. Important that the
// counter increment first. Since it's done the same way in
// Environment::new_async_uid()
function newUid() {
  return ++async_id_fields[kAsyncIdCounter];
}


// Return the triggerAsyncId meant for the constructor calling it. It's up to
// the user to safeguard this call and make sure it's zero'd out when the
// constructor is complete.
function initTriggerId() {
  var triggerAsyncId = async_id_fields[kInitTriggerAsyncId];
  // Reset value after it's been called so the next constructor doesn't
  // inherit it by accident.
  async_id_fields[kInitTriggerAsyncId] = 0;
  if (triggerAsyncId <= 0)
    triggerAsyncId = async_id_fields[kExecutionAsyncId];
  return triggerAsyncId;
}


function setInitTriggerId(triggerAsyncId) {
  // CHECK(Number.isSafeInteger(triggerAsyncId))
  // CHECK(triggerAsyncId > 0)
  async_id_fields[kInitTriggerAsyncId] = triggerAsyncId;
}


function emitInitScript(asyncId, type, triggerAsyncId, resource) {
  validateAsyncId(asyncId, 'asyncId');
  if (triggerAsyncId !== null)
    validateAsyncId(triggerAsyncId, 'triggerAsyncId');
  if (async_hook_fields[kCheck] > 0 &&
      (typeof type !== 'string' || type.length <= 0)) {
    throw new errors.TypeError('ERR_ASYNC_TYPE', type);
  }

  // Short circuit all checks for the common case. Which is that no hooks have
  // been set. Do this to remove performance impact for embedders (and core).
  if (async_hook_fields[kInit] === 0)
    return;

  // This can run after the early return check b/c running this function
  // manually means that the embedder must have used initTriggerId().
  if (triggerAsyncId === null) {
    triggerAsyncId = initTriggerId();
  } else {
    // If a triggerAsyncId was passed, any kInitTriggerAsyncId still must be
    // null'd.
    async_id_fields[kInitTriggerAsyncId] = 0;
  }

  emitInitNative(asyncId, type, triggerAsyncId, resource);
}


function emitBeforeScript(asyncId, triggerAsyncId) {
  // Validate the ids. An id of -1 means it was never set and is visible on the
  // call graph. An id < -1 should never happen in any circumstance. Throw
  // on user calls because async state should still be recoverable.
  validateAsyncId(asyncId, 'asyncId');
  validateAsyncId(triggerAsyncId, 'triggerAsyncId');

  pushAsyncIds(asyncId, triggerAsyncId);

  if (async_hook_fields[kBefore] > 0)
    emitBeforeNative(asyncId);
}


function emitAfterScript(asyncId) {
  validateAsyncId(asyncId, 'asyncId');

  if (async_hook_fields[kAfter] > 0)
    emitAfterNative(asyncId);

  popAsyncIds(asyncId);
}


function emitDestroyScript(asyncId) {
  validateAsyncId(asyncId, 'asyncId');

  // Return early if there are no destroy callbacks, or invalid asyncId.
  if (async_hook_fields[kDestroy] === 0 || asyncId <= 0)
    return;
  async_wrap.queueDestroyAsyncId(asyncId);
}


module.exports = {
  // Private API
  getHookArrays,
  symbols: {
    init_symbol, before_symbol, after_symbol, destroy_symbol,
    promise_resolve_symbol
  },
  enableHooks,
  disableHooks,
  // Internal Embedder API
  newUid,
  initTriggerId,
  setInitTriggerId,
  emitInit: emitInitScript,
  emitBefore: emitBeforeScript,
  emitAfter: emitAfterScript,
  emitDestroy: emitDestroyScript,
};
