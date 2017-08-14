'use strict';
const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const tls = require('tls');
const fixtures = require('../common/fixtures');

const ca1 = fixtures.readKey('ca1-cert.pem', 'utf8');
const ca2 = fixtures.readKey('ca2-cert.pem', 'utf8');
const cert = fixtures.readKey('agent3-cert.pem', 'utf8');
const key = fixtures.readKey('agent3-key.pem', 'utf8');

function test(ca, next) {
  const server = tls.createServer({ ca, cert, key }, function(conn) {
    this.close();
    conn.end();
  });

  server.addContext('agent3', { ca, cert, key });

  const host = common.localhostIPv4;
  server.listen(0, host, function() {
    tls.connect({ servername: 'agent3', host, port: this.address().port, ca });
  });

  if (next) {
    server.once('close', next);
  }
}

const array = [ca1, ca2];
const string = `${ca1}\n${ca2}`;
test(array, common.mustCall(() => test(string)));
