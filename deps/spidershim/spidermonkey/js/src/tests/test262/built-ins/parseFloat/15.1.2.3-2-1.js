// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.1.2.3-2-1
es6id: 18.2.4
esid: sec-parsefloat-string
description: >
    pareseFloat - 'trimmedString' is the empty string when inputString
    does not contain any such characters
---*/

assert.sameValue(parseFloat(""), NaN, 'parseFloat("")');

reportCompare(0, 0);
