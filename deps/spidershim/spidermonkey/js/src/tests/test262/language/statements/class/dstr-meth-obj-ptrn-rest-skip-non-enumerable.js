// This file was procedurally generated from the following sources:
// - src/dstr-binding/obj-ptrn-rest-skip-non-enumerable.case
// - src/dstr-binding/default/cls-decl-meth.template
/*---
description: Rest object doesn't contain non-enumerable properties (class expression method)
esid: sec-runtime-semantics-bindingclassdeclarationevaluation
es6id: 14.5.15
features: [object-rest, destructuring-binding]
flags: [generated]
includes: [propertyHelper.js]
info: |
    ClassDeclaration : class BindingIdentifier ClassTail

    1. Let className be StringValue of BindingIdentifier.
    2. Let value be the result of ClassDefinitionEvaluation of ClassTail with
       argument className.
    [...]

    14.5.14 Runtime Semantics: ClassDefinitionEvaluation

    21. For each ClassElement m in order from methods
        a. If IsStatic of m is false, then
           i. Let status be the result of performing
              PropertyDefinitionEvaluation for m with arguments proto and
              false.
        [...]

    14.3.8 Runtime Semantics: DefineMethod

    MethodDefinition : PropertyName ( StrictFormalParameters ) { FunctionBody }

    [...]
    6. Let closure be FunctionCreate(kind, StrictFormalParameters, FunctionBody,
       scope, strict). If functionPrototype was passed as a parameter then pass its
       value as the functionPrototype optional argument of FunctionCreate.
    [...]

    9.2.1 [[Call]] ( thisArgument, argumentsList)

    [...]
    7. Let result be OrdinaryCallEvaluateBody(F, argumentsList).
    [...]

    9.2.1.3 OrdinaryCallEvaluateBody ( F, argumentsList )

    1. Let status be FunctionDeclarationInstantiation(F, argumentsList).
    [...]

    9.2.12 FunctionDeclarationInstantiation(func, argumentsList)

    [...]
    23. Let iteratorRecord be Record {[[iterator]]:
        CreateListIterator(argumentsList), [[done]]: false}.
    24. If hasDuplicates is true, then
        [...]
    25. Else,
        b. Let formalStatus be IteratorBindingInitialization for formals with
           iteratorRecord and env as arguments.
    [...]
---*/
var o = {a: 3, b: 4};
Object.defineProperty(o, "x", { value: 4, enumerable: false });

var callCount = 0;
class C {
  method({...rest}) {
    assert.sameValue(rest.a, 3);
    assert.sameValue(rest.b, 4);
    assert.sameValue(rest.x, undefined);

    verifyEnumerable(rest, "a");
    verifyWritable(rest, "a");
    verifyConfigurable(rest, "a");

    verifyEnumerable(rest, "b");
    verifyWritable(rest, "b");
    verifyConfigurable(rest, "b");

    callCount = callCount + 1;
  }
};

new C().method(o);
assert.sameValue(callCount, 1, 'method invoked exactly once');

reportCompare(0, 0);
