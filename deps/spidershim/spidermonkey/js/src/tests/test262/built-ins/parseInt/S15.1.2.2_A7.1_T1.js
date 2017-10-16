// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: If Z is empty, return NaN
es5id: 15.1.2.2_A7.1_T1
es6id: 18.2.5
esid: sec-parseint-string-radix
description: Complex test. R in [2, 36]
---*/

//CHECK#
for (var i = 2; i <= 36; i++) {
  assert.sameValue(parseInt("$string", i), NaN);
}

reportCompare(0, 0);
