'use strict';

const common = require('../common');
const { Duplex } = require('stream');
const assert = require('assert');
const { inherits } = require('util');

{
  const duplex = new Duplex({
    write(chunk, enc, cb) { cb(); },
    read() {}
  });

  duplex.resume();

  duplex.on('end', common.mustCall());
  duplex.on('finish', common.mustCall());

  duplex.destroy();
  assert.strictEqual(duplex.destroyed, true);
}

{
  const duplex = new Duplex({
    write(chunk, enc, cb) { cb(); },
    read() {}
  });
  duplex.resume();

  const expected = new Error('kaboom');

  duplex.on('end', common.mustCall());
  duplex.on('finish', common.mustCall());
  duplex.on('error', common.mustCall((err) => {
    assert.strictEqual(err, expected);
  }));

  duplex.destroy(expected);
  assert.strictEqual(duplex.destroyed, true);
}

{
  const duplex = new Duplex({
    write(chunk, enc, cb) { cb(); },
    read() {}
  });

  duplex._destroy = common.mustCall(function(err, cb) {
    assert.strictEqual(err, expected);
    cb(err);
  });

  const expected = new Error('kaboom');

  duplex.on('finish', common.mustNotCall('no finish event'));
  duplex.on('error', common.mustCall((err) => {
    assert.strictEqual(err, expected);
  }));

  duplex.destroy(expected);
  assert.strictEqual(duplex.destroyed, true);
}

{
  const expected = new Error('kaboom');
  const duplex = new Duplex({
    write(chunk, enc, cb) { cb(); },
    read() {},
    destroy: common.mustCall(function(err, cb) {
      assert.strictEqual(err, expected);
      cb();
    })
  });
  duplex.resume();

  duplex.on('end', common.mustNotCall('no end event'));
  duplex.on('finish', common.mustNotCall('no finish event'));

  // error is swallowed by the custom _destroy
  duplex.on('error', common.mustNotCall('no error event'));

  duplex.destroy(expected);
  assert.strictEqual(duplex.destroyed, true);
}

{
  const duplex = new Duplex({
    write(chunk, enc, cb) { cb(); },
    read() {}
  });

  duplex._destroy = common.mustCall(function(err, cb) {
    assert.strictEqual(err, null);
    cb();
  });

  duplex.destroy();
  assert.strictEqual(duplex.destroyed, true);
}

{
  const duplex = new Duplex({
    write(chunk, enc, cb) { cb(); },
    read() {}
  });
  duplex.resume();

  duplex._destroy = common.mustCall(function(err, cb) {
    assert.strictEqual(err, null);
    process.nextTick(() => {
      this.push(null);
      this.end();
      cb();
    });
  });

  const fail = common.mustNotCall('no finish or end event');

  duplex.on('finish', fail);
  duplex.on('end', fail);

  duplex.destroy();

  duplex.removeListener('end', fail);
  duplex.removeListener('finish', fail);
  duplex.on('end', common.mustCall());
  duplex.on('finish', common.mustCall());
  assert.strictEqual(duplex.destroyed, true);
}

{
  const duplex = new Duplex({
    write(chunk, enc, cb) { cb(); },
    read() {}
  });

  const expected = new Error('kaboom');

  duplex._destroy = common.mustCall(function(err, cb) {
    assert.strictEqual(err, null);
    cb(expected);
  });

  duplex.on('finish', common.mustNotCall('no finish event'));
  duplex.on('end', common.mustNotCall('no end event'));
  duplex.on('error', common.mustCall((err) => {
    assert.strictEqual(err, expected);
  }));

  duplex.destroy();
  assert.strictEqual(duplex.destroyed, true);
}

{
  const duplex = new Duplex({
    write(chunk, enc, cb) { cb(); },
    read() {},
    allowHalfOpen: true
  });
  duplex.resume();

  duplex.on('finish', common.mustCall());
  duplex.on('end', common.mustCall());

  duplex.destroy();
  assert.strictEqual(duplex.destroyed, true);
}

{
  const duplex = new Duplex({
    write(chunk, enc, cb) { cb(); },
    read() {},
  });

  duplex.destroyed = true;
  assert.strictEqual(duplex.destroyed, true);

  // the internal destroy() mechanism should not be triggered
  duplex.on('finish', common.mustNotCall());
  duplex.on('end', common.mustNotCall());
  duplex.destroy();
}

{
  function MyDuplex() {
    assert.strictEqual(this.destroyed, false);
    this.destroyed = false;
    Duplex.call(this);
  }

  inherits(MyDuplex, Duplex);

  new MyDuplex();
}
