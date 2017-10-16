'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const http2 = require('http2');

const invalidOptions = [() => {}, 1, 'test', null, undefined];
const invalidArgTypeError = {
  type: TypeError,
  code: 'ERR_INVALID_ARG_TYPE',
  message: 'The "options" argument must be of type object'
};

// Error if options are not passed to createSecureServer
invalidOptions.forEach((invalidOption) =>
  common.expectsError(
    () => http2.createSecureServer(invalidOption),
    invalidArgTypeError
  )
);
