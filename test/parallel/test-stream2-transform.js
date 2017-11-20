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
const common = require('../common');
const assert = require('assert');
const PassThrough = require('_stream_passthrough');
const Transform = require('_stream_transform');

{
  // Verify writable side consumption
  const tx = new Transform({
    highWaterMark: 10
  });

  let transformed = 0;
  tx._transform = function(chunk, encoding, cb) {
    transformed += chunk.length;
    tx.push(chunk);
    cb();
  };

  for (let i = 1; i <= 10; i++) {
    tx.write(Buffer.allocUnsafe(i));
  }
  tx.end();

  assert.strictEqual(tx._readableState.length, 10);
  assert.strictEqual(transformed, 10);
  assert.strictEqual(tx._transformState.writechunk.length, 5);
  assert.deepStrictEqual(tx._writableState.getBuffer().map(function(c) {
    return c.chunk.length;
  }), [6, 7, 8, 9, 10]);
}

{
  // Verify passthrough behavior
  const pt = new PassThrough();

  pt.write(Buffer.from('foog'));
  pt.write(Buffer.from('bark'));
  pt.write(Buffer.from('bazy'));
  pt.write(Buffer.from('kuel'));
  pt.end();

  assert.strictEqual(pt.read(5).toString(), 'foogb');
  assert.strictEqual(pt.read(5).toString(), 'arkba');
  assert.strictEqual(pt.read(5).toString(), 'zykue');
  assert.strictEqual(pt.read(5).toString(), 'l');
}

{
  // Verify object passthrough behavior
  const pt = new PassThrough({ objectMode: true });

  pt.write(1);
  pt.write(true);
  pt.write(false);
  pt.write(0);
  pt.write('foo');
  pt.write('');
  pt.write({ a: 'b' });
  pt.end();

  assert.strictEqual(pt.read(), 1);
  assert.strictEqual(pt.read(), true);
  assert.strictEqual(pt.read(), false);
  assert.strictEqual(pt.read(), 0);
  assert.strictEqual(pt.read(), 'foo');
  assert.strictEqual(pt.read(), '');
  assert.deepStrictEqual(pt.read(), { a: 'b' });
}

{
  // Verify passthrough constructor behavior
  const pt = PassThrough();

  assert(pt instanceof PassThrough);
}

{
  // Perform a simple transform
  const pt = new Transform();
  pt._transform = function(c, e, cb) {
    const ret = Buffer.alloc(c.length, 'x');
    pt.push(ret);
    cb();
  };

  pt.write(Buffer.from('foog'));
  pt.write(Buffer.from('bark'));
  pt.write(Buffer.from('bazy'));
  pt.write(Buffer.from('kuel'));
  pt.end();

  assert.strictEqual(pt.read(5).toString(), 'xxxxx');
  assert.strictEqual(pt.read(5).toString(), 'xxxxx');
  assert.strictEqual(pt.read(5).toString(), 'xxxxx');
  assert.strictEqual(pt.read(5).toString(), 'x');
}

{
  // Verify simple object transform
  const pt = new Transform({ objectMode: true });
  pt._transform = function(c, e, cb) {
    pt.push(JSON.stringify(c));
    cb();
  };

  pt.write(1);
  pt.write(true);
  pt.write(false);
  pt.write(0);
  pt.write('foo');
  pt.write('');
  pt.write({ a: 'b' });
  pt.end();

  assert.strictEqual(pt.read(), '1');
  assert.strictEqual(pt.read(), 'true');
  assert.strictEqual(pt.read(), 'false');
  assert.strictEqual(pt.read(), '0');
  assert.strictEqual(pt.read(), '"foo"');
  assert.strictEqual(pt.read(), '""');
  assert.strictEqual(pt.read(), '{"a":"b"}');
}

{
  // Verify async passthrough
  const pt = new Transform();
  pt._transform = function(chunk, encoding, cb) {
    setTimeout(function() {
      pt.push(chunk);
      cb();
    }, 10);
  };

  pt.write(Buffer.from('foog'));
  pt.write(Buffer.from('bark'));
  pt.write(Buffer.from('bazy'));
  pt.write(Buffer.from('kuel'));
  pt.end();

  pt.on('finish', common.mustCall(function() {
    assert.strictEqual(pt.read(5).toString(), 'foogb');
    assert.strictEqual(pt.read(5).toString(), 'arkba');
    assert.strictEqual(pt.read(5).toString(), 'zykue');
    assert.strictEqual(pt.read(5).toString(), 'l');
  }));
}

