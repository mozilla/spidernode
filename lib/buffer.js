// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

'use strict';

const binding = process.binding('buffer');
const config = process.binding('config');
const { compare: compare_, compareOffset } = binding;
const { isAnyArrayBuffer, isUint8Array } = process.binding('util');
const bindingObj = {};
const internalUtil = require('internal/util');
const pendingDeprecation = !!config.pendingDeprecation;

class FastBuffer extends Uint8Array {
  constructor(arg1, arg2, arg3) {
    super(arg1, arg2, arg3);
  }
}
FastBuffer.prototype.constructor = Buffer;

Buffer.prototype = FastBuffer.prototype;

exports.Buffer = Buffer;
exports.SlowBuffer = SlowBuffer;
exports.INSPECT_MAX_BYTES = 50;
exports.kMaxLength = binding.kMaxLength;

const kFromErrorMsg = 'First argument must be a string, Buffer, ' +
                      'ArrayBuffer, Array, or array-like object.';

Buffer.poolSize = 8 * 1024;
var poolSize, poolOffset, allocPool;


binding.setupBufferJS(Buffer.prototype, bindingObj);

// |binding.zeroFill| can be undefined when running inside an isolate where we
// do not own the ArrayBuffer allocator.  Zero fill is always on in that case.
const zeroFill = bindingObj.zeroFill || [0];

function createUnsafeBuffer(size) {
  return new FastBuffer(createUnsafeArrayBuffer(size));
}

function createUnsafeArrayBuffer(size) {
  zeroFill[0] = 0;
  try {
    return new ArrayBuffer(size);
  } finally {
    zeroFill[0] = 1;
  }
}

function createPool() {
  poolSize = Buffer.poolSize;
  allocPool = createUnsafeArrayBuffer(poolSize);
  poolOffset = 0;
}
createPool();


function alignPool() {
  // Ensure aligned slices
  if (poolOffset & 0x7) {
    poolOffset |= 0x7;
    poolOffset++;
  }
}

var bufferWarn = true;
const bufferWarning = 'The Buffer() and new Buffer() constructors are not ' +
                      'recommended for use due to security and usability ' +
                      'concerns. Please use the new Buffer.alloc(), ' +
                      'Buffer.allocUnsafe(), or Buffer.from() construction ' +
                      'methods instead.';

function showFlaggedDeprecation() {
  if (bufferWarn) {
    // This is a *pending* deprecation warning. It is not emitted by
    // default unless the --pending-deprecation command-line flag is
    // used or the NODE_PENDING_DEPRECATION=1 envvar is set.
    process.emitWarning(bufferWarning, 'DeprecationWarning', 'DEP0005');
    bufferWarn = false;
  }
}

const doFlaggedDeprecation =
  pendingDeprecation ?
    showFlaggedDeprecation :
    function() {};

/**
 * The Buffer() construtor is deprecated in documentation and should not be
 * used moving forward. Rather, developers should use one of the three new
 * factory APIs: Buffer.from(), Buffer.allocUnsafe() or Buffer.alloc() based on
 * their specific needs. There is no runtime deprecation because of the extent
 * to which the Buffer constructor is used in the ecosystem currently -- a
 * runtime deprecation would introduce too much breakage at this time. It's not
 * likely that the Buffer constructors would ever actually be removed.
 * Deprecation Code: DEP0005
 **/
function Buffer(arg, encodingOrOffset, length) {
  doFlaggedDeprecation();
  // Common case.
  if (typeof arg === 'number') {
    if (typeof encodingOrOffset === 'string') {
      throw new Error(
        'If encoding is specified then the first argument must be a string'
      );
    }
    return Buffer.alloc(arg);
  }
  return Buffer.from(arg, encodingOrOffset, length);
}

Object.defineProperty(Buffer, Symbol.species, {
  enumerable: false,
  configurable: true,
  get() { return FastBuffer; }
});

/**
 * Functionally equivalent to Buffer(arg, encoding) but throws a TypeError
 * if value is a number.
 * Buffer.from(str[, encoding])
 * Buffer.from(array)
 * Buffer.from(buffer)
 * Buffer.from(arrayBuffer[, byteOffset[, length]])
 **/
Buffer.from = function(value, encodingOrOffset, length) {
  if (typeof value === 'string')
    return fromString(value, encodingOrOffset);

  if (isAnyArrayBuffer(value))
    return fromArrayBuffer(value, encodingOrOffset, length);

  var b = fromObject(value);
  if (b)
    return b;

  if (typeof value === 'number')
    throw new TypeError('"value" argument must not be a number');
  throw new TypeError(kFromErrorMsg);
};

Object.setPrototypeOf(Buffer, Uint8Array);

// The 'assertSize' method will remove itself from the callstack when an error
// occurs. This is done simply to keep the internal details of the
// implementation from bleeding out to users.
function assertSize(size) {
  let err = null;

  if (typeof size !== 'number')
    err = new TypeError('"size" argument must be a number');
  else if (size < 0)
    err = new RangeError('"size" argument must not be negative');
  else if (size > binding.kMaxLength)
    err = new RangeError('"size" argument must not be larger ' +
                         'than ' + binding.kMaxLength);

  if (err) {
    Error.captureStackTrace(err, assertSize);
    throw err;
  }
}

/**
 * Creates a new filled Buffer instance.
 * alloc(size[, fill[, encoding]])
 **/
