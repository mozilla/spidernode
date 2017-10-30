// |reftest| skip -- class-fields is not supported
// This file was procedurally generated from the following sources:
// - src/class-fields/static-computed-names.case
// - src/class-fields/default/cls-decl-new-sc-line-method.template
/*---
description: Static Computed property names (field definitions followed by a method in a new line with a semicolon)
esid: prod-FieldDefinition
features: [computed-property-names, class-fields]
flags: [generated]
includes: [propertyHelper.js]
info: |
    ClassElement:
      ...
      FieldDefinition ;
      static FieldDefinition ;

    FieldDefinition:
      ClassElementName Initializer_opt

    ClassElementName:
      PropertyName

---*/


class C {
  static ["a"] = 42; ["a"] = 39;
  m() { return 42; }
}

var c = new C();

assert.sameValue(c.m(), 42);
assert.sameValue(c.m, C.prototype.m);
assert.sameValue(Object.hasOwnProperty.call(c, "m"), false);

verifyProperty(C.prototype, "m", {
  enumerable: false,
  configurable: true,
  writable: true,
});

assert.sameValue(Object.hasOwnProperty.call(C.prototype, "a"), false);

verifyProperty(C, "a", {
  value: 42,
  enumerable: true,
  writable: true,
  configurable: true
});

verifyProperty(c, "a", {
  value: 39,
  enumerable: true,
  writable: true,
  configurable: true
});

reportCompare(0, 0);
