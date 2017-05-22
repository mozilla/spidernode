// This file was procedurally generated from the following sources:
// - src/generators/yield-spread-arr-multiple.case
// - src/generators/default/class-decl-method.template
/*---
description: Use yield value in a array spread position (Geenerator method as a ClassDeclaration element)
esid: prod-GeneratorMethod
flags: [generated]
includes: [compareArray.js]
info: |
    ClassElement :
      MethodDefinition

    MethodDefinition :
      GeneratorMethod

    14.4 Generator Function Definitions

    GeneratorMethod :
      * PropertyName ( UniqueFormalParameters ) { GeneratorBody }

    Array Initializer

    SpreadElement[Yield, Await]:
      ...AssignmentExpression[+In, ?Yield, ?Await]

---*/
var arr = ['a', 'b', 'c'];
var item;

var callCount = 0;

class C { *gen() {
    callCount += 1;
    yield [...yield yield];
}}

var gen = C.prototype.gen;

var iter = gen();

iter.next(false);
item = iter.next(['a', 'b', 'c']);
item = iter.next(item.value);

assert(compareArray(item.value, arr));
assert.sameValue(item.done, false);

assert.sameValue(callCount, 1);

reportCompare(0, 0);