Buffer.alloc = function(size, fill, encoding) {
  assertSize(size);
  if (size > 0 && fill !== undefined) {
    // Since we are filling anyway, don't zero fill initially.
    // Only pay attention to encoding if it's a string. This
    // prevents accidentally sending in a number that would
    // be interpretted as a start offset.
    if (typeof encoding !== 'string')
      encoding = undefined;
    return createUnsafeBuffer(size).fill(fill, encoding);
  }
  return new FastBuffer(size);
};

/**
 * Equivalent to Buffer(num), by default creates a non-zero-filled Buffer
 * instance. If `--zero-fill-buffers` is set, will zero-fill the buffer.
 **/
Buffer.allocUnsafe = function(size) {
  assertSize(size);
  return allocate(size);
};

/**
 * Equivalent to SlowBuffer(num), by default creates a non-zero-filled
 * Buffer instance that is not allocated off the pre-initialized pool.
 * If `--zero-fill-buffers` is set, will zero-fill the buffer.
 **/
Buffer.allocUnsafeSlow = function(size) {
  assertSize(size);
  return createUnsafeBuffer(size);
};

// If --zero-fill-buffers command line argument is set, a zero-filled
// buffer is returned.
function SlowBuffer(length) {
  // eslint-disable-next-line eqeqeq
  if (+length != length)
    length = 0;
  assertSize(+length);
  return createUnsafeBuffer(+length);
}

Object.setPrototypeOf(SlowBuffer.prototype, Uint8Array.prototype);
Object.setPrototypeOf(SlowBuffer, Uint8Array);


function allocate(size) {
  if (size <= 0) {
    return new FastBuffer();
  }
  if (size < (Buffer.poolSize >>> 1)) {
    if (size > (poolSize - poolOffset))
      createPool();
    var b = new FastBuffer(allocPool, poolOffset, size);
    poolOffset += size;
    alignPool();
    return b;
  } else {
    return createUnsafeBuffer(size);
  }
}


function fromString(string, encoding) {
  var length;
  if (typeof encoding !== 'string' || encoding.length === 0) {
    encoding = 'utf8';
    if (string.length === 0)
      return new FastBuffer();
    length = binding.byteLengthUtf8(string);
  } else {
    length = byteLength(string, encoding, true);
    if (length === -1)
      throw new TypeError('"encoding" must be a valid string encoding');
    if (string.length === 0)
      return new FastBuffer();
  }

  if (length >= (Buffer.poolSize >>> 1))
    return binding.createFromString(string, encoding);

  if (length > (poolSize - poolOffset))
    createPool();
  var b = new FastBuffer(allocPool, poolOffset, length);
  const actual = b.write(string, encoding);
  if (actual !== length) {
    // byteLength() may overestimate. That's a rare case, though.
    b = new FastBuffer(allocPool, poolOffset, actual);
  }
  poolOffset += actual;
  alignPool();
  return b;
}

function fromArrayLike(obj) {
  const length = obj.length;
  const b = allocate(length);
  for (var i = 0; i < length; i++)
    b[i] = obj[i];
  return b;
}

function fromArrayBuffer(obj, byteOffset, length) {
  // convert byteOffset to integer
  if (byteOffset === undefined) {
    byteOffset = 0;
  } else {
    byteOffset = +byteOffset;
    // check for NaN
    if (byteOffset !== byteOffset)
      byteOffset = 0;
  }

  const maxLength = obj.byteLength - byteOffset;

  if (maxLength < 0)
    throw new RangeError("'offset' is out of bounds");

  if (length === undefined) {
    length = maxLength;
  } else {
    // convert length to non-negative integer
    length = +length;
    // Check for NaN
    if (length !== length) {
      length = 0;
    } else if (length > 0) {
      length = (length < Number.MAX_SAFE_INTEGER ?
                length : Number.MAX_SAFE_INTEGER);
      if (length > maxLength)
        throw new RangeError("'length' is out of bounds");
    } else {
      length = 0;
    }
  }

  return new FastBuffer(obj, byteOffset, length);
}

function fromObject(obj) {
  if (isUint8Array(obj)) {
    const b = allocate(obj.length);

    if (b.length === 0)
      return b;

    binding.copy(obj, b, 0, 0, obj.length);
    return b;
  }

  if (obj != null) {
    if (obj.length !== undefined || isAnyArrayBuffer(obj.buffer)) {
      if (typeof obj.length !== 'number' || obj.length !== obj.length) {
        return new FastBuffer();
      }
      return fromArrayLike(obj);
    }

    if (obj.type === 'Buffer' && Array.isArray(obj.data)) {
      return fromArrayLike(obj.data);
    }
  }
}


// Static methods

Buffer.isBuffer = function isBuffer(b) {
  return b instanceof Buffer;
};


Buffer.compare = function compare(a, b) {
  if (!isUint8Array(a) || !isUint8Array(b)) {
    throw new TypeError('Arguments must be Buffers or Uint8Arrays');
  }

  if (a === b) {
    return 0;
  }

  return binding.compare(a, b);
};


Buffer.isEncoding = function(encoding) {
  return typeof encoding === 'string' &&
         typeof internalUtil.normalizeEncoding(encoding) === 'string';
};
Buffer[internalUtil.kIsEncodingSymbol] = Buffer.isEncoding;

const kConcatErrMsg = '"list" argument must be an Array ' +
                      'of Buffer or Uint8Array instances';

