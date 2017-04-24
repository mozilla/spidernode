'use strict';

const binding = process.binding('util');
const signals = process.binding('constants').os.signals;

const kArrowMessagePrivateSymbolIndex = binding['arrow_message_private_symbol'];
const kDecoratedPrivateSymbolIndex = binding['decorated_private_symbol'];
const noCrypto = !process.versions.openssl;

function isError(e) {
  return objectToString(e) === '[object Error]' || e instanceof Error;
}

function objectToString(o) {
  return Object.prototype.toString.call(o);
}

// Mark that a method should not be used.
// Returns a modified function which warns once by default.
// If --no-deprecation is set, then it is a no-op.
function deprecate(fn, msg, code) {
  // Allow for deprecating things in the process of starting up.
  if (global.process === undefined) {
    return function(...args) {
      return deprecate(fn, msg).apply(this, args);
    };
  }

  if (process.noDeprecation === true) {
    return fn;
  }

  if (code !== undefined && typeof code !== 'string')
    throw new TypeError('`code` argument must be a string');

  var warned = false;
  function deprecated(...args) {
    if (!warned) {
      warned = true;
      if (code !== undefined) {
        process.emitWarning(msg, 'DeprecationWarning', code, deprecated);
      } else {
        process.emitWarning(msg, 'DeprecationWarning', deprecated);
      }
    }
    if (new.target) {
      return Reflect.construct(fn, args, new.target);
    }
    return fn.apply(this, args);
  }

  // The wrapper will keep the same prototype as fn to maintain prototype chain
  Object.setPrototypeOf(deprecated, fn);
  if (fn.prototype) {
    // Setting this (rather than using Object.setPrototype, as above) ensures
    // that calling the unwrapped constructor gives an instanceof the wrapped
    // constructor.
    deprecated.prototype = fn.prototype;
  }

  return deprecated;
}

function decorateErrorStack(err) {
  if (!(isError(err) && err.stack) ||
      binding.getHiddenValue(err, kDecoratedPrivateSymbolIndex) === true)
    return;

  const arrow = binding.getHiddenValue(err, kArrowMessagePrivateSymbolIndex);

  if (arrow) {
    err.stack = arrow + err.stack;
    binding.setHiddenValue(err, kDecoratedPrivateSymbolIndex, true);
  }
}

function assertCrypto() {
  if (noCrypto)
    throw new Error('Node.js is not compiled with openssl crypto support');
}

// The loop should only run at most twice, retrying with lowercased enc
// if there is no match in the first pass.
// We use a loop instead of branching to retry with a helper
// function in order to avoid the performance hit.
// Return undefined if there is no match.
function normalizeEncoding(enc) {
  if (!enc) return 'utf8';
  var retried;
  while (true) {
    switch (enc) {
      case 'utf8':
      case 'utf-8':
        return 'utf8';
      case 'ucs2':
      case 'ucs-2':
      case 'utf16le':
      case 'utf-16le':
        return 'utf16le';
      case 'latin1':
      case 'binary':
        return 'latin1';
      case 'base64':
      case 'ascii':
      case 'hex':
        return enc;
      default:
        if (retried) return; // undefined
        enc = ('' + enc).toLowerCase();
        retried = true;
    }
  }
}

function filterDuplicateStrings(items, low) {
  const map = new Map();
  for (var i = 0; i < items.length; i++) {
    const item = items[i];
    const key = item.toLowerCase();
    if (low) {
      map.set(key, key);
    } else {
      map.set(key, item);
    }
  }
  return Array.from(map.values()).sort();
}

function cachedResult(fn) {
  var result;
  return () => {
    if (result === undefined)
      result = fn();
    return result.slice();
  };
}

// Useful for Wrapping an ES6 Class with a constructor Function that
// does not require the new keyword. For instance:
//   class A { constructor(x) {this.x = x;}}
//   const B = createClassWrapper(A);
//   B() instanceof A // true
//   B() instanceof B // true
function createClassWrapper(type) {
  const fn = function(...args) {
    return Reflect.construct(type, args, new.target || type);
  };
  // Mask the wrapper function name and length values
  Object.defineProperties(fn, {
    name: {value: type.name},
    length: {value: type.length}
  });
  Object.setPrototypeOf(fn, type);
  fn.prototype = type.prototype;
  return fn;
}

let signalsToNamesMapping;
function getSignalsToNamesMapping() {
  if (signalsToNamesMapping !== undefined)
    return signalsToNamesMapping;

  signalsToNamesMapping = Object.create(null);
  for (const key in signals) {
    signalsToNamesMapping[signals[key]] = key;
  }

  return signalsToNamesMapping;
}

function convertToValidSignal(signal) {
  if (typeof signal === 'number' && getSignalsToNamesMapping()[signal])
    return signal;

  if (typeof signal === 'string') {
    const signalName = signals[signal.toUpperCase()];
    if (signalName) return signalName;
  }

  throw new Error('Unknown signal: ' + signal);
}

module.exports = exports = {
  assertCrypto,
  cachedResult,
  convertToValidSignal,
  createClassWrapper,
  decorateErrorStack,
  deprecate,
  filterDuplicateStrings,
  isError,
  normalizeEncoding,
  objectToString,

  // Symbol used to provide a custom inspect function for an object as an
  // alternative to using 'inspect'
  customInspectSymbol: Symbol('util.inspect.custom'),

  // Used by the buffer module to capture an internal reference to the
  // default isEncoding implementation, just in case userland overrides it.
  kIsEncodingSymbol: Symbol('node.isEncoding')
};
