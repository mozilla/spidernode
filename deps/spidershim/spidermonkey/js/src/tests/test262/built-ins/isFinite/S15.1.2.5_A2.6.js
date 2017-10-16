// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The isFinite property has not prototype property
es5id: 15.1.2.5_A2.6
es6id: 18.2.2
esid: sec-isfinite-number
description: Checking isFinite.prototype
---*/

//CHECK#1
if (isFinite.prototype !== undefined) {
  $ERROR('#1: isFinite.prototype === undefined. Actual: ' + (isFinite.prototype));
}

reportCompare(0, 0);