{
  // Verify assymetric transform (expand)
  const pt = new Transform();

  // emit each chunk 2 times.
  pt._transform = function(chunk, encoding, cb) {
    setTimeout(function() {
      pt.push(chunk);
      setTimeout(function() {
        pt.push(chunk);
        cb();
      }, 10);
    }, 10);
  };

  pt.write(Buffer.from('foog'));
  pt.write(Buffer.from('bark'));
  pt.write(Buffer.from('bazy'));
  pt.write(Buffer.from('kuel'));
  pt.end();

  pt.on('finish', common.mustCall(function() {
    assert.strictEqual(pt.read(5).toString(), 'foogf');
    assert.strictEqual(pt.read(5).toString(), 'oogba');
    assert.strictEqual(pt.read(5).toString(), 'rkbar');
    assert.strictEqual(pt.read(5).toString(), 'kbazy');
    assert.strictEqual(pt.read(5).toString(), 'bazyk');
    assert.strictEqual(pt.read(5).toString(), 'uelku');
    assert.strictEqual(pt.read(5).toString(), 'el');
  }));
}

{
  // Verify assymetric trasform (compress)
  const pt = new Transform();

  // each output is the first char of 3 consecutive chunks,
  // or whatever's left.
  pt.state = '';

  pt._transform = function(chunk, encoding, cb) {
    if (!chunk)
      chunk = '';
    const s = chunk.toString();
    setTimeout(() => {
      this.state += s.charAt(0);
      if (this.state.length === 3) {
        pt.push(Buffer.from(this.state));
        this.state = '';
      }
      cb();
    }, 10);
  };

  pt._flush = function(cb) {
    // just output whatever we have.
    pt.push(Buffer.from(this.state));
    this.state = '';
    cb();
  };

  pt.write(Buffer.from('aaaa'));
  pt.write(Buffer.from('bbbb'));
  pt.write(Buffer.from('cccc'));
  pt.write(Buffer.from('dddd'));
  pt.write(Buffer.from('eeee'));
  pt.write(Buffer.from('aaaa'));
  pt.write(Buffer.from('bbbb'));
  pt.write(Buffer.from('cccc'));
  pt.write(Buffer.from('dddd'));
  pt.write(Buffer.from('eeee'));
  pt.write(Buffer.from('aaaa'));
  pt.write(Buffer.from('bbbb'));
  pt.write(Buffer.from('cccc'));
  pt.write(Buffer.from('dddd'));
  pt.end();

  // 'abcdeabcdeabcd'
  pt.on('finish', common.mustCall(function() {
    assert.strictEqual(pt.read(5).toString(), 'abcde');
    assert.strictEqual(pt.read(5).toString(), 'abcde');
    assert.strictEqual(pt.read(5).toString(), 'abcd');
  }));
}

// this tests for a stall when data is written to a full stream
// that has empty transforms.
{
  // Verify compex transform behavior
  let count = 0;
  let saved = null;
  const pt = new Transform({ highWaterMark: 3 });
  pt._transform = function(c, e, cb) {
    if (count++ === 1)
      saved = c;
    else {
      if (saved) {
        pt.push(saved);
        saved = null;
      }
      pt.push(c);
    }

    cb();
  };

  pt.once('readable', function() {
    process.nextTick(function() {
      pt.write(Buffer.from('d'));
      pt.write(Buffer.from('ef'), common.mustCall(function() {
        pt.end();
      }));
      assert.strictEqual(pt.read().toString(), 'abcdef');
      assert.strictEqual(pt.read(), null);
    });
  });

  pt.write(Buffer.from('abc'));
}


