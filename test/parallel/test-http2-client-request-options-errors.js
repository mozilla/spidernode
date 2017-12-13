'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const http2 = require('http2');

// Check if correct errors are emitted when wrong type of data is passed
// to certain options of ClientHttp2Session request method

const optionsToTest = {
  endStream: 'boolean',
  getTrailers: 'function',
  weight: 'number',
  parent: 'number',
  exclusive: 'boolean',
  silent: 'boolean'
};

const types = {
  boolean: true,
  function: () => {},
  number: 1,
  object: {},
  array: [],
  null: null,
  symbol: Symbol('test')
};

const server = http2.createServer(common.mustNotCall());

server.listen(0, common.mustCall(() => {
  const port = server.address().port;
  const client = http2.connect(`http://localhost:${port}`);

  Object.keys(optionsToTest).forEach((option) => {
    Object.keys(types).forEach((type) => {
      if (type === optionsToTest[option]) {
        return;
      }

      common.expectsError(
        () => client.request({
          ':method': 'CONNECT',
          ':authority': `localhost:${port}`
        }, {
          [option]: types[type]
        }),
        {
          type: TypeError,
          code: 'ERR_INVALID_OPT_VALUE',
          message: `The value "${String(types[type])}" is invalid ` +
                   `for option "${option}"`
        }
      );
    });
  });

  server.close();
  client.destroy();
}));
