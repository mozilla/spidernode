'use strict';

const common = require('../common');

const assert = require('assert');
const dns = require('dns');
const net = require('net');

let running = false;
const queue = [];

const isIPv4 = net.isIPv4;
const isIPv6 = net.isIPv6;

dns.setServers([ '8.8.8.8', '8.8.4.4' ]);

function checkWrap(req) {
  assert.ok(typeof req === 'object');
}

const checkers = {
  checkA(r) {
    assert.ok(isIPv4(r.address));
    assert.strictEqual(typeof r.ttl, 'number');
    assert.strictEqual(r.type, 'A');
  },
  checkAAAA(r) {
    assert.ok(isIPv6(r.address));
    assert.strictEqual(typeof r.ttl, 'number');
    assert.strictEqual(r.type, 'AAAA');
  },
  checkCNAME(r) {
    assert.ok(r.value);
    assert.strictEqual(typeof r.value, 'string');
    assert.strictEqual(r.type, 'CNAME');
  },
  checkMX(r) {
    assert.strictEqual(typeof r.exchange, 'string');
    assert.strictEqual(typeof r.priority, 'number');
    assert.strictEqual(r.type, 'MX');
  },
  checkNAPTR(r) {
    assert.strictEqual(typeof r.flags, 'string');
    assert.strictEqual(typeof r.service, 'string');
    assert.strictEqual(typeof r.regexp, 'string');
    assert.strictEqual(typeof r.replacement, 'string');
    assert.strictEqual(typeof r.order, 'number');
    assert.strictEqual(typeof r.preference, 'number');
    assert.strictEqual(r.type, 'NAPTR');
  },
  checkNS(r) {
    assert.strictEqual(typeof r.value, 'string');
    assert.strictEqual(r.type, 'NS');
  },
  checkPTR(r) {
    assert.strictEqual(typeof r.value, 'string');
    assert.strictEqual(r.type, 'PTR');
  },
  checkTXT(r) {
    assert.ok(Array.isArray(r.entries));
    assert.ok(r.entries.length > 0);
    r.entries.forEach((txt) => {
      assert(txt.startsWith('v=spf1'));
    });
    assert.strictEqual(r.type, 'TXT');
  },
  checkSOA(r) {
    assert.strictEqual(typeof r.nsname, 'string');
    assert.strictEqual(typeof r.hostmaster, 'string');
    assert.strictEqual(typeof r.serial, 'number');
    assert.strictEqual(typeof r.refresh, 'number');
    assert.strictEqual(typeof r.retry, 'number');
    assert.strictEqual(typeof r.expire, 'number');
    assert.strictEqual(typeof r.minttl, 'number');
    assert.strictEqual(r.type, 'SOA');
  },
  checkSRV(r) {
    assert.strictEqual(typeof r.name, 'string');
    assert.strictEqual(typeof r.port, 'number');
    assert.strictEqual(typeof r.priority, 'number');
    assert.strictEqual(typeof r.weight, 'number');
    assert.strictEqual(r.type, 'SRV');
  }
};

function TEST(f) {
  function next() {
    const f = queue.shift();
    if (f) {
      running = true;
      f(done);
    }
  }

  function done() {
    running = false;
    process.nextTick(next);
  }

  queue.push(f);

  if (!running) {
    next();
  }
}

TEST(function test_google(done) {
  const req = dns.resolve(
    'google.com',
    'ANY',
    common.mustCall(function(err, ret) {
      assert.ifError(err);
      assert.ok(Array.isArray(ret));
      assert.ok(ret.length > 0);

      /* current google.com has A / AAAA / MX / NS / TXT and SOA records */
      const types = {};
      ret.forEach((obj) => {
        types[obj.type] = true;
        checkers[`check${obj.type}`](obj);
      });
      assert.ok(
        types.A && types.AAAA && types.MX &&
        types.NS && types.TXT && types.SOA);

      done();
    }));

  checkWrap(req);
});

TEST(function test_sip2sip_for_naptr(done) {
  const req = dns.resolve(
    'sip2sip.info',
    'ANY',
    common.mustCall(function(err, ret) {
      assert.ifError(err);
      assert.ok(Array.isArray(ret));
      assert.ok(ret.length > 0);

      /* current sip2sip.info has A / NS / NAPTR and SOA records */
      const types = {};
      ret.forEach((obj) => {
        types[obj.type] = true;
        checkers[`check${obj.type}`](obj);
      });
      assert.ok(types.A && types.NS && types.NAPTR && types.SOA);

      done();
    }));

  checkWrap(req);
});

TEST(function test_google_for_cname_and_srv(done) {
  const req = dns.resolve(
    '_jabber._tcp.google.com',
    'ANY',
    common.mustCall(function(err, ret) {
      assert.ifError(err);
      assert.ok(Array.isArray(ret));
      assert.ok(ret.length > 0);

      const types = {};
      ret.forEach((obj) => {
        types[obj.type] = true;
        checkers[`check${obj.type}`](obj);
      });
      assert.ok(types.SRV);

      done();
    }));

  checkWrap(req);
});

TEST(function test_ptr(done) {
  const req = dns.resolve(
    '8.8.8.8.in-addr.arpa',
    'ANY',
    common.mustCall(function(err, ret) {
      assert.ifError(err);
      assert.ok(Array.isArray(ret));
      assert.ok(ret.length > 0);

      /* current 8.8.8.8.in-addr.arpa has PTR record */
      const types = {};
      ret.forEach((obj) => {
        types[obj.type] = true;
        checkers[`check${obj.type}`](obj);
      });
      assert.ok(types.PTR);

      done();
    }));

  checkWrap(req);
});