Buffer.concat = function(list, length) {
  var i;
  if (!Array.isArray(list))
    throw new TypeError(kConcatErrMsg);

  if (list.length === 0)
    return new FastBuffer();

  if (length === undefined) {
    length = 0;
    for (i = 0; i < list.length; i++)
      length += list[i].length;
  } else {
    length = length >>> 0;
  }

  var buffer = Buffer.allocUnsafe(length);
  var pos = 0;
  for (i = 0; i < list.length; i++) {
    var buf = list[i];
    if (!isUint8Array(buf))
      throw new TypeError(kConcatErrMsg);
    binding.copy(buf, buffer, pos);
    pos += buf.length;
  }

  // Note: `length` is always equal to `buffer.length` at this point
  if (pos < length) {
    // Zero-fill the remaining bytes if the specified `length` was more than
    // the actual total length, i.e. if we have some remaining allocated bytes
    // there were not initialized.
    buffer.fill(0, pos, length);
  }

  return buffer;
};


function base64ByteLength(str, bytes) {
  // Handle padding
  if (str.charCodeAt(bytes - 1) === 0x3D)
    bytes--;
  if (bytes > 1 && str.charCodeAt(bytes - 1) === 0x3D)
    bytes--;

  // Base64 ratio: 3/4
  return (bytes * 3) >>> 2;
}


function byteLength(string, encoding) {
  if (typeof string !== 'string') {
    if (ArrayBuffer.isView(string) || isAnyArrayBuffer(string)) {
      return string.byteLength;
    }

    throw new TypeError('"string" must be a string, Buffer, or ArrayBuffer');
  }

  const len = string.length;
  const mustMatch = (arguments.length > 2 && arguments[2] === true);
  if (!mustMatch && len === 0)
    return 0;

  if (!encoding)
    return (mustMatch ? -1 : binding.byteLengthUtf8(string));

  encoding += '';
  switch (encoding.length) {
    case 4:
      if (encoding === 'utf8') return binding.byteLengthUtf8(string);
      if (encoding === 'ucs2') return len * 2;
      encoding = encoding.toLowerCase();
      if (encoding === 'utf8') return binding.byteLengthUtf8(string);
      if (encoding === 'ucs2') return len * 2;
      break;
    case 5:
      if (encoding === 'utf-8') return binding.byteLengthUtf8(string);
      if (encoding === 'ascii') return len;
      if (encoding === 'ucs-2') return len * 2;
      encoding = encoding.toLowerCase();
      if (encoding === 'utf-8') return binding.byteLengthUtf8(string);
      if (encoding === 'ascii') return len;
      if (encoding === 'ucs-2') return len * 2;
      break;
    case 7:
      if (encoding === 'utf16le' || encoding.toLowerCase() === 'utf16le')
        return len * 2;
      break;
    case 8:
      if (encoding === 'utf-16le' || encoding.toLowerCase() === 'utf-16le')
        return len * 2;
      break;
    case 6:
      if (encoding === 'latin1' || encoding === 'binary') return len;
      if (encoding === 'base64') return base64ByteLength(string, len);
      encoding = encoding.toLowerCase();
      if (encoding === 'latin1' || encoding === 'binary') return len;
      if (encoding === 'base64') return base64ByteLength(string, len);
      break;
    case 3:
      if (encoding === 'hex' || encoding.toLowerCase() === 'hex')
        return len >>> 1;
      break;
  }
  return (mustMatch ? -1 : binding.byteLengthUtf8(string));
}

Buffer.byteLength = byteLength;


// For backwards compatibility.
Object.defineProperty(Buffer.prototype, 'parent', {
  enumerable: true,
  get: function() {
    if (!(this instanceof Buffer))
      return undefined;
    return this.buffer;
  }
});
Object.defineProperty(Buffer.prototype, 'offset', {
  enumerable: true,
  get: function() {
    if (!(this instanceof Buffer))
      return undefined;
    return this.byteOffset;
  }
});


function stringSlice(buf, encoding, start, end) {
  if (encoding === undefined) return buf.utf8Slice(start, end);
  encoding += '';
  switch (encoding.length) {
    case 4:
      if (encoding === 'utf8') return buf.utf8Slice(start, end);
      if (encoding === 'ucs2') return buf.ucs2Slice(start, end);
      encoding = encoding.toLowerCase();
      if (encoding === 'utf8') return buf.utf8Slice(start, end);
      if (encoding === 'ucs2') return buf.ucs2Slice(start, end);
      break;
    case 5:
      if (encoding === 'utf-8') return buf.utf8Slice(start, end);
      if (encoding === 'ascii') return buf.asciiSlice(start, end);
      if (encoding === 'ucs-2') return buf.ucs2Slice(start, end);
      encoding = encoding.toLowerCase();
      if (encoding === 'utf-8') return buf.utf8Slice(start, end);
      if (encoding === 'ascii') return buf.asciiSlice(start, end);
      if (encoding === 'ucs-2') return buf.ucs2Slice(start, end);
      break;
    case 6:
      if (encoding === 'latin1' || encoding === 'binary')
        return buf.latin1Slice(start, end);
      if (encoding === 'base64') return buf.base64Slice(start, end);
      encoding = encoding.toLowerCase();
      if (encoding === 'latin1' || encoding === 'binary')
        return buf.latin1Slice(start, end);
      if (encoding === 'base64') return buf.base64Slice(start, end);
      break;
    case 3:
      if (encoding === 'hex' || encoding.toLowerCase() === 'hex')
        return buf.hexSlice(start, end);
      break;
    case 7:
      if (encoding === 'utf16le' || encoding.toLowerCase() === 'utf16le')
        return buf.ucs2Slice(start, end);
      break;
    case 8:
      if (encoding === 'utf-16le' || encoding.toLowerCase() === 'utf-16le')
        return buf.ucs2Slice(start, end);
      break;
  }
  throw new TypeError('Unknown encoding: ' + encoding);
}


