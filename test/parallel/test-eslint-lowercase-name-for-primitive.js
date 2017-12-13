'use strict';

require('../common');

const RuleTester = require('../../tools/eslint').RuleTester;
const rule = require('../../tools/eslint-rules/lowercase-name-for-primitive');

const valid = [
  'string',
  'number',
  'boolean',
  'null',
  'undefined'
];

new RuleTester().run('lowercase-name-for-primitive', rule, {
  valid: [
    'new errors.TypeError("ERR_INVALID_ARG_TYPE", "a", ["string", "number"])',
    ...valid.map((name) =>
      `new errors.TypeError("ERR_INVALID_ARG_TYPE", "name", "${name}")`
    )
  ],
  invalid: [
    {
      code: 'new errors.TypeError("ERR_INVALID_ARG_TYPE", "a", "Number")',
      errors: [{ message: 'primitive should use lowercase: Number' }]
    },
    {
      code: 'new errors.TypeError("ERR_INVALID_ARG_TYPE", "a", "STRING")',
      errors: [{ message: 'primitive should use lowercase: STRING' }]
    },
    {
      code: 'new errors.TypeError("ERR_INVALID_ARG_TYPE", "a",' +
            '["String", "Number"])',
      errors: [
        { message: 'primitive should use lowercase: String' },
        { message: 'primitive should use lowercase: Number' }
      ]
    }
  ]
});
