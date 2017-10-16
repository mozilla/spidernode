// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Operator use ToString
es5id: 15.1.2.2_A1_T1
es6id: 18.2.5
esid: sec-parseint-string-radix
description: Checking for boolean primitive
---*/

assert.sameValue(parseInt(true), NaN, "true");
assert.sameValue(parseInt(false), NaN, "false");

reportCompare(0, 0);
