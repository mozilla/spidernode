// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-iterator-gen.h"

namespace v8 {
namespace internal {

using compiler::Node;

Node* IteratorBuiltinsAssembler::GetIterator(Node* context, Node* object,
                                             Label* if_exception,
                                             Variable* exception) {
  Node* method = GetProperty(context, object, factory()->iterator_symbol());
  GotoIfException(method, if_exception, exception);

  Callable callable = CodeFactory::Call(isolate());
  Node* iterator = CallJS(callable, context, method, object);
  GotoIfException(iterator, if_exception, exception);

  Label done(this), if_notobject(this, Label::kDeferred);
  GotoIf(TaggedIsSmi(iterator), &if_notobject);
  Branch(IsJSReceiver(iterator), &done, &if_notobject);

  BIND(&if_notobject);
  {
    Node* ret =
        CallRuntime(Runtime::kThrowTypeError, context,
                    SmiConstant(MessageTemplate::kNotAnIterator), iterator);
    GotoIfException(ret, if_exception, exception);
    Unreachable();
  }

  BIND(&done);
  return iterator;
}

Node* IteratorBuiltinsAssembler::IteratorStep(Node* context, Node* iterator,
                                              Label* if_done,
                                              Node* fast_iterator_result_map,
                                              Label* if_exception,
                                              Variable* exception) {
  DCHECK_NOT_NULL(if_done);

  // IteratorNext
  Node* next_method = GetProperty(context, iterator, factory()->next_string());
  GotoIfException(next_method, if_exception, exception);

  // 1. a. Let result be ? Invoke(iterator, "next", « »).
  Callable callable = CodeFactory::Call(isolate());
  Node* result = CallJS(callable, context, next_method, iterator);
  GotoIfException(result, if_exception, exception);

  // 3. If Type(result) is not Object, throw a TypeError exception.
  Label if_notobject(this, Label::kDeferred), return_result(this);
  GotoIf(TaggedIsSmi(result), &if_notobject);
  GotoIfNot(IsJSReceiver(result), &if_notobject);

  VARIABLE(var_done, MachineRepresentation::kTagged);

  if (fast_iterator_result_map != nullptr) {
    // Fast iterator result case:
    Label if_generic(this);

    // 4. Return result.
    Node* map = LoadMap(result);
    GotoIfNot(WordEqual(map, fast_iterator_result_map), &if_generic);

    // IteratorComplete
    // 2. Return ToBoolean(? Get(iterResult, "done")).
    Node* done = LoadObjectField(result, JSIteratorResult::kDoneOffset);
    CSA_ASSERT(this, IsBoolean(done));
    var_done.Bind(done);
    Goto(&return_result);

    BIND(&if_generic);
  }

  // Generic iterator result case:
  {
    // IteratorComplete
    // 2. Return ToBoolean(? Get(iterResult, "done")).
    Node* done = GetProperty(context, result, factory()->done_string());
    GotoIfException(done, if_exception, exception);
    var_done.Bind(done);

    Label to_boolean(this, Label::kDeferred);
    GotoIf(TaggedIsSmi(done), &to_boolean);
    Branch(IsBoolean(done), &return_result, &to_boolean);

    BIND(&to_boolean);
    var_done.Bind(CallBuiltin(Builtins::kToBoolean, context, done));
    Goto(&return_result);
  }

  BIND(&if_notobject);
  {
    Node* ret =
        CallRuntime(Runtime::kThrowIteratorResultNotAnObject, context, result);
    GotoIfException(ret, if_exception, exception);
    Unreachable();
  }

  BIND(&return_result);
  GotoIf(IsTrue(var_done.value()), if_done);
  return result;
}

Node* IteratorBuiltinsAssembler::IteratorValue(Node* context, Node* result,
                                               Node* fast_iterator_result_map,
                                               Label* if_exception,
                                               Variable* exception) {
  CSA_ASSERT(this, IsJSReceiver(result));

  Label exit(this);
  VARIABLE(var_value, MachineRepresentation::kTagged);
  if (fast_iterator_result_map != nullptr) {
    // Fast iterator result case:
    Label if_generic(this);
    Node* map = LoadMap(result);
    GotoIfNot(WordEqual(map, fast_iterator_result_map), &if_generic);
    var_value.Bind(LoadObjectField(result, JSIteratorResult::kValueOffset));
    Goto(&exit);

    BIND(&if_generic);
  }

  // Generic iterator result case:
  {
    Node* value = GetProperty(context, result, factory()->value_string());
    GotoIfException(value, if_exception, exception);
    var_value.Bind(value);
    Goto(&exit);
  }

  BIND(&exit);
  return var_value.value();
}

void IteratorBuiltinsAssembler::IteratorCloseOnException(Node* context,
                                                         Node* iterator,
                                                         Label* if_exception,
                                                         Variable* exception) {
  // Perform ES #sec-iteratorclose when an exception occurs. This simpler
  // algorithm does not include redundant steps which are never reachable from
  // the spec IteratorClose algorithm.
  DCHECK_NOT_NULL(if_exception);
  DCHECK_NOT_NULL(exception);
  CSA_ASSERT(this, IsNotTheHole(exception->value()));
  CSA_ASSERT(this, IsJSReceiver(iterator));

  // Let return be ? GetMethod(iterator, "return").
  Node* method = GetProperty(context, iterator, factory()->return_string());
  GotoIfException(method, if_exception, exception);

  // If return is undefined, return Completion(completion).
  GotoIf(Word32Or(IsUndefined(method), IsNull(method)), if_exception);

  {
    // Let innerResult be Call(return, iterator, « »).
    // If an exception occurs, the original exception remains bound
    Node* inner_result =
        CallJS(CodeFactory::Call(isolate()), context, method, iterator);
    GotoIfException(inner_result, if_exception, nullptr);

    // (If completion.[[Type]] is throw) return Completion(completion).
    Goto(if_exception);
  }
}

void IteratorBuiltinsAssembler::IteratorCloseOnException(Node* context,
                                                         Node* iterator,
                                                         Variable* exception) {
  Label rethrow(this, Label::kDeferred);
  IteratorCloseOnException(context, iterator, &rethrow, exception);

  BIND(&rethrow);
  CallRuntime(Runtime::kReThrow, context, exception->value());
  Unreachable();
}

}  // namespace internal
}  // namespace v8
