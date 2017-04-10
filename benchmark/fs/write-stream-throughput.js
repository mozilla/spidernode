// test the throughput of the fs.WriteStream class.
'use strict';

var path = require('path');
var common = require('../common.js');
var filename = path.resolve(__dirname, '.removeme-benchmark-garbage');
var fs = require('fs');

var bench = common.createBenchmark(main, {
  dur: [5],
  type: ['buf', 'asc', 'utf'],
  size: [2, 1024, 65535, 1024 * 1024]
});

function main(conf) {
  var dur = +conf.dur;
  var type = conf.type;
  var size = +conf.size;
  var encoding;

  var chunk;
  switch (type) {
    case 'buf':
      chunk = Buffer.alloc(size, 'b');
      break;
    case 'asc':
      chunk = 'a'.repeat(size);
      encoding = 'ascii';
      break;
    case 'utf':
      chunk = 'ü'.repeat(Math.ceil(size / 2));
      encoding = 'utf8';
      break;
    default:
      throw new Error('invalid type');
  }

  try { fs.unlinkSync(filename); } catch (e) {}

  var started = false;
  var ending = false;
  var ended = false;
  setTimeout(function() {
    ending = true;
    f.end();
  }, dur * 1000);

  var f = fs.createWriteStream(filename);
  f.on('drain', write);
  f.on('open', write);
  f.on('close', done);
  f.on('finish', function() {
    ended = true;
    var written = fs.statSync(filename).size / 1024;
    try { fs.unlinkSync(filename); } catch (e) {}
    bench.end(written / 1024);
  });


  function write() {
    // don't try to write after we end, even if a 'drain' event comes.
    // v0.8 streams are so sloppy!
    if (ending)
      return;

    if (!started) {
      started = true;
      bench.start();
    }

    while (false !== f.write(chunk, encoding));
  }

  function done() {
    if (!ended)
      f.emit('finish');
  }
}