Buffer.prototype.copy = function(target, targetStart, sourceStart, sourceEnd) {
  return binding.copy(this, target, targetStart, sourceStart, sourceEnd);
};

// No need to verify that "buf.length <= MAX_UINT32" since it's a read-only
// property of a typed array.
// This behaves neither like String nor Uint8Array in that we set start/end
// to their upper/lower bounds if the value passed is out of range.
Buffer.prototype.toString = function(encoding, start, end) {
  var result;
  if (arguments.length === 0) {
    result = this.utf8Slice(0, this.length);
  } else {
    const len = this.length;
    if (len === 0)
      return '';

    if (!start || start < 0)
      start = 0;
    else if (start >= len)
      return '';

    if (end === undefined || end > len)
      end = len;
    else if (end <= 0)
      return '';

    start |= 0;
    end |= 0;

    if (end <= start)
      return '';
    result = stringSlice(this, encoding, start, end);
  }
  if (result === undefined)
    throw new Error('"toString()" failed');
  return result;
};


Buffer.prototype.equals = function equals(b) {
  if (!isUint8Array(b))
    throw new TypeError('Argument must be a Buffer or Uint8Array');

  if (this === b)
    return true;

  return binding.compare(this, b) === 0;
};


// Override how buffers are presented by util.inspect().
Buffer.prototype[internalUtil.customInspectSymbol] = function inspect() {
  var str = '';
  var max = exports.INSPECT_MAX_BYTES;
  str = this.toString('hex', 0, max).replace(/(.{2})/g, '$1 ').trim();
  if (this.length > max)
    str += ' ... ';
  return `<${this.constructor.name} ${str}>`;
};
Buffer.prototype.inspect = Buffer.prototype[internalUtil.customInspectSymbol];

Buffer.prototype.compare = function compare(target,
                                            start,
                                            end,
                                            thisStart,
                                            thisEnd) {
  if (!isUint8Array(target))
    throw new TypeError('Argument must be a Buffer or Uint8Array');
  if (arguments.length === 1)
    return compare_(this, target);

  if (start === undefined)
    start = 0;
  else if (start < 0)
    throw new RangeError('out of range index');
  else
    start >>>= 0;

  if (end === undefined)
    end = target.length;
  else if (end > target.length)
    throw new RangeError('out of range index');
  else
    end >>>= 0;

  if (thisStart === undefined)
    thisStart = 0;
  else if (thisStart < 0)
    throw new RangeError('out of range index');
  else
    thisStart >>>= 0;

  if (thisEnd === undefined)
    thisEnd = this.length;
  else if (thisEnd > this.length)
    throw new RangeError('out of range index');
  else
    thisEnd >>>= 0;

  if (thisStart >= thisEnd)
    return (start >= end ? 0 : -1);
  else if (start >= end)
    return 1;

  return compareOffset(this, target, start, thisStart, end, thisEnd);
};


// Finds either the first index of `val` in `buffer` at offset >= `byteOffset`,
// OR the last index of `val` in `buffer` at offset <= `byteOffset`.
//
// Arguments:
// - buffer - a Buffer to search
// - val - a string, Buffer, or number
// - byteOffset - an index into `buffer`; will be clamped to an int32
// - encoding - an optional encoding, relevant is val is a string
// - dir - true for indexOf, false for lastIndexOf
function bidirectionalIndexOf(buffer, val, byteOffset, encoding, dir) {
  if (typeof byteOffset === 'string') {
    encoding = byteOffset;
    byteOffset = undefined;
  } else if (byteOffset > 0x7fffffff) {
    byteOffset = 0x7fffffff;
  } else if (byteOffset < -0x80000000) {
    byteOffset = -0x80000000;
  }
  // Coerce to Number. Values like null and [] become 0.
  byteOffset = +byteOffset;
  // If the offset is undefined, "foo", {}, coerces to NaN, search whole buffer.
  // `x !== x`-style conditionals are a faster form of `isNaN(x)`
  if (byteOffset !== byteOffset) {
    byteOffset = dir ? 0 : (buffer.length - 1);
  }
  dir = !!dir;  // Cast to bool.

  if (typeof val === 'string') {
    if (encoding === undefined) {
      return binding.indexOfString(buffer, val, byteOffset, encoding, dir);
    }
    return slowIndexOf(buffer, val, byteOffset, encoding, dir);
  } else if (isUint8Array(val)) {
    return binding.indexOfBuffer(buffer, val, byteOffset, encoding, dir);
  } else if (typeof val === 'number') {
    return binding.indexOfNumber(buffer, val, byteOffset, dir);
  }

  throw new TypeError('"val" argument must be string, number, Buffer ' +
                      'or Uint8Array');
}


function slowIndexOf(buffer, val, byteOffset, encoding, dir) {
  var loweredCase = false;
  for (;;) {
    switch (encoding) {
      case 'utf8':
      case 'utf-8':
      case 'ucs2':
      case 'ucs-2':
      case 'utf16le':
      case 'utf-16le':
      case 'latin1':
      case 'binary':
        return binding.indexOfString(buffer, val, byteOffset, encoding, dir);

      case 'base64':
      case 'ascii':
      case 'hex':
        return binding.indexOfBuffer(
            buffer, Buffer.from(val, encoding), byteOffset, encoding, dir);

      default:
        if (loweredCase) {
          throw new TypeError('Unknown encoding: ' + encoding);
        }

        encoding = ('' + encoding).toLowerCase();
        loweredCase = true;
    }
  }
}


