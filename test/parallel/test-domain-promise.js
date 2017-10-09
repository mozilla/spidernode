'use strict';
const common = require('../common');
const assert = require('assert');
const domain = require('domain');
const fs = require('fs');
const vm = require('vm');

common.crashOnUnhandledRejection();

{
  const d = domain.create();

  d.run(common.mustCall(() => {
    Promise.resolve().then(common.mustCall(() => {
      assert.strictEqual(process.domain, d);
    }));
  }));
}

{
  const d = domain.create();

  d.run(common.mustCall(() => {
    Promise.resolve().then(() => {}).then(() => {}).then(common.mustCall(() => {
      assert.strictEqual(process.domain, d);
    }));
  }));
}

{
  const d = domain.create();

  d.run(common.mustCall(() => {
    vm.runInNewContext(`
      const promise = Promise.resolve();
      assert.strictEqual(promise.domain, undefined);
      promise.then(common.mustCall(() => {
        assert.strictEqual(process.domain, d);
      }));
    `, { common, assert, process, d });
  }));
}

{
  const d1 = domain.create();
  const d2 = domain.create();
  let p;
  d1.run(common.mustCall(() => {
    p = Promise.resolve(42);
  }));

  d2.run(common.mustCall(() => {
    p.then(common.mustCall((v) => {
      assert.strictEqual(process.domain, d2);
      assert.strictEqual(p.domain, d1);
    }));
  }));
}

{
  const d1 = domain.create();
  const d2 = domain.create();
  let p;
  d1.run(common.mustCall(() => {
    p = Promise.resolve(42);
  }));

  d2.run(common.mustCall(() => {
    p.then(p.domain.bind(common.mustCall((v) => {
      assert.strictEqual(process.domain, d1);
      assert.strictEqual(p.domain, d1);
    })));
  }));
}

{
  const d1 = domain.create();
  const d2 = domain.create();
  let p;
  d1.run(common.mustCall(() => {
    p = Promise.resolve(42);
  }));

  d1.run(common.mustCall(() => {
    d2.run(common.mustCall(() => {
      p.then(common.mustCall((v) => {
        assert.strictEqual(process.domain, d2);
        assert.strictEqual(p.domain, d1);
      }));
    }));
  }));
}

{
  const d1 = domain.create();
  const d2 = domain.create();
  let p;
  d1.run(common.mustCall(() => {
    p = Promise.reject(new Error('foobar'));
  }));

  d2.run(common.mustCall(() => {
    p.catch(common.mustCall((v) => {
      assert.strictEqual(process.domain, d2);
      assert.strictEqual(p.domain, d1);
    }));
  }));
}

{
  const d = domain.create();

  d.run(common.mustCall(() => {
    Promise.resolve().then(common.mustCall(() => {
      setTimeout(common.mustCall(() => {
        assert.strictEqual(process.domain, d);
      }), 0);
    }));
  }));
}

{
  const d = domain.create();

  d.run(common.mustCall(() => {
    Promise.resolve().then(common.mustCall(() => {
      fs.readFile(__filename, common.mustCall(() => {
        assert.strictEqual(process.domain, d);
      }));
    }));
  }));
}
