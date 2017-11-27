// file: atomicsHelper.js
// Copyright (C) 2017 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: |
    The amount of slack allowed for testing time-related Atomics methods (i.e.
    wait and wake). The absolute value of the difference of the observed time
    and the expected time must be epsilon-close.
---*/
var $ATOMICS_MAX_TIME_EPSILON = 100;