Buffer.prototype.indexOf = function indexOf(val, byteOffset, encoding) {
  return bidirectionalIndexOf(this, val, byteOffset, encoding, true);
};


Buffer.prototype.lastIndexOf = function lastIndexOf(val, byteOffset, encoding) {
  return bidirectionalIndexOf(this, val, byteOffset, encoding, false);
};


Buffer.prototype.includes = function includes(val, byteOffset, encoding) {
  return this.indexOf(val, byteOffset, encoding) !== -1;
};


// Usage:
//    buffer.fill(number[, offset[, end]])
//    buffer.fill(buffer[, offset[, end]])
//    buffer.fill(string[, offset[, end]][, encoding])
Buffer.prototype.fill = function fill(val, start, end, encoding) {
  // Handle string cases:
  if (typeof val === 'string') {
    if (typeof start === 'string') {
      encoding = start;
      start = 0;
      end = this.length;
    } else if (typeof end === 'string') {
      encoding = end;
      end = this.length;
    }

    if (encoding !== undefined && typeof encoding !== 'string') {
      throw new TypeError('encoding must be a string');
    }
    var normalizedEncoding = internalUtil.normalizeEncoding(encoding);
    if (normalizedEncoding === undefined) {
      throw new TypeError('Unknown encoding: ' + encoding);
    }

    if (val.length === 0) {
      // Previously, if val === '', the Buffer would not fill,
      // which is rather surprising.
      val = 0;
    } else if (val.length === 1) {
      var code = val.charCodeAt(0);
      if ((normalizedEncoding === 'utf8' && code < 128) ||
          normalizedEncoding === 'latin1') {
        // Fast path: If `val` fits into a single byte, use that numeric value.
        val = code;
      }
    }
  } else if (typeof val === 'number') {
    val = val & 255;
  }

  // Invalid ranges are not set to a default, so can range check early.
  if (start < 0 || end > this.length)
    throw new RangeError('Out of range index');

  if (end <= start)
    return this;

  start = start >>> 0;
  end = end === undefined ? this.length : end >>> 0;

  binding.fill(this, val, start, end, encoding);

  return this;
};


Buffer.prototype.write = function(string, offset, length, encoding) {
  // Buffer#write(string);
  if (offset === undefined) {
    return this.utf8Write(string, 0, this.length);

  // Buffer#write(string, encoding)
  } else if (length === undefined && typeof offset === 'string') {
    encoding = offset;
    length = this.length;
    offset = 0;

  // Buffer#write(string, offset[, length][, encoding])
  } else if (isFinite(offset)) {
    offset = offset >>> 0;
    if (isFinite(length)) {
      length = length >>> 0;
    } else {
      encoding = length;
      length = undefined;
    }

    var remaining = this.length - offset;
    if (length === undefined || length > remaining)
      length = remaining;

    if (string.length > 0 && (length < 0 || offset < 0))
      throw new RangeError('Attempt to write outside buffer bounds');
  } else {
    // if someone is still calling the obsolete form of write(), tell them.
    // we don't want eg buf.write("foo", "utf8", 10) to silently turn into
    // buf.write("foo", "utf8"), so we can't ignore extra args
    throw new Error('Buffer.write(string, encoding, offset[, length]) ' +
                    'is no longer supported');
  }

  if (!encoding) return this.utf8Write(string, offset, length);

  encoding += '';
  switch (encoding.length) {
    case 4:
      if (encoding === 'utf8') return this.utf8Write(string, offset, length);
      if (encoding === 'ucs2') return this.ucs2Write(string, offset, length);
      encoding = encoding.toLowerCase();
      if (encoding === 'utf8') return this.utf8Write(string, offset, length);
      if (encoding === 'ucs2') return this.ucs2Write(string, offset, length);
      break;
    case 5:
      if (encoding === 'utf-8') return this.utf8Write(string, offset, length);
      if (encoding === 'ascii') return this.asciiWrite(string, offset, length);
      if (encoding === 'ucs-2') return this.ucs2Write(string, offset, length);
      encoding = encoding.toLowerCase();
      if (encoding === 'utf-8') return this.utf8Write(string, offset, length);
      if (encoding === 'ascii') return this.asciiWrite(string, offset, length);
      if (encoding === 'ucs-2') return this.ucs2Write(string, offset, length);
      break;
    case 7:
      if (encoding === 'utf16le' || encoding.toLowerCase() === 'utf16le')
        return this.ucs2Write(string, offset, length);
      break;
    case 8:
      if (encoding === 'utf-16le' || encoding.toLowerCase() === 'utf-16le')
        return this.ucs2Write(string, offset, length);
      break;
    case 6:
      if (encoding === 'latin1' || encoding === 'binary')
        return this.latin1Write(string, offset, length);
      if (encoding === 'base64')
        return this.base64Write(string, offset, length);
      encoding = encoding.toLowerCase();
      if (encoding === 'latin1' || encoding === 'binary')
        return this.latin1Write(string, offset, length);
      if (encoding === 'base64')
        return this.base64Write(string, offset, length);
      break;
    case 3:
      if (encoding === 'hex' || encoding.toLowerCase() === 'hex')
        return this.hexWrite(string, offset, length);
      break;
  }
  throw new TypeError('Unknown encoding: ' + encoding);
};


Buffer.prototype.toJSON = function() {
  if (this.length > 0) {
    const data = new Array(this.length);
    for (var i = 0; i < this.length; ++i)
      data[i] = this[i];
    return { type: 'Buffer', data };
  } else {
    return { type: 'Buffer', data: [] };
  }
};


