'use strict';

require('../common');

const RuleTester = require('../../tools/eslint').RuleTester;
const rule = require('../../tools/eslint-rules/require-buffer');
const ruleTester = new RuleTester({
  parserOptions: { ecmaVersion: 6 },
  env: { node: true }
});

const message = "Use const Buffer = require('buffer').Buffer; " +
                'at the beginning of this file';

const useStrict = '\'use strict\';\n\n';
const bufferModule = 'const { Buffer } = require(\'buffer\');\n';
const mockComment = '// Some Comment\n//\n// Another Comment\n\n';
const useBuffer = 'Buffer;';
ruleTester.run('require-buffer', rule, {
  valid: [
    'foo',
    'const Buffer = require("Buffer"); Buffer;',
    'const { Buffer } = require(\'buffer\'); Buffer;',
  ],
  invalid: [
    {
      code: useBuffer,
      errors: [{ message }],
      output: bufferModule + useBuffer,
    },
    {
      code: useStrict + useBuffer,
      errors: [{ message }],
      output: useStrict + bufferModule + useBuffer,
    },
    {
      code:  mockComment + useBuffer,
      errors: [{ message }],
      output:  mockComment + bufferModule + useBuffer,
    },
    {
      code:  mockComment + useStrict + useBuffer,
      errors: [{ message }],
      output:  mockComment + useStrict + bufferModule + useBuffer,
    },
  ]
});
