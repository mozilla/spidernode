'use strict';

var http = require('http');

var fixed = 'C'.repeat(20 * 1024);
var storedBytes = Object.create(null);
var storedBuffer = Object.create(null);
var storedUnicode = Object.create(null);

var useDomains = process.env.NODE_USE_DOMAINS;

// set up one global domain.
if (useDomains) {
  var domain = require('domain');
  var gdom = domain.create();
  gdom.on('error', function(er) {
    console.error('Error on global domain', er);
    throw er;
  });
  gdom.enter();
}

module.exports = http.createServer(function(req, res) {
  if (useDomains) {
    var dom = domain.create();
    dom.add(req);
    dom.add(res);
  }

  // URL format: /<type>/<length>/<chunks>/<responseBehavior>
  var params = req.url.split('/');
  var command = params[1];
  var body = '';
  var arg = params[2];
  var n_chunks = parseInt(params[3], 10);
  var resHow = (params.length >= 5 ? params[4] : 'normal');
  var status = 200;

  var n, i;
  if (command === 'bytes') {
    n = ~~arg;
    if (n <= 0)
      throw new Error('bytes called with n <= 0');
    if (storedBytes[n] === undefined) {
      storedBytes[n] = 'C'.repeat(n);
    }
    body = storedBytes[n];
  } else if (command === 'buffer') {
    n = ~~arg;
    if (n <= 0)
      throw new Error('buffer called with n <= 0');
    if (storedBuffer[n] === undefined) {
      storedBuffer[n] = Buffer.allocUnsafe(n);
      for (i = 0; i < n; i++) {
        storedBuffer[n][i] = 'C'.charCodeAt(0);
      }
    }
    body = storedBuffer[n];
  } else if (command === 'unicode') {
    n = ~~arg;
    if (n <= 0)
      throw new Error('unicode called with n <= 0');
    if (storedUnicode[n] === undefined) {
      storedUnicode[n] = '\u263A'.repeat(n);
    }
    body = storedUnicode[n];
  } else if (command === 'quit') {
    res.connection.server.close();
    body = 'quitting';
  } else if (command === 'fixed') {
    body = fixed;
  } else if (command === 'echo') {
    switch (resHow) {
      case 'setHeader':
        res.statusCode = 200;
        res.setHeader('Content-Type', 'text/plain');
        res.setHeader('Transfer-Encoding', 'chunked');
        break;
      case 'setHeaderWH':
        res.setHeader('Content-Type', 'text/plain');
        res.writeHead(200, { 'Transfer-Encoding': 'chunked' });
        break;
      default:
        res.writeHead(200, {
          'Content-Type': 'text/plain',
          'Transfer-Encoding': 'chunked'
        });
    }
    req.pipe(res);
    return;
  } else {
    status = 404;
    body = 'not found\n';
  }

  // example: http://localhost:port/bytes/512/4
  // sends a 512 byte body in 4 chunks of 128 bytes
  if (n_chunks > 0) {
    switch (resHow) {
      case 'setHeader':
        res.statusCode = status;
        res.setHeader('Content-Type', 'text/plain');
        res.setHeader('Transfer-Encoding', 'chunked');
        break;
      case 'setHeaderWH':
        res.setHeader('Content-Type', 'text/plain');
        res.writeHead(status, { 'Transfer-Encoding': 'chunked' });
        break;
      default:
        res.writeHead(status, {
          'Content-Type': 'text/plain',
          'Transfer-Encoding': 'chunked'
        });
    }
    // send body in chunks
    var len = body.length;
    var step = Math.floor(len / n_chunks) || 1;

    for (i = 0, n = (n_chunks - 1); i < n; ++i) {
      res.write(body.slice(i * step, i * step + step));
    }
    res.end(body.slice((n_chunks - 1) * step));
  } else {
    switch (resHow) {
      case 'setHeader':
        res.statusCode = status;
        res.setHeader('Content-Type', 'text/plain');
        res.setHeader('Content-Length', body.length.toString());
        break;
      case 'setHeaderWH':
        res.setHeader('Content-Type', 'text/plain');
        res.writeHead(status, { 'Content-Length': body.length.toString() });
        break;
      default:
        res.writeHead(status, {
          'Content-Type': 'text/plain',
          'Content-Length': body.length.toString()
        });
    }
    res.end(body);
  }
});