function adjustOffset(offset, length) {
  // Use Math.trunc() to convert offset to an integer value that can be larger
  // than an Int32. Hence, don't use offset | 0 or similar techniques.
  offset = Math.trunc(offset);
  // `x !== x`-style conditionals are a faster form of `isNaN(x)`
  if (offset === 0 || offset !== offset) {
    return 0;
  } else if (offset < 0) {
    offset += length;
    return offset > 0 ? offset : 0;
  } else {
    return offset < length ? offset : length;
  }
}


Buffer.prototype.slice = function slice(start, end) {
  const srcLength = this.length;
  start = adjustOffset(start, srcLength);
  end = end !== undefined ? adjustOffset(end, srcLength) : srcLength;
  const newLength = end > start ? end - start : 0;
  return new FastBuffer(this.buffer, this.byteOffset + start, newLength);
};


function checkOffset(offset, ext, length) {
  if (offset + ext > length)
    throw new RangeError('Index out of range');
}


Buffer.prototype.readUIntLE = function(offset, byteLength, noAssert) {
  offset = offset >>> 0;
  byteLength = byteLength >>> 0;
  if (!noAssert)
    checkOffset(offset, byteLength, this.length);

  var val = this[offset];
  var mul = 1;
  var i = 0;
  while (++i < byteLength && (mul *= 0x100))
    val += this[offset + i] * mul;

  return val;
};


Buffer.prototype.readUIntBE = function(offset, byteLength, noAssert) {
  offset = offset >>> 0;
  byteLength = byteLength >>> 0;
  if (!noAssert)
    checkOffset(offset, byteLength, this.length);

  var val = this[offset + --byteLength];
  var mul = 1;
  while (byteLength > 0 && (mul *= 0x100))
    val += this[offset + --byteLength] * mul;

  return val;
};


Buffer.prototype.readUInt8 = function(offset, noAssert) {
  offset = offset >>> 0;
  if (!noAssert)
    checkOffset(offset, 1, this.length);
  return this[offset];
};


Buffer.prototype.readUInt16LE = function(offset, noAssert) {
  offset = offset >>> 0;
  if (!noAssert)
    checkOffset(offset, 2, this.length);
  return this[offset] | (this[offset + 1] << 8);
};


Buffer.prototype.readUInt16BE = function(offset, noAssert) {
  offset = offset >>> 0;
  if (!noAssert)
    checkOffset(offset, 2, this.length);
  return (this[offset] << 8) | this[offset + 1];
};


Buffer.prototype.readUInt32LE = function(offset, noAssert) {
  offset = offset >>> 0;
  if (!noAssert)
    checkOffset(offset, 4, this.length);

  return ((this[offset]) |
      (this[offset + 1] << 8) |
      (this[offset + 2] << 16)) +
      (this[offset + 3] * 0x1000000);
};


Buffer.prototype.readUInt32BE = function(offset, noAssert) {
  offset = offset >>> 0;
  if (!noAssert)
    checkOffset(offset, 4, this.length);

  return (this[offset] * 0x1000000) +
      ((this[offset + 1] << 16) |
      (this[offset + 2] << 8) |
      this[offset + 3]);
};


Buffer.prototype.readIntLE = function(offset, byteLength, noAssert) {
  offset = offset >>> 0;
  byteLength = byteLength >>> 0;
  if (!noAssert)
    checkOffset(offset, byteLength, this.length);

  var val = this[offset];
  var mul = 1;
  var i = 0;
  while (++i < byteLength && (mul *= 0x100))
    val += this[offset + i] * mul;
  mul *= 0x80;

  if (val >= mul)
    val -= Math.pow(2, 8 * byteLength);

  return val;
};


Buffer.prototype.readIntBE = function(offset, byteLength, noAssert) {
  offset = offset >>> 0;
  byteLength = byteLength >>> 0;
  if (!noAssert)
    checkOffset(offset, byteLength, this.length);

  var i = byteLength;
  var mul = 1;
  var val = this[offset + --i];
  while (i > 0 && (mul *= 0x100))
    val += this[offset + --i] * mul;
  mul *= 0x80;

  if (val >= mul)
    val -= Math.pow(2, 8 * byteLength);

  return val;
};


Buffer.prototype.readInt8 = function(offset, noAssert) {
  offset = offset >>> 0;
  if (!noAssert)
    checkOffset(offset, 1, this.length);
  var val = this[offset];
  return !(val & 0x80) ? val : (0xff - val + 1) * -1;
};


Buffer.prototype.readInt16LE = function(offset, noAssert) {
  offset = offset >>> 0;
  if (!noAssert)
    checkOffset(offset, 2, this.length);
  var val = this[offset] | (this[offset + 1] << 8);
  return (val & 0x8000) ? val | 0xFFFF0000 : val;
};


Buffer.prototype.readInt16BE = function(offset, noAssert) {
  offset = offset >>> 0;
  if (!noAssert)
    checkOffset(offset, 2, this.length);
  var val = this[offset + 1] | (this[offset] << 8);
  return (val & 0x8000) ? val | 0xFFFF0000 : val;
};


Buffer.prototype.readInt32LE = function(offset, noAssert) {
  offset = offset >>> 0;
  if (!noAssert)
    checkOffset(offset, 4, this.length);

  return (this[offset]) |
      (this[offset + 1] << 8) |
      (this[offset + 2] << 16) |
      (this[offset + 3] << 24);
};


