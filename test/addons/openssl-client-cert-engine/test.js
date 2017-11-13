'use strict';
const common = require('../../common');
const fixture = require('../../common/fixtures');

if (!common.hasCrypto)
  common.skip('missing crypto');

const fs = require('fs');
const path = require('path');

const engine = path.join(__dirname,
                         `/build/${common.buildType}/testengine.engine`);

if (!fs.existsSync(engine))
  common.skip('no client cert engine');

const assert = require('assert');
const https = require('https');

const agentKey = fs.readFileSync(fixture.path('/keys/agent1-key.pem'));
const agentCert = fs.readFileSync(fixture.path('/keys/agent1-cert.pem'));
const agentCa = fs.readFileSync(fixture.path('/keys/ca1-cert.pem'));

const port = common.PORT;

const serverOptions = {
  key: agentKey,
  cert: agentCert,
  ca: agentCa,
  requestCert: true,
  rejectUnauthorized: true
};

const server = https.createServer(serverOptions, (req, res) => {
  res.writeHead(200);
  res.end('hello world');
}).listen(port, common.localhostIPv4, () => {
  const clientOptions = {
    method: 'GET',
    host: common.localhostIPv4,
    port: port,
    path: '/test',
    clientCertEngine: engine,  // engine will provide key+cert
    rejectUnauthorized: false, // prevent failing on self-signed certificates
    headers: {}
  };

  const req = https.request(clientOptions, common.mustCall(function(response) {
    let body = '';
    response.setEncoding('utf8');
    response.on('data', function(chunk) {
      body += chunk;
    });

    response.on('end', common.mustCall(function() {
      assert.strictEqual(body, 'hello world');
      server.close();
    }));
  }));

  req.end();
});
