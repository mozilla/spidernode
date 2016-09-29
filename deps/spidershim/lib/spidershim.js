// Copyright Mozilla Foundation. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.

(function() {
  // Save original builtIns
  var
    Function_prototype_toString = Function.prototype.toString,
    Object_defineProperty = Object.defineProperty,
    Object_getOwnPropertyDescriptor = Object.getOwnPropertyDescriptor,
    Object_setPrototypeOf = Object.setPrototypeOf,
    Reflect_construct = Reflect.construct;
  var BuiltInError = Error;
  var global = this;

  // Simulate V8 JavaScript stack trace API
  function StackFrame(func, funcName, fileName, lineNumber, columnNumber) {
    this.column = columnNumber;
    this.lineNumber = lineNumber;
    this.scriptName = fileName;
    this.functionName = funcName;
    this.function = func;
  }

  StackFrame.prototype.getFunction = function() {
    // TODO: Fix if .stack is called from different callsite
    // from where Error() or Error.captureStackTrace was called
    return this.function;
  };

  StackFrame.prototype.getTypeName = function() {
    //TODO : Fix this
    return this.functionName;
  };

  StackFrame.prototype.getMethodName = function() {
    return this.functionName;
  };

  StackFrame.prototype.getFunctionName = function() {
    return this.functionName;
  };

  StackFrame.prototype.getFileName = function() {
    return this.scriptName;
  };

  StackFrame.prototype.getLineNumber = function() {
    return this.lineNumber;
  };

  StackFrame.prototype.getColumnNumber = function() {
    return this.column;
  };

  StackFrame.prototype.isEval = function() {
    // TODO
    return false;
  };

  StackFrame.prototype.isToplevel = function() {
    // TODO
    return false;
  };

  StackFrame.prototype.isNative = function() {
    // TODO
    return false;
  };

  StackFrame.prototype.isConstructor = function() {
    // TODO
    return false;
  };

  StackFrame.prototype.toString = function() {
    return (this.functionName || 'Anonymous function') + ' (' +
      this.scriptName + ':' + this.lineNumber + ':' + this.column + ')';
  };

  // default StackTrace stringify function
  function prepareStackTrace(error, stack) {
    var stackString = (error.name || 'Error') + ': ' + (error.message || '');

    for (var i = 0; i < stack.length; i++) {
      stackString += '\n   at ' + stack[i].toString();
    }

    return stackString;
  }

  // Parse 'stack' string into StackTrace frames. Skip top 'skipDepth' frames,
  // and optionally skip top to 'startName' function frames.
  function parseStack(stack, skipDepth, startName) {
    var stackSplitter = "\n";
    var reStackDetails = /^([^@]*)@([^:]*):(\d+):(\d+)$/;

    var curr = parseStack;
    var splittedStack = stack.split(stackSplitter);
    var errstack = [];

    for (var i = 0; i < splittedStack.length; i++) {
      // parseStack has 1 frame lesser than skipDepth. So skip calling .caller
      // once. After that, continue calling .caller
      if (skipDepth != 1 && curr) {
        try {
          curr = curr.caller;
        } catch (e) {
          curr = undefined;  // .caller might not be allowed in curr's context
        }
      }

      if (skipDepth-- > 0) {
        continue;
      }

      var func = curr;
      var stackDetails = reStackDetails.exec(splittedStack[i]);
      if (!stackDetails) {
        continue;
      }
      var funcName = stackDetails[1];

      if (startName) {
        if (funcName === startName) {
          startName = undefined;
        }
        continue;
      }
      if (funcName === 'Anonymous function') {
        funcName = null;
      }

      var fileName = stackDetails[2];
      var lineNumber = stackDetails[3];
      var columnNumber = stackDetails[4];

      errstack.push(
          new StackFrame(func, funcName, fileName, lineNumber, columnNumber));
    }
    return errstack;
  }

  function findFuncDepth(func) {
    if (!func) {
      return 0;
    }

    try {
      var curr = privateCaptureStackTrace.caller;
      var limit = Error.stackTraceLimit;
      var skipDepth = 0;
      while (curr) {
        skipDepth++;
        if (curr === func) {
          return skipDepth;
        }
        if (skipDepth > limit) {
          return 0;
        }
        curr = curr.caller;
      }
    } catch (e) {
      // Strict mode may throw on .caller. Will try to match by function name.
      return -1;
    }

    return 0;
  }

  function withStackTraceLimitOffset(offset, f) {
    var oldLimit = BuiltInError.stackTraceLimit;
    if (typeof oldLimit === 'number') {
      BuiltInError.stackTraceLimit = oldLimit + offset;
    }
    try {
      return f();
    } finally {
      BuiltInError.stackTraceLimit = oldLimit;
    }
  }

  function captureStackTrace(err, func) {
    // skip 3 frames: lambda, withStackTraceLimitOffset, this frame
    return privateCaptureStackTrace(err, func,
      withStackTraceLimitOffset(3, () => new BuiltInError()),
      3);
  }

  // private captureStackTrace implementation
  //  err, func -- args from Error.captureStackTrace
  //  e -- a new Error object which already captured stack
  //  skipDepth -- known number of top frames to be skipped
  function privateCaptureStackTrace(err, func, e, skipDepth) {
    var currentStack = e;
    var isPrepared = false;
    var originalStack = e.stack;

    var funcSkipDepth = findFuncDepth(func);
    var startFuncName = (func && funcSkipDepth < 0) ? func.name : undefined;
    skipDepth += Math.max(funcSkipDepth - 1, 0);

    var currentStackTrace;
    function ensureStackTrace() {
      if (!currentStackTrace) {
        currentStackTrace = parseStack(
          originalStack || '',
          skipDepth, startFuncName);
      }
      return currentStackTrace;
    }

    function stackGetter() {
      if (!isPrepared) {
        var prep = Error.prepareStackTrace || prepareStackTrace;
        stackSetter(prep(err, ensureStackTrace()));
      }
      return currentStack;
    }
    function stackSetter(value) {
      currentStack = value;
      isPrepared = true;
    }

    Object_defineProperty(err, 'stack', {
      get: stackGetter, set: stackSetter, configurable: true, enumerable: false
    });
  }

  // patch Error types to hook with Error.captureStackTrace/prepareStackTrace
  function patchErrorTypes() {
    // patch all these builtin Error types
    [
      Error, EvalError, RangeError, ReferenceError, SyntaxError, TypeError,
      URIError
    ].forEach(function(type) {
      var newType = function __newType() {
        var e = withStackTraceLimitOffset(
          3, () => Reflect_construct(type, arguments, new.target || newType));
        // skip 3 frames: lambda, withStackTraceLimitOffset, this frame
        privateCaptureStackTrace(e, undefined, e, 3);
        return e;
      };

      Object_defineProperty(newType, 'name', {
        value: type.name,
        writable: false, enumerable: false, configurable: true
      });
      newType.prototype = type.prototype;
      newType.prototype.constructor = newType;
      if (type !== BuiltInError) {
        Object_setPrototypeOf(newType, Error);
      }
      global[type.name] = newType;
    });

    // Delegate Error.stackTraceLimit to builtin Error constructor
    Object_defineProperty(Error, 'stackTraceLimit', {
      enumerable: false,
      configurable: true,
      get: function() { return BuiltInError.stackTraceLimit; },
      set: function(value) { BuiltInError.stackTraceLimit = value; }
    });
  }

  function patchErrorStack() {
    Error.captureStackTrace = captureStackTrace;
  }

  patchErrorTypes();
  patchErrorStack();
})();