Buffer.prototype.readInt32BE = function(offset, noAssert) {
  offset = offset >>> 0;
  if (!noAssert)
    checkOffset(offset, 4, this.length);

  return (this[offset] << 24) |
      (this[offset + 1] << 16) |
      (this[offset + 2] << 8) |
      (this[offset + 3]);
};


Buffer.prototype.readFloatLE = function readFloatLE(offset, noAssert) {
  offset = offset >>> 0;
  if (!noAssert)
    checkOffset(offset, 4, this.length);
  return binding.readFloatLE(this, offset);
};


Buffer.prototype.readFloatBE = function readFloatBE(offset, noAssert) {
  offset = offset >>> 0;
  if (!noAssert)
    checkOffset(offset, 4, this.length);
  return binding.readFloatBE(this, offset);
};


Buffer.prototype.readDoubleLE = function readDoubleLE(offset, noAssert) {
  offset = offset >>> 0;
  if (!noAssert)
    checkOffset(offset, 8, this.length);
  return binding.readDoubleLE(this, offset);
};


Buffer.prototype.readDoubleBE = function readDoubleBE(offset, noAssert) {
  offset = offset >>> 0;
  if (!noAssert)
    checkOffset(offset, 8, this.length);
  return binding.readDoubleBE(this, offset);
};


function checkInt(buffer, value, offset, ext, max, min) {
  if (value > max || value < min)
    throw new TypeError('"value" argument is out of bounds');
  if (offset + ext > buffer.length)
    throw new RangeError('Index out of range');
}


Buffer.prototype.writeUIntLE = function(value, offset, byteLength, noAssert) {
  value = +value;
  offset = offset >>> 0;
  byteLength = byteLength >>> 0;
  if (!noAssert) {
    const maxBytes = Math.pow(2, 8 * byteLength) - 1;
    checkInt(this, value, offset, byteLength, maxBytes, 0);
  }

  var mul = 1;
  var i = 0;
  this[offset] = value;
  while (++i < byteLength && (mul *= 0x100))
    this[offset + i] = (value / mul) >>> 0;

  return offset + byteLength;
};


Buffer.prototype.writeUIntBE = function(value, offset, byteLength, noAssert) {
  value = +value;
  offset = offset >>> 0;
  byteLength = byteLength >>> 0;
  if (!noAssert) {
    const maxBytes = Math.pow(2, 8 * byteLength) - 1;
    checkInt(this, value, offset, byteLength, maxBytes, 0);
  }

  var i = byteLength - 1;
  var mul = 1;
  this[offset + i] = value;
  while (--i >= 0 && (mul *= 0x100))
    this[offset + i] = (value / mul) >>> 0;

  return offset + byteLength;
};


Buffer.prototype.writeUInt8 = function(value, offset, noAssert) {
  value = +value;
  offset = offset >>> 0;
  if (!noAssert)
    checkInt(this, value, offset, 1, 0xff, 0);
  this[offset] = value;
  return offset + 1;
};


Buffer.prototype.writeUInt16LE = function(value, offset, noAssert) {
  value = +value;
  offset = offset >>> 0;
  if (!noAssert)
    checkInt(this, value, offset, 2, 0xffff, 0);
  this[offset] = value;
  this[offset + 1] = (value >>> 8);
  return offset + 2;
};


Buffer.prototype.writeUInt16BE = function(value, offset, noAssert) {
  value = +value;
  offset = offset >>> 0;
  if (!noAssert)
    checkInt(this, value, offset, 2, 0xffff, 0);
  this[offset] = (value >>> 8);
  this[offset + 1] = value;
  return offset + 2;
};


Buffer.prototype.writeUInt32LE = function(value, offset, noAssert) {
  value = +value;
  offset = offset >>> 0;
  if (!noAssert)
    checkInt(this, value, offset, 4, 0xffffffff, 0);
  this[offset + 3] = (value >>> 24);
  this[offset + 2] = (value >>> 16);
  this[offset + 1] = (value >>> 8);
  this[offset] = value;
  return offset + 4;
};


Buffer.prototype.writeUInt32BE = function(value, offset, noAssert) {
  value = +value;
  offset = offset >>> 0;
  if (!noAssert)
    checkInt(this, value, offset, 4, 0xffffffff, 0);
  this[offset] = (value >>> 24);
  this[offset + 1] = (value >>> 16);
  this[offset + 2] = (value >>> 8);
  this[offset + 3] = value;
  return offset + 4;
};


Buffer.prototype.writeIntLE = function(value, offset, byteLength, noAssert) {
  value = +value;
  offset = offset >>> 0;
  if (!noAssert) {
    checkInt(this,
             value,
             offset,
             byteLength,
             Math.pow(2, 8 * byteLength - 1) - 1,
             -Math.pow(2, 8 * byteLength - 1));
  }

  var i = 0;
  var mul = 1;
  var sub = 0;
  this[offset] = value;
  while (++i < byteLength && (mul *= 0x100)) {
    if (value < 0 && sub === 0 && this[offset + i - 1] !== 0)
      sub = 1;
    this[offset + i] = ((value / mul) >> 0) - sub;
  }

  return offset + byteLength;
};


Buffer.prototype.writeIntBE = function(value, offset, byteLength, noAssert) {
  value = +value;
  offset = offset >>> 0;
  if (!noAssert) {
    checkInt(this,
             value,
             offset,
             byteLength,
             Math.pow(2, 8 * byteLength - 1) - 1,
             -Math.pow(2, 8 * byteLength - 1));
  }

  var i = byteLength - 1;
  var mul = 1;
  var sub = 0;
  this[offset + i] = value;
  while (--i >= 0 && (mul *= 0x100)) {
    if (value < 0 && sub === 0 && this[offset + i + 1] !== 0)
      sub = 1;
    this[offset + i] = ((value / mul) >> 0) - sub;
  }

  return offset + byteLength;
};


