// |reftest| skip-if(release_or_beta) error:SyntaxError -- async-iteration is not released yet
// This file was procedurally generated from the following sources:
// - src/async-generators/yield-as-binding-identifier-escaped.case
// - src/async-generators/syntax/async-obj-method.template
/*---
description: yield is a reserved keyword within generator function bodies and may not be used as a binding identifier. (Async generator method)
esid: prod-AsyncGeneratorMethod
features: [async-iteration]
flags: [generated]
negative:
  phase: early
  type: SyntaxError
info: |
    Async Generator Function Definitions

    AsyncGeneratorMethod :
      async [no LineTerminator here] * PropertyName ( UniqueFormalParameters ) { AsyncGeneratorBody }


    BindingIdentifier : Identifier

    It is a Syntax Error if this production has a [Yield] parameter and
    StringValue of Identifier is "yield".

---*/

var obj = {
  async *method() {
    var yi\u0065ld;
  }
};