{
  // Verify passthrough event emission
  const pt = new PassThrough();
  let emits = 0;
  pt.on('readable', function() {
    emits++;
  });

  pt.write(Buffer.from('foog'));
  pt.write(Buffer.from('bark'));

  assert.strictEqual(emits, 1);
  assert.strictEqual(pt.read(5).toString(), 'foogb');
  assert.strictEqual(String(pt.read(5)), 'null');

  pt.write(Buffer.from('bazy'));
  pt.write(Buffer.from('kuel'));

  assert.strictEqual(emits, 2);
  assert.strictEqual(pt.read(5).toString(), 'arkba');
  assert.strictEqual(pt.read(5).toString(), 'zykue');
  assert.strictEqual(pt.read(5), null);

  pt.end();

  assert.strictEqual(emits, 3);
  assert.strictEqual(pt.read(5).toString(), 'l');
  assert.strictEqual(pt.read(5), null);

  assert.strictEqual(emits, 3);
}

{
  // Verify passthrough event emission reordering
  const pt = new PassThrough();
  let emits = 0;
  pt.on('readable', function() {
    emits++;
  });

  pt.write(Buffer.from('foog'));
  pt.write(Buffer.from('bark'));

  assert.strictEqual(emits, 1);
  assert.strictEqual(pt.read(5).toString(), 'foogb');
  assert.strictEqual(pt.read(5), null);

  pt.once('readable', common.mustCall(function() {
    assert.strictEqual(pt.read(5).toString(), 'arkba');
    assert.strictEqual(pt.read(5), null);

    pt.once('readable', common.mustCall(function() {
      assert.strictEqual(pt.read(5).toString(), 'zykue');
      assert.strictEqual(pt.read(5), null);
      pt.once('readable', common.mustCall(function() {
        assert.strictEqual(pt.read(5).toString(), 'l');
        assert.strictEqual(pt.read(5), null);
        assert.strictEqual(emits, 4);
      }));
      pt.end();
    }));
    pt.write(Buffer.from('kuel'));
  }));

  pt.write(Buffer.from('bazy'));
}

{
  // Verify passthrough facade
  const pt = new PassThrough();
  const datas = [];
  pt.on('data', function(chunk) {
    datas.push(chunk.toString());
  });

  pt.on('end', common.mustCall(function() {
    assert.deepStrictEqual(datas, ['foog', 'bark', 'bazy', 'kuel']);
  }));

  pt.write(Buffer.from('foog'));
  setTimeout(function() {
    pt.write(Buffer.from('bark'));
    setTimeout(function() {
      pt.write(Buffer.from('bazy'));
      setTimeout(function() {
        pt.write(Buffer.from('kuel'));
        setTimeout(function() {
          pt.end();
        }, 10);
      }, 10);
    }, 10);
  }, 10);
}

{
  // Verify object transform (JSON parse)
  const jp = new Transform({ objectMode: true });
  jp._transform = function(data, encoding, cb) {
    try {
      jp.push(JSON.parse(data));
      cb();
    } catch (er) {
      cb(er);
    }
  };

  // anything except null/undefined is fine.
  // those are "magic" in the stream API, because they signal EOF.
  const objects = [
    { foo: 'bar' },
    100,
    'string',
    { nested: { things: [ { foo: 'bar' }, 100, 'string' ] } }
  ];

  let ended = false;
  jp.on('end', function() {
    ended = true;
  });

  objects.forEach(function(obj) {
    jp.write(JSON.stringify(obj));
    const res = jp.read();
    assert.deepStrictEqual(res, obj);
  });

  jp.end();
  // read one more time to get the 'end' event
  jp.read();

  process.nextTick(common.mustCall(function() {
    assert.strictEqual(ended, true);
  }));
}

{
  // Verify object transform (JSON stringify)
  const js = new Transform({ objectMode: true });
  js._transform = function(data, encoding, cb) {
    try {
      js.push(JSON.stringify(data));
      cb();
    } catch (er) {
      cb(er);
    }
  };

  // anything except null/undefined is fine.
  // those are "magic" in the stream API, because they signal EOF.
  const objects = [
    { foo: 'bar' },
    100,
    'string',
    { nested: { things: [ { foo: 'bar' }, 100, 'string' ] } }
  ];

  let ended = false;
  js.on('end', function() {
    ended = true;
  });

  objects.forEach(function(obj) {
    js.write(obj);
    const res = js.read();
    assert.strictEqual(res, JSON.stringify(obj));
  });

  js.end();
  // read one more time to get the 'end' event
  js.read();

  process.nextTick(common.mustCall(function() {
    assert.strictEqual(ended, true);
  }));
}