Buffer.prototype.writeInt8 = function(value, offset, noAssert) {
  value = +value;
  offset = offset >>> 0;
  if (!noAssert)
    checkInt(this, value, offset, 1, 0x7f, -0x80);
  this[offset] = value;
  return offset + 1;
};


Buffer.prototype.writeInt16LE = function(value, offset, noAssert) {
  value = +value;
  offset = offset >>> 0;
  if (!noAssert)
    checkInt(this, value, offset, 2, 0x7fff, -0x8000);
  this[offset] = value;
  this[offset + 1] = (value >>> 8);
  return offset + 2;
};


Buffer.prototype.writeInt16BE = function(value, offset, noAssert) {
  value = +value;
  offset = offset >>> 0;
  if (!noAssert)
    checkInt(this, value, offset, 2, 0x7fff, -0x8000);
  this[offset] = (value >>> 8);
  this[offset + 1] = value;
  return offset + 2;
};


Buffer.prototype.writeInt32LE = function(value, offset, noAssert) {
  value = +value;
  offset = offset >>> 0;
  if (!noAssert)
    checkInt(this, value, offset, 4, 0x7fffffff, -0x80000000);
  this[offset] = value;
  this[offset + 1] = (value >>> 8);
  this[offset + 2] = (value >>> 16);
  this[offset + 3] = (value >>> 24);
  return offset + 4;
};


Buffer.prototype.writeInt32BE = function(value, offset, noAssert) {
  value = +value;
  offset = offset >>> 0;
  if (!noAssert)
    checkInt(this, value, offset, 4, 0x7fffffff, -0x80000000);
  this[offset] = (value >>> 24);
  this[offset + 1] = (value >>> 16);
  this[offset + 2] = (value >>> 8);
  this[offset + 3] = value;
  return offset + 4;
};


Buffer.prototype.writeFloatLE = function writeFloatLE(val, offset, noAssert) {
  val = +val;
  offset = offset >>> 0;
  if (!noAssert)
    binding.writeFloatLE(this, val, offset);
  else
    binding.writeFloatLE(this, val, offset, true);
  return offset + 4;
};


Buffer.prototype.writeFloatBE = function writeFloatBE(val, offset, noAssert) {
  val = +val;
  offset = offset >>> 0;
  if (!noAssert)
    binding.writeFloatBE(this, val, offset);
  else
    binding.writeFloatBE(this, val, offset, true);
  return offset + 4;
};


Buffer.prototype.writeDoubleLE = function writeDoubleLE(val, offset, noAssert) {
  val = +val;
  offset = offset >>> 0;
  if (!noAssert)
    binding.writeDoubleLE(this, val, offset);
  else
    binding.writeDoubleLE(this, val, offset, true);
  return offset + 8;
};


Buffer.prototype.writeDoubleBE = function writeDoubleBE(val, offset, noAssert) {
  val = +val;
  offset = offset >>> 0;
  if (!noAssert)
    binding.writeDoubleBE(this, val, offset);
  else
    binding.writeDoubleBE(this, val, offset, true);
  return offset + 8;
};

const swap16n = binding.swap16;
const swap32n = binding.swap32;
const swap64n = binding.swap64;

function swap(b, n, m) {
  const i = b[n];
  b[n] = b[m];
  b[m] = i;
}


Buffer.prototype.swap16 = function swap16() {
  // For Buffer.length < 128, it's generally faster to
  // do the swap in javascript. For larger buffers,
  // dropping down to the native code is faster.
  const len = this.length;
  if (len % 2 !== 0)
    throw new RangeError('Buffer size must be a multiple of 16-bits');
  if (len < 128) {
    for (var i = 0; i < len; i += 2)
      swap(this, i, i + 1);
    return this;
  }
  return swap16n(this);
};


Buffer.prototype.swap32 = function swap32() {
  // For Buffer.length < 192, it's generally faster to
  // do the swap in javascript. For larger buffers,
  // dropping down to the native code is faster.
  const len = this.length;
  if (len % 4 !== 0)
    throw new RangeError('Buffer size must be a multiple of 32-bits');
  if (len < 192) {
    for (var i = 0; i < len; i += 4) {
      swap(this, i, i + 3);
      swap(this, i + 1, i + 2);
    }
    return this;
  }
  return swap32n(this);
};


Buffer.prototype.swap64 = function swap64() {
  // For Buffer.length < 192, it's generally faster to
  // do the swap in javascript. For larger buffers,
  // dropping down to the native code is faster.
  const len = this.length;
  if (len % 8 !== 0)
    throw new RangeError('Buffer size must be a multiple of 64-bits');
  if (len < 192) {
    for (var i = 0; i < len; i += 8) {
      swap(this, i, i + 7);
      swap(this, i + 1, i + 6);
      swap(this, i + 2, i + 5);
      swap(this, i + 3, i + 4);
    }
    return this;
  }
  return swap64n(this);
};

Buffer.prototype.toLocaleString = Buffer.prototype.toString;

// Put this at the end because internal/buffer has a circular
// dependency on Buffer.
const internalBuffer = require('internal/buffer');
exports.transcode = internalBuffer.transcode;
internalBuffer.FastBuffer = FastBuffer;
