// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.1.1.3-0
es6id: 18.1.3
esid: sec-undefined
description: >
    Global.undefined is a data property with default attribute values
    (false)
---*/

var desc = Object.getOwnPropertyDescriptor(this, 'undefined');

assert.sameValue(desc.writable, false, 'desc.writable');
assert.sameValue(desc.enumerable, false, 'desc.enumerable');
assert.sameValue(desc.configurable, false, 'desc.configurable');

reportCompare(0, 0);
