// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/js-typed-lowering.h"

#include "src/builtins/builtins-utils.h"
#include "src/code-factory.h"
#include "src/compilation-dependencies.h"
#include "src/compiler/access-builder.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/linkage.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/operator-properties.h"
#include "src/type-cache.h"
#include "src/types.h"

namespace v8 {
namespace internal {
namespace compiler {

// A helper class to simplify the process of reducing a single binop node with a
// JSOperator. This class manages the rewriting of context, control, and effect
// dependencies during lowering of a binop and contains numerous helper
// functions for matching the types of inputs to an operation.
class JSBinopReduction final {
 public:
  JSBinopReduction(JSTypedLowering* lowering, Node* node)
      : lowering_(lowering), node_(node) {}

  bool GetBinaryNumberOperationHint(NumberOperationHint* hint) {
    if (lowering_->flags() & JSTypedLowering::kDeoptimizationEnabled) {
      DCHECK_NE(0, node_->op()->ControlOutputCount());
      DCHECK_EQ(1, node_->op()->EffectOutputCount());
      DCHECK_EQ(1, OperatorProperties::GetFrameStateInputCount(node_->op()));
      switch (BinaryOperationHintOf(node_->op())) {
        case BinaryOperationHint::kSignedSmall:
          *hint = NumberOperationHint::kSignedSmall;
          return true;
        case BinaryOperationHint::kSigned32:
          *hint = NumberOperationHint::kSigned32;
          return true;
        case BinaryOperationHint::kNumberOrOddball:
          *hint = NumberOperationHint::kNumberOrOddball;
          return true;
        case BinaryOperationHint::kAny:
        case BinaryOperationHint::kNone:
          break;
      }
    }
    return false;
  }

  bool GetCompareNumberOperationHint(NumberOperationHint* hint) {
    if (lowering_->flags() & JSTypedLowering::kDeoptimizationEnabled) {
      DCHECK_EQ(1, node_->op()->EffectOutputCount());
      switch (CompareOperationHintOf(node_->op())) {
        case CompareOperationHint::kSignedSmall:
          *hint = NumberOperationHint::kSignedSmall;
          return true;
        case CompareOperationHint::kNumber:
          *hint = NumberOperationHint::kNumber;
          return true;
        case CompareOperationHint::kNumberOrOddball:
          *hint = NumberOperationHint::kNumberOrOddball;
          return true;
        case CompareOperationHint::kAny:
        case CompareOperationHint::kNone:
          break;
      }
    }
    return false;
  }

  void ConvertInputsToNumber() {
    // To convert the inputs to numbers, we have to provide frame states
    // for lazy bailouts in the ToNumber conversions.
    // We use a little hack here: we take the frame state before the binary
    // operation and use it to construct the frame states for the conversion
    // so that after the deoptimization, the binary operation IC gets
    // already converted values from full code. This way we are sure that we
    // will not re-do any of the side effects.

    Node* left_input = nullptr;
    Node* right_input = nullptr;
    bool left_is_primitive = left_type()->Is(Type::PlainPrimitive());
    bool right_is_primitive = right_type()->Is(Type::PlainPrimitive());
    bool handles_exception = NodeProperties::IsExceptionalCall(node_);

    if (!left_is_primitive && !right_is_primitive && handles_exception) {
      ConvertBothInputsToNumber(&left_input, &right_input);
    } else {
      left_input = left_is_primitive
                       ? ConvertPlainPrimitiveToNumber(left())
                       : ConvertSingleInputToNumber(
                             left(), CreateFrameStateForLeftInput());
      right_input =
          right_is_primitive
              ? ConvertPlainPrimitiveToNumber(right())
              : ConvertSingleInputToNumber(
                    right(), CreateFrameStateForRightInput(left_input));
    }

    node_->ReplaceInput(0, left_input);
    node_->ReplaceInput(1, right_input);
  }

  void ConvertInputsToUI32(Signedness left_signedness,
                           Signedness right_signedness) {
    node_->ReplaceInput(0, ConvertToUI32(left(), left_signedness));
    node_->ReplaceInput(1, ConvertToUI32(right(), right_signedness));
  }

  void SwapInputs() {
    Node* l = left();
    Node* r = right();
    node_->ReplaceInput(0, r);
    node_->ReplaceInput(1, l);
  }

  // Remove all effect and control inputs and outputs to this node and change
  // to the pure operator {op}, possibly inserting a boolean inversion.
  Reduction ChangeToPureOperator(const Operator* op, bool invert = false,
                                 Type* type = Type::Any()) {
    DCHECK_EQ(0, op->EffectInputCount());
    DCHECK_EQ(false, OperatorProperties::HasContextInput(op));
    DCHECK_EQ(0, op->ControlInputCount());
    DCHECK_EQ(2, op->ValueInputCount());

    // Remove the effects from the node, and update its effect/control usages.
    if (node_->op()->EffectInputCount() > 0) {
      lowering_->RelaxEffectsAndControls(node_);
    }
    // Remove the inputs corresponding to context, effect, and control.
    NodeProperties::RemoveNonValueInputs(node_);
    // Finally, update the operator to the new one.
    NodeProperties::ChangeOp(node_, op);

    // TODO(jarin): Replace the explicit typing hack with a call to some method
    // that encapsulates changing the operator and re-typing.
    Type* node_type = NodeProperties::GetType(node_);
    NodeProperties::SetType(node_, Type::Intersect(node_type, type, zone()));

    if (invert) {
      // Insert an boolean not to invert the value.
      Node* value = graph()->NewNode(simplified()->BooleanNot(), node_);
      node_->ReplaceUses(value);
      // Note: ReplaceUses() smashes all uses, so smash it back here.
      value->ReplaceInput(0, node_);
      return lowering_->Replace(value);
    }
    return lowering_->Changed(node_);
  }

  Reduction ChangeToSpeculativeOperator(const Operator* op, bool invert,
                                        Type* upper_bound) {
    DCHECK_EQ(1, op->EffectInputCount());
    DCHECK_EQ(1, op->EffectOutputCount());
    DCHECK_EQ(false, OperatorProperties::HasContextInput(op));
    DCHECK_EQ(1, op->ControlInputCount());
    DCHECK_EQ(0, op->ControlOutputCount());
    DCHECK_EQ(0, OperatorProperties::GetFrameStateInputCount(op));
    DCHECK_EQ(2, op->ValueInputCount());

    DCHECK_EQ(1, node_->op()->EffectInputCount());
    DCHECK_EQ(1, node_->op()->EffectOutputCount());
    DCHECK_EQ(1, node_->op()->ControlInputCount());
    DCHECK_EQ(2, node_->op()->ValueInputCount());

    // Reconnect the control output to bypass the IfSuccess node and
    // possibly disconnect from the IfException node.
    for (Edge edge : node_->use_edges()) {
      Node* const user = edge.from();
      DCHECK(!user->IsDead());
      if (NodeProperties::IsControlEdge(edge)) {
        if (user->opcode() == IrOpcode::kIfSuccess) {
          user->ReplaceUses(NodeProperties::GetControlInput(node_));
          user->Kill();
        } else {
          DCHECK_EQ(user->opcode(), IrOpcode::kIfException);
          edge.UpdateTo(jsgraph()->Dead());
        }
      }
    }

    // Remove the frame state and the context.
    if (OperatorProperties::HasFrameStateInput(node_->op())) {
      node_->RemoveInput(NodeProperties::FirstFrameStateIndex(node_));
    }
    node_->RemoveInput(NodeProperties::FirstContextIndex(node_));

    NodeProperties::ChangeOp(node_, op);

    // Update the type to number.
    Type* node_type = NodeProperties::GetType(node_);
    NodeProperties::SetType(node_,
                            Type::Intersect(node_type, upper_bound, zone()));

    if (invert) {
      // Insert an boolean not to invert the value.
      Node* value = graph()->NewNode(simplified()->BooleanNot(), node_);
      node_->ReplaceUses(value);
      // Note: ReplaceUses() smashes all uses, so smash it back here.
      value->ReplaceInput(0, node_);
      return lowering_->Replace(value);
    }
    return lowering_->Changed(node_);
  }

  Reduction ChangeToPureOperator(const Operator* op, Type* type) {
    return ChangeToPureOperator(op, false, type);
  }

  Reduction ChangeToSpeculativeOperator(const Operator* op, Type* type) {
    return ChangeToSpeculativeOperator(op, false, type);
  }

  const Operator* NumberOp() {
    switch (node_->opcode()) {
      case IrOpcode::kJSAdd:
        return simplified()->NumberAdd();
      case IrOpcode::kJSSubtract:
        return simplified()->NumberSubtract();
      case IrOpcode::kJSMultiply:
        return simplified()->NumberMultiply();
      case IrOpcode::kJSDivide:
        return simplified()->NumberDivide();
      case IrOpcode::kJSModulus:
        return simplified()->NumberModulus();
      case IrOpcode::kJSBitwiseAnd:
        return simplified()->NumberBitwiseAnd();
      case IrOpcode::kJSBitwiseOr:
        return simplified()->NumberBitwiseOr();
      case IrOpcode::kJSBitwiseXor:
        return simplified()->NumberBitwiseXor();
      case IrOpcode::kJSShiftLeft:
        return simplified()->NumberShiftLeft();
      case IrOpcode::kJSShiftRight:
        return simplified()->NumberShiftRight();
      case IrOpcode::kJSShiftRightLogical:
        return simplified()->NumberShiftRightLogical();
      default:
        break;
    }
    UNREACHABLE();
    return nullptr;
  }

  const Operator* SpeculativeNumberOp(NumberOperationHint hint) {
    switch (node_->opcode()) {
      case IrOpcode::kJSAdd:
        return simplified()->SpeculativeNumberAdd(hint);
      case IrOpcode::kJSSubtract:
        return simplified()->SpeculativeNumberSubtract(hint);
      case IrOpcode::kJSMultiply:
        return simplified()->SpeculativeNumberMultiply(hint);
      case IrOpcode::kJSDivide:
        return simplified()->SpeculativeNumberDivide(hint);
      case IrOpcode::kJSModulus:
        return simplified()->SpeculativeNumberModulus(hint);
      case IrOpcode::kJSBitwiseAnd:
        return simplified()->SpeculativeNumberBitwiseAnd(hint);
      case IrOpcode::kJSBitwiseOr:
        return simplified()->SpeculativeNumberBitwiseOr(hint);
      case IrOpcode::kJSBitwiseXor:
        return simplified()->SpeculativeNumberBitwiseXor(hint);
      case IrOpcode::kJSShiftLeft:
        return simplified()->SpeculativeNumberShiftLeft(hint);
      case IrOpcode::kJSShiftRight:
        return simplified()->SpeculativeNumberShiftRight(hint);
      case IrOpcode::kJSShiftRightLogical:
        return simplified()->SpeculativeNumberShiftRightLogical(hint);
      default:
        break;
    }
    UNREACHABLE();
    return nullptr;
  }

  bool LeftInputIs(Type* t) { return left_type()->Is(t); }

  bool RightInputIs(Type* t) { return right_type()->Is(t); }

  bool OneInputIs(Type* t) { return LeftInputIs(t) || RightInputIs(t); }

  bool BothInputsAre(Type* t) { return LeftInputIs(t) && RightInputIs(t); }

  bool OneInputCannotBe(Type* t) {
    return !left_type()->Maybe(t) || !right_type()->Maybe(t);
  }

  bool NeitherInputCanBe(Type* t) {
    return !left_type()->Maybe(t) && !right_type()->Maybe(t);
  }

  Node* effect() { return NodeProperties::GetEffectInput(node_); }
  Node* control() { return NodeProperties::GetControlInput(node_); }
  Node* context() { return NodeProperties::GetContextInput(node_); }
  Node* left() { return NodeProperties::GetValueInput(node_, 0); }
  Node* right() { return NodeProperties::GetValueInput(node_, 1); }
  Type* left_type() { return NodeProperties::GetType(node_->InputAt(0)); }
  Type* right_type() { return NodeProperties::GetType(node_->InputAt(1)); }
  Type* type() { return NodeProperties::GetType(node_); }

  SimplifiedOperatorBuilder* simplified() { return lowering_->simplified(); }
  Graph* graph() const { return lowering_->graph(); }
  JSGraph* jsgraph() { return lowering_->jsgraph(); }
  JSOperatorBuilder* javascript() { return lowering_->javascript(); }
  CommonOperatorBuilder* common() { return jsgraph()->common(); }
  Zone* zone() const { return graph()->zone(); }

 private:
  JSTypedLowering* lowering_;  // The containing lowering instance.
  Node* node_;                 // The original node.

  Node* CreateFrameStateForLeftInput() {
    // Deoptimization is disabled => return dummy frame state instead.
    Node* dummy_state = NodeProperties::GetFrameStateInput(node_);
    DCHECK(OpParameter<FrameStateInfo>(dummy_state).bailout_id().IsNone());
    return dummy_state;
  }

  Node* CreateFrameStateForRightInput(Node* converted_left) {
    // Deoptimization is disabled => return dummy frame state instead.
    Node* dummy_state = NodeProperties::GetFrameStateInput(node_);
    DCHECK(OpParameter<FrameStateInfo>(dummy_state).bailout_id().IsNone());
    return dummy_state;
  }

  Node* ConvertPlainPrimitiveToNumber(Node* node) {
    DCHECK(NodeProperties::GetType(node)->Is(Type::PlainPrimitive()));
    // Avoid inserting too many eager ToNumber() operations.
    Reduction const reduction = lowering_->ReduceJSToNumberInput(node);
    if (reduction.Changed()) return reduction.replacement();
    if (NodeProperties::GetType(node)->Is(Type::Number())) {
      return node;
    }
    return graph()->NewNode(simplified()->PlainPrimitiveToNumber(), node);
  }

  Node* ConvertSingleInputToNumber(Node* node, Node* frame_state) {
    DCHECK(!NodeProperties::GetType(node)->Is(Type::PlainPrimitive()));
    Node* const n = graph()->NewNode(javascript()->ToNumber(), node, context(),
                                     frame_state, effect(), control());
    Node* const if_success = graph()->NewNode(common()->IfSuccess(), n);
    NodeProperties::ReplaceControlInput(node_, if_success);
    NodeProperties::ReplaceUses(node_, node_, node_, node_, n);
    update_effect(n);
    return n;
  }

  void ConvertBothInputsToNumber(Node** left_result, Node** right_result) {
    Node* projections[2];

    // Find {IfSuccess} and {IfException} continuations of the operation.
    NodeProperties::CollectControlProjections(node_, projections, 2);
    Node* if_exception = projections[1];
    Node* if_success = projections[0];

    // Insert two ToNumber() operations that both potentially throw.
    Node* left_state = CreateFrameStateForLeftInput();
    Node* left_conv =
        graph()->NewNode(javascript()->ToNumber(), left(), context(),
                         left_state, effect(), control());
    Node* left_success = graph()->NewNode(common()->IfSuccess(), left_conv);
    Node* right_state = CreateFrameStateForRightInput(left_conv);
    Node* right_conv =
        graph()->NewNode(javascript()->ToNumber(), right(), context(),
                         right_state, left_conv, left_success);
    Node* left_exception =
        graph()->NewNode(common()->IfException(), left_conv, left_conv);
    Node* right_exception =
        graph()->NewNode(common()->IfException(), right_conv, right_conv);
    NodeProperties::ReplaceControlInput(if_success, right_conv);
    update_effect(right_conv);

    // Wire conversions to existing {IfException} continuation.
    Node* exception_merge = if_exception;
    Node* exception_value =
        graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                         left_exception, right_exception, exception_merge);
    Node* exception_effect =
        graph()->NewNode(common()->EffectPhi(2), left_exception,
                         right_exception, exception_merge);
    for (Edge edge : exception_merge->use_edges()) {
      if (NodeProperties::IsEffectEdge(edge)) edge.UpdateTo(exception_effect);
      if (NodeProperties::IsValueEdge(edge)) edge.UpdateTo(exception_value);
    }
    NodeProperties::RemoveType(exception_merge);
    exception_merge->ReplaceInput(0, left_exception);
    exception_merge->ReplaceInput(1, right_exception);
    NodeProperties::ChangeOp(exception_merge, common()->Merge(2));

    *left_result = left_conv;
    *right_result = right_conv;
  }

  Node* ConvertToUI32(Node* node, Signedness signedness) {
    // Avoid introducing too many eager NumberToXXnt32() operations.
    Type* type = NodeProperties::GetType(node);
    if (signedness == kSigned) {
      if (!type->Is(Type::Signed32())) {
        node = graph()->NewNode(simplified()->NumberToInt32(), node);
      }
    } else {
      DCHECK_EQ(kUnsigned, signedness);
      if (!type->Is(Type::Unsigned32())) {
        node = graph()->NewNode(simplified()->NumberToUint32(), node);
      }
    }
    return node;
  }

  void update_effect(Node* effect) {
    NodeProperties::ReplaceEffectInput(node_, effect);
  }
};


// TODO(turbofan): js-typed-lowering improvements possible
// - immediately put in type bounds for all new nodes
// - relax effects from generic but not-side-effecting operations


JSTypedLowering::JSTypedLowering(Editor* editor,
                                 CompilationDependencies* dependencies,
                                 Flags flags, JSGraph* jsgraph, Zone* zone)
    : AdvancedReducer(editor),
      dependencies_(dependencies),
      flags_(flags),
      jsgraph_(jsgraph),
      true_type_(Type::Constant(factory()->true_value(), graph()->zone())),
      false_type_(Type::Constant(factory()->false_value(), graph()->zone())),
      the_hole_type_(
          Type::Constant(factory()->the_hole_value(), graph()->zone())),
      type_cache_(TypeCache::Get()) {
  for (size_t k = 0; k < arraysize(shifted_int32_ranges_); ++k) {
    double min = kMinInt / (1 << k);
    double max = kMaxInt / (1 << k);
    shifted_int32_ranges_[k] = Type::Range(min, max, graph()->zone());
  }
}

Reduction JSTypedLowering::ReduceJSAdd(Node* node) {
  JSBinopReduction r(this, node);
  NumberOperationHint hint;
  if (r.GetBinaryNumberOperationHint(&hint)) {
    if (hint == NumberOperationHint::kNumberOrOddball &&
        r.BothInputsAre(Type::PlainPrimitive()) &&
        r.NeitherInputCanBe(Type::StringOrReceiver())) {
      // JSAdd(x:-string, y:-string) => NumberAdd(ToNumber(x), ToNumber(y))
      r.ConvertInputsToNumber();
      return r.ChangeToPureOperator(simplified()->NumberAdd(), Type::Number());
    }
    return r.ChangeToSpeculativeOperator(
        simplified()->SpeculativeNumberAdd(hint), Type::Number());
  }
  if (r.BothInputsAre(Type::Number())) {
    // JSAdd(x:number, y:number) => NumberAdd(x, y)
    r.ConvertInputsToNumber();
    return r.ChangeToPureOperator(simplified()->NumberAdd(), Type::Number());
  }
  if ((r.BothInputsAre(Type::PlainPrimitive()) ||
       !(flags() & kDeoptimizationEnabled)) &&
      r.NeitherInputCanBe(Type::StringOrReceiver())) {
    // JSAdd(x:-string, y:-string) => NumberAdd(ToNumber(x), ToNumber(y))
    r.ConvertInputsToNumber();
    return r.ChangeToPureOperator(simplified()->NumberAdd(), Type::Number());
  }
  if (r.OneInputIs(Type::String())) {
    StringAddFlags flags = STRING_ADD_CHECK_NONE;
    if (!r.LeftInputIs(Type::String())) {
      flags = STRING_ADD_CONVERT_LEFT;
    } else if (!r.RightInputIs(Type::String())) {
      flags = STRING_ADD_CONVERT_RIGHT;
    }
    // JSAdd(x:string, y) => CallStub[StringAdd](x, y)
    // JSAdd(x, y:string) => CallStub[StringAdd](x, y)
    Callable const callable =
        CodeFactory::StringAdd(isolate(), flags, NOT_TENURED);
    CallDescriptor const* const desc = Linkage::GetStubCallDescriptor(
        isolate(), graph()->zone(), callable.descriptor(), 0,
        CallDescriptor::kNeedsFrameState, node->op()->properties());
    DCHECK_EQ(1, OperatorProperties::GetFrameStateInputCount(node->op()));
    node->InsertInput(graph()->zone(), 0,
                      jsgraph()->HeapConstant(callable.code()));
    NodeProperties::ChangeOp(node, common()->Call(desc));
    return Changed(node);
  }
  return NoChange();
}

Reduction JSTypedLowering::ReduceNumberBinop(Node* node) {
  JSBinopReduction r(this, node);
  NumberOperationHint hint;
  if (r.GetBinaryNumberOperationHint(&hint)) {
    if (hint == NumberOperationHint::kNumberOrOddball &&
        r.BothInputsAre(Type::PlainPrimitive())) {
      r.ConvertInputsToNumber();
      return r.ChangeToPureOperator(r.NumberOp(), Type::Number());
    }
    return r.ChangeToSpeculativeOperator(r.SpeculativeNumberOp(hint),
                                         Type::Number());
  }
  if (r.BothInputsAre(Type::PlainPrimitive()) ||
      !(flags() & kDeoptimizationEnabled)) {
    r.ConvertInputsToNumber();
    return r.ChangeToPureOperator(r.NumberOp(), Type::Number());
  }
  return NoChange();
}

Reduction JSTypedLowering::ReduceInt32Binop(Node* node) {
  JSBinopReduction r(this, node);
  NumberOperationHint hint;
  if (r.GetBinaryNumberOperationHint(&hint)) {
    return r.ChangeToSpeculativeOperator(r.SpeculativeNumberOp(hint),
                                         Type::Signed32());
  }
  if (r.BothInputsAre(Type::PlainPrimitive()) ||
      !(flags() & kDeoptimizationEnabled)) {
    r.ConvertInputsToNumber();
    r.ConvertInputsToUI32(kSigned, kSigned);
    return r.ChangeToPureOperator(r.NumberOp(), Type::Signed32());
  }
  return NoChange();
}

Reduction JSTypedLowering::ReduceUI32Shift(Node* node, Signedness signedness) {
  JSBinopReduction r(this, node);
  NumberOperationHint hint;
  if (r.GetBinaryNumberOperationHint(&hint)) {
    return r.ChangeToSpeculativeOperator(
        r.SpeculativeNumberOp(hint),
        signedness == kUnsigned ? Type::Unsigned32() : Type::Signed32());
  }
  if (r.BothInputsAre(Type::PlainPrimitive()) ||
      !(flags() & kDeoptimizationEnabled)) {
    r.ConvertInputsToNumber();
    r.ConvertInputsToUI32(signedness, kUnsigned);
    return r.ChangeToPureOperator(r.NumberOp(), signedness == kUnsigned
                                                    ? Type::Unsigned32()
                                                    : Type::Signed32());
  }
  return NoChange();
}

Reduction JSTypedLowering::ReduceJSComparison(Node* node) {
  JSBinopReduction r(this, node);
  if (r.BothInputsAre(Type::String())) {
    // If both inputs are definitely strings, perform a string comparison.
    const Operator* stringOp;
    switch (node->opcode()) {
      case IrOpcode::kJSLessThan:
        stringOp = simplified()->StringLessThan();
        break;
      case IrOpcode::kJSGreaterThan:
        stringOp = simplified()->StringLessThan();
        r.SwapInputs();  // a > b => b < a
        break;
      case IrOpcode::kJSLessThanOrEqual:
        stringOp = simplified()->StringLessThanOrEqual();
        break;
      case IrOpcode::kJSGreaterThanOrEqual:
        stringOp = simplified()->StringLessThanOrEqual();
        r.SwapInputs();  // a >= b => b <= a
        break;
      default:
        return NoChange();
    }
    r.ChangeToPureOperator(stringOp);
    return Changed(node);
  }

  NumberOperationHint hint;
  const Operator* less_than;
  const Operator* less_than_or_equal;
  if (r.BothInputsAre(Type::Signed32()) ||
      r.BothInputsAre(Type::Unsigned32())) {
    less_than = simplified()->NumberLessThan();
    less_than_or_equal = simplified()->NumberLessThanOrEqual();
  } else if (r.GetCompareNumberOperationHint(&hint)) {
    less_than = simplified()->SpeculativeNumberLessThan(hint);
    less_than_or_equal = simplified()->SpeculativeNumberLessThanOrEqual(hint);
  } else if (r.OneInputCannotBe(Type::StringOrReceiver()) &&
             (r.BothInputsAre(Type::PlainPrimitive()) ||
              !(flags() & kDeoptimizationEnabled))) {
    r.ConvertInputsToNumber();
    less_than = simplified()->NumberLessThan();
    less_than_or_equal = simplified()->NumberLessThanOrEqual();
  } else {
    return NoChange();
  }
  const Operator* comparison;
  switch (node->opcode()) {
    case IrOpcode::kJSLessThan:
      comparison = less_than;
      break;
    case IrOpcode::kJSGreaterThan:
      comparison = less_than;
      r.SwapInputs();  // a > b => b < a
      break;
    case IrOpcode::kJSLessThanOrEqual:
      comparison = less_than_or_equal;
      break;
    case IrOpcode::kJSGreaterThanOrEqual:
      comparison = less_than_or_equal;
      r.SwapInputs();  // a >= b => b <= a
      break;
    default:
      return NoChange();
  }
  if (comparison->EffectInputCount() > 0) {
    return r.ChangeToSpeculativeOperator(comparison, Type::Boolean());
  } else {
    return r.ChangeToPureOperator(comparison);
  }
}

Reduction JSTypedLowering::ReduceJSEqualTypeOf(Node* node, bool invert) {
  HeapObjectBinopMatcher m(node);
  if (m.left().IsJSTypeOf() && m.right().HasValue() &&
      m.right().Value()->IsString()) {
    Node* replacement;
    Node* input = m.left().InputAt(0);
    Handle<String> value = Handle<String>::cast(m.right().Value());
    if (String::Equals(value, factory()->boolean_string())) {
      replacement =
          graph()->NewNode(common()->Select(MachineRepresentation::kTagged),
                           graph()->NewNode(simplified()->ReferenceEqual(),
                                            input, jsgraph()->TrueConstant()),
                           jsgraph()->TrueConstant(),
                           graph()->NewNode(simplified()->ReferenceEqual(),
                                            input, jsgraph()->FalseConstant()));
    } else if (String::Equals(value, factory()->function_string())) {
      replacement = graph()->NewNode(simplified()->ObjectIsCallable(), input);
    } else if (String::Equals(value, factory()->number_string())) {
      replacement = graph()->NewNode(simplified()->ObjectIsNumber(), input);
    } else if (String::Equals(value, factory()->string_string())) {
      replacement = graph()->NewNode(simplified()->ObjectIsString(), input);
    } else if (String::Equals(value, factory()->undefined_string())) {
      replacement = graph()->NewNode(
          common()->Select(MachineRepresentation::kTagged),
          graph()->NewNode(simplified()->ReferenceEqual(), input,
                           jsgraph()->NullConstant()),
          jsgraph()->FalseConstant(),
          graph()->NewNode(simplified()->ObjectIsUndetectable(), input));
    } else {
      return NoChange();
    }
    if (invert) {
      replacement = graph()->NewNode(simplified()->BooleanNot(), replacement);
    }
    ReplaceWithValue(node, replacement);
    return Replace(replacement);
  }
  return NoChange();
}

Reduction JSTypedLowering::ReduceJSEqual(Node* node, bool invert) {
  Reduction const reduction = ReduceJSEqualTypeOf(node, invert);
  if (reduction.Changed()) return reduction;

  JSBinopReduction r(this, node);

  if (r.BothInputsAre(Type::String())) {
    return r.ChangeToPureOperator(simplified()->StringEqual(), invert);
  }
  if (r.BothInputsAre(Type::Boolean())) {
    return r.ChangeToPureOperator(simplified()->ReferenceEqual(), invert);
  }
  if (r.BothInputsAre(Type::Receiver())) {
    return r.ChangeToPureOperator(simplified()->ReferenceEqual(), invert);
  }
  if (r.OneInputIs(Type::Undetectable())) {
    RelaxEffectsAndControls(node);
    node->RemoveInput(r.LeftInputIs(Type::Undetectable()) ? 0 : 1);
    node->TrimInputCount(1);
    NodeProperties::ChangeOp(node, simplified()->ObjectIsUndetectable());
    if (invert) {
      // Insert an boolean not to invert the value.
      Node* value = graph()->NewNode(simplified()->BooleanNot(), node);
      node->ReplaceUses(value);
      // Note: ReplaceUses() smashes all uses, so smash it back here.
      value->ReplaceInput(0, node);
      return Replace(value);
    }
    return Changed(node);
  }

  NumberOperationHint hint;
  if (r.BothInputsAre(Type::Signed32()) ||
      r.BothInputsAre(Type::Unsigned32())) {
    return r.ChangeToPureOperator(simplified()->NumberEqual(), invert);
  } else if (r.GetCompareNumberOperationHint(&hint)) {
    return r.ChangeToSpeculativeOperator(
        simplified()->SpeculativeNumberEqual(hint), invert, Type::Boolean());
  } else if (r.BothInputsAre(Type::Number())) {
    return r.ChangeToPureOperator(simplified()->NumberEqual(), invert);
  }
  return NoChange();
}

Reduction JSTypedLowering::ReduceJSStrictEqual(Node* node, bool invert) {
  JSBinopReduction r(this, node);
  if (r.left() == r.right()) {
    // x === x is always true if x != NaN
    if (!r.left_type()->Maybe(Type::NaN())) {
      Node* replacement = jsgraph()->BooleanConstant(!invert);
      ReplaceWithValue(node, replacement);
      return Replace(replacement);
    }
  }
  if (r.OneInputCannotBe(Type::NumberOrSimdOrString())) {
    // For values with canonical representation (i.e. neither String, nor
    // Simd128Value nor Number) an empty type intersection means the values
    // cannot be strictly equal.
    if (!r.left_type()->Maybe(r.right_type())) {
      Node* replacement = jsgraph()->BooleanConstant(invert);
      ReplaceWithValue(node, replacement);
      return Replace(replacement);
    }
  }

  Reduction const reduction = ReduceJSEqualTypeOf(node, invert);
  if (reduction.Changed()) return reduction;

  if (r.OneInputIs(the_hole_type_)) {
    return r.ChangeToPureOperator(simplified()->ReferenceEqual(), invert);
  }
  if (r.OneInputIs(Type::Undefined())) {
    return r.ChangeToPureOperator(simplified()->ReferenceEqual(), invert);
  }
  if (r.OneInputIs(Type::Null())) {
    return r.ChangeToPureOperator(simplified()->ReferenceEqual(), invert);
  }
  if (r.OneInputIs(Type::Boolean())) {
    return r.ChangeToPureOperator(simplified()->ReferenceEqual(), invert);
  }
  if (r.OneInputIs(Type::Object())) {
    return r.ChangeToPureOperator(simplified()->ReferenceEqual(), invert);
  }
  if (r.OneInputIs(Type::Receiver())) {
    return r.ChangeToPureOperator(simplified()->ReferenceEqual(), invert);
  }
  if (r.BothInputsAre(Type::Unique())) {
    return r.ChangeToPureOperator(simplified()->ReferenceEqual(), invert);
  }
  if (r.BothInputsAre(Type::String())) {
    return r.ChangeToPureOperator(simplified()->StringEqual(), invert);
  }

  NumberOperationHint hint;
  if (r.BothInputsAre(Type::Signed32()) ||
      r.BothInputsAre(Type::Unsigned32())) {
    return r.ChangeToPureOperator(simplified()->NumberEqual(), invert);
  } else if (r.GetCompareNumberOperationHint(&hint)) {
    return r.ChangeToSpeculativeOperator(
        simplified()->SpeculativeNumberEqual(hint), invert, Type::Boolean());
  } else if (r.BothInputsAre(Type::Number())) {
    return r.ChangeToPureOperator(simplified()->NumberEqual(), invert);
  }
  return NoChange();
}

Reduction JSTypedLowering::ReduceJSToBoolean(Node* node) {
  Node* const input = node->InputAt(0);
  Type* const input_type = NodeProperties::GetType(input);
  if (input_type->Is(Type::Boolean())) {
    // JSToBoolean(x:boolean) => x
    return Replace(input);
  } else if (input_type->Is(Type::OrderedNumber())) {
    // JSToBoolean(x:ordered-number) => BooleanNot(NumberEqual(x,#0))
    RelaxEffectsAndControls(node);
    node->ReplaceInput(0, graph()->NewNode(simplified()->NumberEqual(), input,
                                           jsgraph()->ZeroConstant()));
    node->TrimInputCount(1);
    NodeProperties::ChangeOp(node, simplified()->BooleanNot());
    return Changed(node);
  } else if (input_type->Is(Type::Number())) {
    // JSToBoolean(x:number) => NumberLessThan(#0,NumberAbs(x))
    RelaxEffectsAndControls(node);
    node->ReplaceInput(0, jsgraph()->ZeroConstant());
    node->ReplaceInput(1, graph()->NewNode(simplified()->NumberAbs(), input));
    node->TrimInputCount(2);
    NodeProperties::ChangeOp(node, simplified()->NumberLessThan());
    return Changed(node);
  } else if (input_type->Is(Type::String())) {
    // JSToBoolean(x:string) => NumberLessThan(#0,x.length)
    FieldAccess const access = AccessBuilder::ForStringLength();
    Node* length = graph()->NewNode(simplified()->LoadField(access), input,
                                    graph()->start(), graph()->start());
    ReplaceWithValue(node, node, length);
    node->ReplaceInput(0, jsgraph()->ZeroConstant());
    node->ReplaceInput(1, length);
    NodeProperties::ChangeOp(node, simplified()->NumberLessThan());
    return Changed(node);
  }
  return NoChange();
}

Reduction JSTypedLowering::ReduceJSToInteger(Node* node) {
  Node* const input = NodeProperties::GetValueInput(node, 0);
  Type* const input_type = NodeProperties::GetType(input);
  if (input_type->Is(type_cache_.kIntegerOrMinusZero)) {
    // JSToInteger(x:integer) => x
    ReplaceWithValue(node, input);
    return Replace(input);
  }
  return NoChange();
}

Reduction JSTypedLowering::ReduceJSToLength(Node* node) {
  Node* input = NodeProperties::GetValueInput(node, 0);
  Type* input_type = NodeProperties::GetType(input);
  if (input_type->Is(type_cache_.kIntegerOrMinusZero)) {
    if (input_type->Max() <= 0.0) {
      input = jsgraph()->ZeroConstant();
    } else if (input_type->Min() >= kMaxSafeInteger) {
      input = jsgraph()->Constant(kMaxSafeInteger);
    } else {
      if (input_type->Min() <= 0.0) {
        input = graph()->NewNode(
            common()->Select(MachineRepresentation::kTagged),
            graph()->NewNode(simplified()->NumberLessThanOrEqual(), input,
                             jsgraph()->ZeroConstant()),
            jsgraph()->ZeroConstant(), input);
        input_type = Type::Range(0.0, input_type->Max(), graph()->zone());
        NodeProperties::SetType(input, input_type);
      }
      if (input_type->Max() > kMaxSafeInteger) {
        input = graph()->NewNode(
            common()->Select(MachineRepresentation::kTagged),
            graph()->NewNode(simplified()->NumberLessThanOrEqual(),
                             jsgraph()->Constant(kMaxSafeInteger), input),
            jsgraph()->Constant(kMaxSafeInteger), input);
        input_type =
            Type::Range(input_type->Min(), kMaxSafeInteger, graph()->zone());
        NodeProperties::SetType(input, input_type);
      }
    }
    ReplaceWithValue(node, input);
    return Replace(input);
  }
  return NoChange();
}

Reduction JSTypedLowering::ReduceJSToNumberInput(Node* input) {
  // Try constant-folding of JSToNumber with constant inputs.
  Type* input_type = NodeProperties::GetType(input);
  if (input_type->IsConstant()) {
    Handle<Object> input_value = input_type->AsConstant()->Value();
    if (input_value->IsString()) {
      return Replace(jsgraph()->Constant(
          String::ToNumber(Handle<String>::cast(input_value))));
    } else if (input_value->IsOddball()) {
      return Replace(jsgraph()->Constant(
          Oddball::ToNumber(Handle<Oddball>::cast(input_value))));
    }
  }
  if (input_type->Is(Type::Number())) {
    // JSToNumber(x:number) => x
    return Changed(input);
  }
  if (input_type->Is(Type::Undefined())) {
    // JSToNumber(undefined) => #NaN
    return Replace(jsgraph()->NaNConstant());
  }
  if (input_type->Is(Type::Null())) {
    // JSToNumber(null) => #0
    return Replace(jsgraph()->ZeroConstant());
  }
  return NoChange();
}

Reduction JSTypedLowering::ReduceJSToNumber(Node* node) {
  // Try to reduce the input first.
  Node* const input = node->InputAt(0);
  Reduction reduction = ReduceJSToNumberInput(input);
  if (reduction.Changed()) {
    ReplaceWithValue(node, reduction.replacement());
    return reduction;
  }
  Type* const input_type = NodeProperties::GetType(input);
  if (input_type->Is(Type::PlainPrimitive())) {
    RelaxEffectsAndControls(node);
    node->TrimInputCount(1);
    NodeProperties::ChangeOp(node, simplified()->PlainPrimitiveToNumber());
    return Changed(node);
  }
  return NoChange();
}

Reduction JSTypedLowering::ReduceJSToStringInput(Node* input) {
  if (input->opcode() == IrOpcode::kJSToString) {
    // Recursively try to reduce the input first.
    Reduction result = ReduceJSToString(input);
    if (result.Changed()) return result;
    return Changed(input);  // JSToString(JSToString(x)) => JSToString(x)
  }
  Type* input_type = NodeProperties::GetType(input);
  if (input_type->Is(Type::String())) {
    return Changed(input);  // JSToString(x:string) => x
  }
  if (input_type->Is(Type::Boolean())) {
    return Replace(graph()->NewNode(
        common()->Select(MachineRepresentation::kTagged), input,
        jsgraph()->HeapConstant(factory()->true_string()),
        jsgraph()->HeapConstant(factory()->false_string())));
  }
  if (input_type->Is(Type::Undefined())) {
    return Replace(jsgraph()->HeapConstant(factory()->undefined_string()));
  }
  if (input_type->Is(Type::Null())) {
    return Replace(jsgraph()->HeapConstant(factory()->null_string()));
  }
  // TODO(turbofan): js-typed-lowering of ToString(x:number)
  return NoChange();
}

Reduction JSTypedLowering::ReduceJSToString(Node* node) {
  // Try to reduce the input first.
  Node* const input = node->InputAt(0);
  Reduction reduction = ReduceJSToStringInput(input);
  if (reduction.Changed()) {
    ReplaceWithValue(node, reduction.replacement());
    return reduction;
  }
  return NoChange();
}

Reduction JSTypedLowering::ReduceJSToObject(Node* node) {
  DCHECK_EQ(IrOpcode::kJSToObject, node->opcode());
  Node* receiver = NodeProperties::GetValueInput(node, 0);
  Type* receiver_type = NodeProperties::GetType(receiver);
  Node* context = NodeProperties::GetContextInput(node);
  Node* frame_state = NodeProperties::GetFrameStateInput(node);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  if (receiver_type->Is(Type::Receiver())) {
    ReplaceWithValue(node, receiver, effect, control);
    return Replace(receiver);
  }

  // TODO(bmeurer/mstarzinger): Add support for lowering inside try blocks.
  if (receiver_type->Maybe(Type::NullOrUndefined()) &&
      NodeProperties::IsExceptionalCall(node)) {
    // ToObject throws for null or undefined inputs.
    return NoChange();
  }

  // Check whether {receiver} is a spec object.
  Node* check = graph()->NewNode(simplified()->ObjectIsReceiver(), receiver);
  Node* branch =
      graph()->NewNode(common()->Branch(BranchHint::kTrue), check, control);

  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
  Node* etrue = effect;
  Node* rtrue = receiver;

  Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
  Node* efalse = effect;
  Node* rfalse;
  {
    // Convert {receiver} using the ToObjectStub.
    Callable callable = CodeFactory::ToObject(isolate());
    CallDescriptor const* const desc = Linkage::GetStubCallDescriptor(
        isolate(), graph()->zone(), callable.descriptor(), 0,
        CallDescriptor::kNeedsFrameState, node->op()->properties());
    rfalse = efalse = graph()->NewNode(
        common()->Call(desc), jsgraph()->HeapConstant(callable.code()),
        receiver, context, frame_state, efalse, if_false);
    if_false = graph()->NewNode(common()->IfSuccess(), rfalse);
  }

  control = graph()->NewNode(common()->Merge(2), if_true, if_false);
  effect = graph()->NewNode(common()->EffectPhi(2), etrue, efalse, control);

  // Morph the {node} into an appropriate Phi.
  ReplaceWithValue(node, node, effect, control);
  node->ReplaceInput(0, rtrue);
  node->ReplaceInput(1, rfalse);
  node->ReplaceInput(2, control);
  node->TrimInputCount(3);
  NodeProperties::ChangeOp(node,
                           common()->Phi(MachineRepresentation::kTagged, 2));
  return Changed(node);
}

Reduction JSTypedLowering::ReduceJSLoadNamed(Node* node) {
  DCHECK_EQ(IrOpcode::kJSLoadNamed, node->opcode());
  Node* receiver = NodeProperties::GetValueInput(node, 0);
  Type* receiver_type = NodeProperties::GetType(receiver);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  Handle<Name> name = NamedAccessOf(node->op()).name();
  // Optimize "length" property of strings.
  if (name.is_identical_to(factory()->length_string()) &&
      receiver_type->Is(Type::String())) {
    Node* value = effect = graph()->NewNode(
        simplified()->LoadField(AccessBuilder::ForStringLength()), receiver,
        effect, control);
    ReplaceWithValue(node, value, effect);
    return Replace(value);
  }
  return NoChange();
}

Reduction JSTypedLowering::ReduceJSLoadProperty(Node* node) {
  Node* key = NodeProperties::GetValueInput(node, 1);
  Node* base = NodeProperties::GetValueInput(node, 0);
  Type* key_type = NodeProperties::GetType(key);
  HeapObjectMatcher mbase(base);
  if (mbase.HasValue() && mbase.Value()->IsJSTypedArray()) {
    Handle<JSTypedArray> const array =
        Handle<JSTypedArray>::cast(mbase.Value());
    if (!array->GetBuffer()->was_neutered()) {
      array->GetBuffer()->set_is_neuterable(false);
      BufferAccess const access(array->type());
      size_t const k =
          ElementSizeLog2Of(access.machine_type().representation());
      double const byte_length = array->byte_length()->Number();
      CHECK_LT(k, arraysize(shifted_int32_ranges_));
      if (key_type->Is(shifted_int32_ranges_[k]) && byte_length <= kMaxInt) {
        // JSLoadProperty(typed-array, int32)
        Handle<FixedTypedArrayBase> elements =
            Handle<FixedTypedArrayBase>::cast(handle(array->elements()));
        Node* buffer = jsgraph()->PointerConstant(elements->external_pointer());
        Node* length = jsgraph()->Constant(byte_length);
        Node* effect = NodeProperties::GetEffectInput(node);
        Node* control = NodeProperties::GetControlInput(node);
        // Check if we can avoid the bounds check.
        if (key_type->Min() >= 0 && key_type->Max() < array->length_value()) {
          Node* load = graph()->NewNode(
              simplified()->LoadElement(
                  AccessBuilder::ForTypedArrayElement(array->type(), true)),
              buffer, key, effect, control);
          ReplaceWithValue(node, load, load);
          return Replace(load);
        }
        // Compute byte offset.
        Node* offset =
            (k == 0) ? key : graph()->NewNode(
                                 simplified()->NumberShiftLeft(), key,
                                 jsgraph()->Constant(static_cast<double>(k)));
        Node* load = graph()->NewNode(simplified()->LoadBuffer(access), buffer,
                                      offset, length, effect, control);
        ReplaceWithValue(node, load, load);
        return Replace(load);
      }
    }
  }
  return NoChange();
}

Reduction JSTypedLowering::ReduceJSStoreProperty(Node* node) {
  Node* key = NodeProperties::GetValueInput(node, 1);
  Node* base = NodeProperties::GetValueInput(node, 0);
  Node* value = NodeProperties::GetValueInput(node, 2);
  Type* key_type = NodeProperties::GetType(key);
  Type* value_type = NodeProperties::GetType(value);
  HeapObjectMatcher mbase(base);
  if (mbase.HasValue() && mbase.Value()->IsJSTypedArray()) {
    Handle<JSTypedArray> const array =
        Handle<JSTypedArray>::cast(mbase.Value());
    if (!array->GetBuffer()->was_neutered()) {
      array->GetBuffer()->set_is_neuterable(false);
      BufferAccess const access(array->type());
      size_t const k =
          ElementSizeLog2Of(access.machine_type().representation());
      double const byte_length = array->byte_length()->Number();
      CHECK_LT(k, arraysize(shifted_int32_ranges_));
      if (access.external_array_type() != kExternalUint8ClampedArray &&
          key_type->Is(shifted_int32_ranges_[k]) && byte_length <= kMaxInt) {
        // JSLoadProperty(typed-array, int32)
        Handle<FixedTypedArrayBase> elements =
            Handle<FixedTypedArrayBase>::cast(handle(array->elements()));
        Node* buffer = jsgraph()->PointerConstant(elements->external_pointer());
        Node* length = jsgraph()->Constant(byte_length);
        Node* context = NodeProperties::GetContextInput(node);
        Node* effect = NodeProperties::GetEffectInput(node);
        Node* control = NodeProperties::GetControlInput(node);
        // Convert to a number first.
        if (!value_type->Is(Type::Number())) {
          Reduction number_reduction = ReduceJSToNumberInput(value);
          if (number_reduction.Changed()) {
            value = number_reduction.replacement();
          } else {
            Node* frame_state_for_to_number =
                NodeProperties::FindFrameStateBefore(node);
            value = effect =
                graph()->NewNode(javascript()->ToNumber(), value, context,
                                 frame_state_for_to_number, effect, control);
            control = graph()->NewNode(common()->IfSuccess(), value);
          }
        }
        // Check if we can avoid the bounds check.
        if (key_type->Min() >= 0 && key_type->Max() < array->length_value()) {
          RelaxControls(node);
          node->ReplaceInput(0, buffer);
          DCHECK_EQ(key, node->InputAt(1));
          node->ReplaceInput(2, value);
          node->ReplaceInput(3, effect);
          node->ReplaceInput(4, control);
          node->TrimInputCount(5);
          NodeProperties::ChangeOp(
              node,
              simplified()->StoreElement(
                  AccessBuilder::ForTypedArrayElement(array->type(), true)));
          return Changed(node);
        }
        // Compute byte offset.
        Node* offset =
            (k == 0) ? key : graph()->NewNode(
                                 simplified()->NumberShiftLeft(), key,
                                 jsgraph()->Constant(static_cast<double>(k)));
        // Turn into a StoreBuffer operation.
        RelaxControls(node);
        node->ReplaceInput(0, buffer);
        node->ReplaceInput(1, offset);
        node->ReplaceInput(2, length);
        node->ReplaceInput(3, value);
        node->ReplaceInput(4, effect);
        node->ReplaceInput(5, control);
        node->TrimInputCount(6);
        NodeProperties::ChangeOp(node, simplified()->StoreBuffer(access));
        return Changed(node);
      }
    }
  }
  return NoChange();
}

Reduction JSTypedLowering::ReduceJSInstanceOf(Node* node) {
  DCHECK_EQ(IrOpcode::kJSInstanceOf, node->opcode());
  Node* const context = NodeProperties::GetContextInput(node);
  Node* const frame_state = NodeProperties::GetFrameStateInput(node);

  // If deoptimization is disabled, we cannot optimize.
  if (!(flags() & kDeoptimizationEnabled)) return NoChange();

  // If we are in a try block, don't optimize since the runtime call
  // in the proxy case can throw.
  if (NodeProperties::IsExceptionalCall(node)) return NoChange();

  JSBinopReduction r(this, node);
  Node* effect = r.effect();
  Node* control = r.control();

  if (!r.right_type()->IsConstant() ||
      !r.right_type()->AsConstant()->Value()->IsJSFunction()) {
    return NoChange();
  }

  Handle<JSFunction> function =
      Handle<JSFunction>::cast(r.right_type()->AsConstant()->Value());
  Handle<SharedFunctionInfo> shared(function->shared(), isolate());

  // Make sure the prototype of {function} is the %FunctionPrototype%, and it
  // already has a meaningful initial map (i.e. we constructed at least one
  // instance using the constructor {function}).
  if (function->map()->prototype() != function->native_context()->closure() ||
      function->map()->has_non_instance_prototype() ||
      !function->has_initial_map()) {
    return NoChange();
  }

  // We can only use the fast case if @@hasInstance was not used so far.
  if (!isolate()->IsHasInstanceLookupChainIntact()) return NoChange();
  dependencies()->AssumePropertyCell(factory()->has_instance_protector());

  Handle<Map> initial_map(function->initial_map(), isolate());
  dependencies()->AssumeInitialMapCantChange(initial_map);
  Node* prototype =
      jsgraph()->Constant(handle(initial_map->prototype(), isolate()));

  // If the left hand side is an object, no smi check is needed.
  Node* is_smi = graph()->NewNode(simplified()->ObjectIsSmi(), r.left());
  Node* branch_is_smi =
      graph()->NewNode(common()->Branch(BranchHint::kFalse), is_smi, control);
  Node* if_is_smi = graph()->NewNode(common()->IfTrue(), branch_is_smi);
  Node* e_is_smi = effect;
  control = graph()->NewNode(common()->IfFalse(), branch_is_smi);

  Node* object_map = effect =
      graph()->NewNode(simplified()->LoadField(AccessBuilder::ForMap()),
                       r.left(), effect, control);

  // Loop through the {object}s prototype chain looking for the {prototype}.
  Node* loop = control = graph()->NewNode(common()->Loop(2), control, control);

  Node* loop_effect = effect =
      graph()->NewNode(common()->EffectPhi(2), effect, effect, loop);

  Node* loop_object_map =
      graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                       object_map, r.left(), loop);

  // Check if the lhs needs access checks.
  Node* map_bit_field = effect =
      graph()->NewNode(simplified()->LoadField(AccessBuilder::ForMapBitField()),
                       loop_object_map, loop_effect, control);
  int is_access_check_needed_bit = 1 << Map::kIsAccessCheckNeeded;
  Node* is_access_check_needed_num =
      graph()->NewNode(simplified()->NumberBitwiseAnd(), map_bit_field,
                       jsgraph()->Constant(is_access_check_needed_bit));
  Node* is_access_check_needed =
      graph()->NewNode(simplified()->NumberEqual(), is_access_check_needed_num,
                       jsgraph()->Constant(is_access_check_needed_bit));

  Node* branch_is_access_check_needed = graph()->NewNode(
      common()->Branch(BranchHint::kFalse), is_access_check_needed, control);
  Node* if_is_access_check_needed =
      graph()->NewNode(common()->IfTrue(), branch_is_access_check_needed);
  Node* e_is_access_check_needed = effect;

  control =
      graph()->NewNode(common()->IfFalse(), branch_is_access_check_needed);

  // Check if the lhs is a proxy.
  Node* map_instance_type = effect = graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForMapInstanceType()),
      loop_object_map, loop_effect, control);
  Node* is_proxy =
      graph()->NewNode(simplified()->NumberEqual(), map_instance_type,
                       jsgraph()->Constant(JS_PROXY_TYPE));
  Node* branch_is_proxy =
      graph()->NewNode(common()->Branch(BranchHint::kFalse), is_proxy, control);
  Node* if_is_proxy = graph()->NewNode(common()->IfTrue(), branch_is_proxy);
  Node* e_is_proxy = effect;

  control = graph()->NewNode(common()->Merge(2), if_is_access_check_needed,
                             if_is_proxy);
  effect = graph()->NewNode(common()->EffectPhi(2), e_is_access_check_needed,
                            e_is_proxy, control);

  // If we need an access check or the object is a Proxy, make a runtime call
  // to finish the lowering.
  Node* runtimecall = graph()->NewNode(
      javascript()->CallRuntime(Runtime::kHasInPrototypeChain), r.left(),
      prototype, context, frame_state, effect, control);

  Node* runtimecall_control =
      graph()->NewNode(common()->IfSuccess(), runtimecall);

  control = graph()->NewNode(common()->IfFalse(), branch_is_proxy);

  Node* object_prototype = effect = graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForMapPrototype()),
      loop_object_map, loop_effect, control);

  // If not, check if object prototype is the null prototype.
  Node* null_proto =
      graph()->NewNode(simplified()->ReferenceEqual(), object_prototype,
                       jsgraph()->NullConstant());
  Node* branch_null_proto = graph()->NewNode(
      common()->Branch(BranchHint::kFalse), null_proto, control);
  Node* if_null_proto = graph()->NewNode(common()->IfTrue(), branch_null_proto);
  Node* e_null_proto = effect;

  control = graph()->NewNode(common()->IfFalse(), branch_null_proto);

  // Check if object prototype is equal to function prototype.
  Node* eq_proto = graph()->NewNode(simplified()->ReferenceEqual(),
                                    object_prototype, prototype);
  Node* branch_eq_proto =
      graph()->NewNode(common()->Branch(BranchHint::kFalse), eq_proto, control);
  Node* if_eq_proto = graph()->NewNode(common()->IfTrue(), branch_eq_proto);
  Node* e_eq_proto = effect;

  control = graph()->NewNode(common()->IfFalse(), branch_eq_proto);

  Node* load_object_map = effect =
      graph()->NewNode(simplified()->LoadField(AccessBuilder::ForMap()),
                       object_prototype, effect, control);
  // Close the loop.
  loop_effect->ReplaceInput(1, effect);
  loop_object_map->ReplaceInput(1, load_object_map);
  loop->ReplaceInput(1, control);

  control = graph()->NewNode(common()->Merge(3), runtimecall_control,
                             if_eq_proto, if_null_proto);
  effect = graph()->NewNode(common()->EffectPhi(3), runtimecall, e_eq_proto,
                            e_null_proto, control);

  Node* result = graph()->NewNode(
      common()->Phi(MachineRepresentation::kTagged, 3), runtimecall,
      jsgraph()->TrueConstant(), jsgraph()->FalseConstant(), control);

  control = graph()->NewNode(common()->Merge(2), if_is_smi, control);
  effect = graph()->NewNode(common()->EffectPhi(2), e_is_smi, effect, control);
  result = graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                            jsgraph()->FalseConstant(), result, control);

  ReplaceWithValue(node, result, effect, control);
  return Changed(result);
}

Reduction JSTypedLowering::ReduceJSLoadContext(Node* node) {
  DCHECK_EQ(IrOpcode::kJSLoadContext, node->opcode());
  ContextAccess const& access = ContextAccessOf(node->op());
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = graph()->start();
  for (size_t i = 0; i < access.depth(); ++i) {
    Node* previous = effect = graph()->NewNode(
        simplified()->LoadField(
            AccessBuilder::ForContextSlot(Context::PREVIOUS_INDEX)),
        NodeProperties::GetValueInput(node, 0), effect, control);
    node->ReplaceInput(0, previous);
  }
  node->ReplaceInput(1, effect);
  node->ReplaceInput(2, control);
  NodeProperties::ChangeOp(
      node,
      simplified()->LoadField(AccessBuilder::ForContextSlot(access.index())));
  return Changed(node);
}

Reduction JSTypedLowering::ReduceJSStoreContext(Node* node) {
  DCHECK_EQ(IrOpcode::kJSStoreContext, node->opcode());
  ContextAccess const& access = ContextAccessOf(node->op());
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = graph()->start();
  for (size_t i = 0; i < access.depth(); ++i) {
    Node* previous = effect = graph()->NewNode(
        simplified()->LoadField(
            AccessBuilder::ForContextSlot(Context::PREVIOUS_INDEX)),
        NodeProperties::GetValueInput(node, 0), effect, control);
    node->ReplaceInput(0, previous);
  }
  node->RemoveInput(2);
  node->ReplaceInput(2, effect);
  NodeProperties::ChangeOp(
      node,
      simplified()->StoreField(AccessBuilder::ForContextSlot(access.index())));
  return Changed(node);
}

Reduction JSTypedLowering::ReduceJSConvertReceiver(Node* node) {
  DCHECK_EQ(IrOpcode::kJSConvertReceiver, node->opcode());
  ConvertReceiverMode mode = ConvertReceiverModeOf(node->op());
  Node* receiver = NodeProperties::GetValueInput(node, 0);
  Type* receiver_type = NodeProperties::GetType(receiver);
  Node* context = NodeProperties::GetContextInput(node);
  Type* context_type = NodeProperties::GetType(context);
  Node* frame_state = NodeProperties::GetFrameStateInput(node);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  // Check if {receiver} is known to be a receiver.
  if (receiver_type->Is(Type::Receiver())) {
    ReplaceWithValue(node, receiver, effect, control);
    return Replace(receiver);
  }

  // If the {receiver} is known to be null or undefined, we can just replace it
  // with the global proxy unconditionally.
  if (receiver_type->Is(Type::NullOrUndefined()) ||
      mode == ConvertReceiverMode::kNullOrUndefined) {
    if (context_type->IsConstant()) {
      Handle<JSObject> global_proxy(
          Handle<Context>::cast(context_type->AsConstant()->Value())
              ->global_proxy(),
          isolate());
      receiver = jsgraph()->Constant(global_proxy);
    } else {
      Node* native_context = effect = graph()->NewNode(
          javascript()->LoadContext(0, Context::NATIVE_CONTEXT_INDEX, true),
          context, context, effect);
      receiver = effect = graph()->NewNode(
          javascript()->LoadContext(0, Context::GLOBAL_PROXY_INDEX, true),
          native_context, native_context, effect);
    }
    ReplaceWithValue(node, receiver, effect, control);
    return Replace(receiver);
  }

  // If {receiver} cannot be null or undefined we can skip a few checks.
  if (!receiver_type->Maybe(Type::NullOrUndefined()) ||
      mode == ConvertReceiverMode::kNotNullOrUndefined) {
    Node* check = graph()->NewNode(simplified()->ObjectIsReceiver(), receiver);
    Node* branch =
        graph()->NewNode(common()->Branch(BranchHint::kTrue), check, control);

    Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
    Node* etrue = effect;
    Node* rtrue = receiver;

    Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
    Node* efalse = effect;
    Node* rfalse;
    {
      // Convert {receiver} using the ToObjectStub.
      Callable callable = CodeFactory::ToObject(isolate());
      CallDescriptor const* const desc = Linkage::GetStubCallDescriptor(
          isolate(), graph()->zone(), callable.descriptor(), 0,
          CallDescriptor::kNeedsFrameState, node->op()->properties());
      rfalse = efalse = graph()->NewNode(
          common()->Call(desc), jsgraph()->HeapConstant(callable.code()),
          receiver, context, frame_state, efalse);
    }

    control = graph()->NewNode(common()->Merge(2), if_true, if_false);
    effect = graph()->NewNode(common()->EffectPhi(2), etrue, efalse, control);

    // Morph the {node} into an appropriate Phi.
    ReplaceWithValue(node, node, effect, control);
    node->ReplaceInput(0, rtrue);
    node->ReplaceInput(1, rfalse);
    node->ReplaceInput(2, control);
    node->TrimInputCount(3);
    NodeProperties::ChangeOp(node,
                             common()->Phi(MachineRepresentation::kTagged, 2));
    return Changed(node);
  }

  // Check if {receiver} is already a JSReceiver.
  Node* check0 = graph()->NewNode(simplified()->ObjectIsReceiver(), receiver);
  Node* branch0 =
      graph()->NewNode(common()->Branch(BranchHint::kTrue), check0, control);
  Node* if_true0 = graph()->NewNode(common()->IfTrue(), branch0);
  Node* if_false0 = graph()->NewNode(common()->IfFalse(), branch0);

  // Check {receiver} for undefined.
  Node* check1 = graph()->NewNode(simplified()->ReferenceEqual(), receiver,
                                  jsgraph()->UndefinedConstant());
  Node* branch1 =
      graph()->NewNode(common()->Branch(BranchHint::kFalse), check1, if_false0);
  Node* if_true1 = graph()->NewNode(common()->IfTrue(), branch1);
  Node* if_false1 = graph()->NewNode(common()->IfFalse(), branch1);

  // Check {receiver} for null.
  Node* check2 = graph()->NewNode(simplified()->ReferenceEqual(), receiver,
                                  jsgraph()->NullConstant());
  Node* branch2 =
      graph()->NewNode(common()->Branch(BranchHint::kFalse), check2, if_false1);
  Node* if_true2 = graph()->NewNode(common()->IfTrue(), branch2);
  Node* if_false2 = graph()->NewNode(common()->IfFalse(), branch2);

  // We just use {receiver} directly.
  Node* if_noop = if_true0;
  Node* enoop = effect;
  Node* rnoop = receiver;

  // Convert {receiver} using ToObject.
  Node* if_convert = if_false2;
  Node* econvert = effect;
  Node* rconvert;
  {
    // Convert {receiver} using the ToObjectStub.
    Callable callable = CodeFactory::ToObject(isolate());
    CallDescriptor const* const desc = Linkage::GetStubCallDescriptor(
        isolate(), graph()->zone(), callable.descriptor(), 0,
        CallDescriptor::kNeedsFrameState, node->op()->properties());
    rconvert = econvert = graph()->NewNode(
        common()->Call(desc), jsgraph()->HeapConstant(callable.code()),
        receiver, context, frame_state, econvert);
  }

  // Replace {receiver} with global proxy of {context}.
  Node* if_global = graph()->NewNode(common()->Merge(2), if_true1, if_true2);
  Node* eglobal = effect;
  Node* rglobal;
  {
    if (context_type->IsConstant()) {
      Handle<JSObject> global_proxy(
          Handle<Context>::cast(context_type->AsConstant()->Value())
              ->global_proxy(),
          isolate());
      rglobal = jsgraph()->Constant(global_proxy);
    } else {
      Node* native_context = eglobal = graph()->NewNode(
          javascript()->LoadContext(0, Context::NATIVE_CONTEXT_INDEX, true),
          context, context, eglobal);
      rglobal = eglobal = graph()->NewNode(
          javascript()->LoadContext(0, Context::GLOBAL_PROXY_INDEX, true),
          native_context, native_context, eglobal);
    }
  }

  control =
      graph()->NewNode(common()->Merge(3), if_noop, if_convert, if_global);
  effect = graph()->NewNode(common()->EffectPhi(3), enoop, econvert, eglobal,
                            control);
  // Morph the {node} into an appropriate Phi.
  ReplaceWithValue(node, node, effect, control);
  node->ReplaceInput(0, rnoop);
  node->ReplaceInput(1, rconvert);
  node->ReplaceInput(2, rglobal);
  node->ReplaceInput(3, control);
  node->TrimInputCount(4);
  NodeProperties::ChangeOp(node,
                           common()->Phi(MachineRepresentation::kTagged, 3));
  return Changed(node);
}

namespace {

void ReduceBuiltin(Isolate* isolate, JSGraph* jsgraph, Node* node,
                   int builtin_index, int arity, CallDescriptor::Flags flags) {
  // Patch {node} to a direct CEntryStub call.
  //
  // ----------- A r g u m e n t s -----------
  // -- 0: CEntryStub
  // --- Stack args ---
  // -- 1: receiver
  // -- [2, 2 + n[: the n actual arguments passed to the builtin
  // -- 2 + n: argc, including the receiver and implicit args (Smi)
  // -- 2 + n + 1: target
  // -- 2 + n + 2: new_target
  // --- Register args ---
  // -- 2 + n + 3: the C entry point
  // -- 2 + n + 4: argc (Int32)
  // -----------------------------------

  // The logic contained here is mirrored in Builtins::Generate_Adaptor.
  // Keep these in sync.

  const bool is_construct = (node->opcode() == IrOpcode::kJSCallConstruct);

  DCHECK(Builtins::HasCppImplementation(builtin_index));

  Node* target = NodeProperties::GetValueInput(node, 0);
  Node* new_target = is_construct
                         ? NodeProperties::GetValueInput(node, arity + 1)
                         : jsgraph->UndefinedConstant();

  // API and CPP builtins are implemented in C++, and we can inline both.
  // CPP builtins create a builtin exit frame, API builtins don't.
  const bool has_builtin_exit_frame = Builtins::IsCpp(builtin_index);

  Node* stub = jsgraph->CEntryStubConstant(1, kDontSaveFPRegs, kArgvOnStack,
                                           has_builtin_exit_frame);
  node->ReplaceInput(0, stub);

  Zone* zone = jsgraph->zone();
  if (is_construct) {
    // Unify representations between construct and call nodes.
    // Remove new target and add receiver as a stack parameter.
    Node* receiver = jsgraph->UndefinedConstant();
    node->RemoveInput(arity + 1);
    node->InsertInput(zone, 1, receiver);
  }

  const int argc = arity + BuiltinArguments::kNumExtraArgsWithReceiver;
  Node* argc_node = jsgraph->Int32Constant(argc);

  node->InsertInput(zone, arity + 2, argc_node);
  node->InsertInput(zone, arity + 3, target);
  node->InsertInput(zone, arity + 4, new_target);

  Address entry = Builtins::CppEntryOf(builtin_index);
  ExternalReference entry_ref(ExternalReference(entry, isolate));
  Node* entry_node = jsgraph->ExternalConstant(entry_ref);

  node->InsertInput(zone, arity + 5, entry_node);
  node->InsertInput(zone, arity + 6, argc_node);

  static const int kReturnCount = 1;
  const char* debug_name = Builtins::name(builtin_index);
  Operator::Properties properties = node->op()->properties();
  CallDescriptor* desc = Linkage::GetCEntryStubCallDescriptor(
      zone, kReturnCount, argc, debug_name, properties, flags);

  NodeProperties::ChangeOp(node, jsgraph->common()->Call(desc));
}

}  // namespace

Reduction JSTypedLowering::ReduceJSCallConstruct(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCallConstruct, node->opcode());
  CallConstructParameters const& p = CallConstructParametersOf(node->op());
  DCHECK_LE(2u, p.arity());
  int const arity = static_cast<int>(p.arity() - 2);
  Node* target = NodeProperties::GetValueInput(node, 0);
  Type* target_type = NodeProperties::GetType(target);
  Node* new_target = NodeProperties::GetValueInput(node, arity + 1);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  // Check if {target} is a known JSFunction.
  if (target_type->IsConstant() &&
      target_type->AsConstant()->Value()->IsJSFunction()) {
    Handle<JSFunction> function =
        Handle<JSFunction>::cast(target_type->AsConstant()->Value());
    Handle<SharedFunctionInfo> shared(function->shared(), isolate());
    const int builtin_index = shared->construct_stub()->builtin_index();
    const bool is_builtin = (builtin_index != -1);

    CallDescriptor::Flags flags = CallDescriptor::kNeedsFrameState;

    if (is_builtin && Builtins::HasCppImplementation(builtin_index) &&
        (shared->internal_formal_parameter_count() == arity ||
         shared->internal_formal_parameter_count() ==
             SharedFunctionInfo::kDontAdaptArgumentsSentinel)) {
      // Patch {node} to a direct CEntryStub call.

      // Load the context from the {target}.
      Node* context = effect = graph()->NewNode(
          simplified()->LoadField(AccessBuilder::ForJSFunctionContext()),
          target, effect, control);
      NodeProperties::ReplaceContextInput(node, context);

      // Update the effect dependency for the {node}.
      NodeProperties::ReplaceEffectInput(node, effect);

      ReduceBuiltin(isolate(), jsgraph(), node, builtin_index, arity, flags);
    } else {
      // Patch {node} to an indirect call via the {function}s construct stub.
      Callable callable(handle(shared->construct_stub(), isolate()),
                        ConstructStubDescriptor(isolate()));
      node->RemoveInput(arity + 1);
      node->InsertInput(graph()->zone(), 0,
                        jsgraph()->HeapConstant(callable.code()));
      node->InsertInput(graph()->zone(), 2, new_target);
      node->InsertInput(graph()->zone(), 3, jsgraph()->Int32Constant(arity));
      node->InsertInput(graph()->zone(), 4, jsgraph()->UndefinedConstant());
      node->InsertInput(graph()->zone(), 5, jsgraph()->UndefinedConstant());
      NodeProperties::ChangeOp(
          node, common()->Call(Linkage::GetStubCallDescriptor(
                    isolate(), graph()->zone(), callable.descriptor(),
                    1 + arity, flags)));
    }
    return Changed(node);
  }

  // Check if {target} is a JSFunction.
  if (target_type->Is(Type::Function())) {
    // Patch {node} to an indirect call via the ConstructFunction builtin.
    Callable callable = CodeFactory::ConstructFunction(isolate());
    node->RemoveInput(arity + 1);
    node->InsertInput(graph()->zone(), 0,
                      jsgraph()->HeapConstant(callable.code()));
    node->InsertInput(graph()->zone(), 2, new_target);
    node->InsertInput(graph()->zone(), 3, jsgraph()->Int32Constant(arity));
    node->InsertInput(graph()->zone(), 4, jsgraph()->UndefinedConstant());
    NodeProperties::ChangeOp(
        node, common()->Call(Linkage::GetStubCallDescriptor(
                  isolate(), graph()->zone(), callable.descriptor(), 1 + arity,
                  CallDescriptor::kNeedsFrameState)));
    return Changed(node);
  }

  return NoChange();
}


Reduction JSTypedLowering::ReduceJSCallFunction(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCallFunction, node->opcode());
  CallFunctionParameters const& p = CallFunctionParametersOf(node->op());
  int const arity = static_cast<int>(p.arity() - 2);
  ConvertReceiverMode convert_mode = p.convert_mode();
  Node* target = NodeProperties::GetValueInput(node, 0);
  Type* target_type = NodeProperties::GetType(target);
  Node* receiver = NodeProperties::GetValueInput(node, 1);
  Type* receiver_type = NodeProperties::GetType(receiver);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  Node* frame_state = NodeProperties::FindFrameStateBefore(node);

  // Try to infer receiver {convert_mode} from {receiver} type.
  if (receiver_type->Is(Type::NullOrUndefined())) {
    convert_mode = ConvertReceiverMode::kNullOrUndefined;
  } else if (!receiver_type->Maybe(Type::NullOrUndefined())) {
    convert_mode = ConvertReceiverMode::kNotNullOrUndefined;
  }

  // Check if {target} is a known JSFunction.
  if (target_type->IsConstant() &&
      target_type->AsConstant()->Value()->IsJSFunction()) {
    Handle<JSFunction> function =
        Handle<JSFunction>::cast(target_type->AsConstant()->Value());
    Handle<SharedFunctionInfo> shared(function->shared(), isolate());
    const int builtin_index = shared->code()->builtin_index();
    const bool is_builtin = (builtin_index != -1);

    // Class constructors are callable, but [[Call]] will raise an exception.
    // See ES6 section 9.2.1 [[Call]] ( thisArgument, argumentsList ).
    if (IsClassConstructor(shared->kind())) return NoChange();

    // Load the context from the {target}.
    Node* context = effect = graph()->NewNode(
        simplified()->LoadField(AccessBuilder::ForJSFunctionContext()), target,
        effect, control);
    NodeProperties::ReplaceContextInput(node, context);

    // Check if we need to convert the {receiver}.
    if (is_sloppy(shared->language_mode()) && !shared->native() &&
        !receiver_type->Is(Type::Receiver())) {
      receiver = effect =
          graph()->NewNode(javascript()->ConvertReceiver(convert_mode),
                           receiver, context, frame_state, effect, control);
      NodeProperties::ReplaceValueInput(node, receiver, 1);
    }

    // Update the effect dependency for the {node}.
    NodeProperties::ReplaceEffectInput(node, effect);

    // Compute flags for the call.
    CallDescriptor::Flags flags = CallDescriptor::kNeedsFrameState;
    if (p.tail_call_mode() == TailCallMode::kAllow) {
      flags |= CallDescriptor::kSupportsTailCalls;
    }

    Node* new_target = jsgraph()->UndefinedConstant();
    Node* argument_count = jsgraph()->Int32Constant(arity);
    if (is_builtin && Builtins::HasCppImplementation(builtin_index) &&
        (shared->internal_formal_parameter_count() == arity ||
         shared->internal_formal_parameter_count() ==
             SharedFunctionInfo::kDontAdaptArgumentsSentinel)) {
      // Patch {node} to a direct CEntryStub call.
      ReduceBuiltin(isolate(), jsgraph(), node, builtin_index, arity, flags);
    } else if (shared->internal_formal_parameter_count() == arity ||
               shared->internal_formal_parameter_count() ==
                   SharedFunctionInfo::kDontAdaptArgumentsSentinel) {
      // Patch {node} to a direct call.
      node->InsertInput(graph()->zone(), arity + 2, new_target);
      node->InsertInput(graph()->zone(), arity + 3, argument_count);
      NodeProperties::ChangeOp(node,
                               common()->Call(Linkage::GetJSCallDescriptor(
                                   graph()->zone(), false, 1 + arity, flags)));
    } else {
      // Patch {node} to an indirect call via the ArgumentsAdaptorTrampoline.
      Callable callable = CodeFactory::ArgumentAdaptor(isolate());
      node->InsertInput(graph()->zone(), 0,
                        jsgraph()->HeapConstant(callable.code()));
      node->InsertInput(graph()->zone(), 2, new_target);
      node->InsertInput(graph()->zone(), 3, argument_count);
      node->InsertInput(
          graph()->zone(), 4,
          jsgraph()->Int32Constant(shared->internal_formal_parameter_count()));
      NodeProperties::ChangeOp(
          node, common()->Call(Linkage::GetStubCallDescriptor(
                    isolate(), graph()->zone(), callable.descriptor(),
                    1 + arity, flags)));
    }
    return Changed(node);
  }

  // Check if {target} is a JSFunction.
  if (target_type->Is(Type::Function())) {
    // Compute flags for the call.
    CallDescriptor::Flags flags = CallDescriptor::kNeedsFrameState;
    if (p.tail_call_mode() == TailCallMode::kAllow) {
      flags |= CallDescriptor::kSupportsTailCalls;
    }

    // Patch {node} to an indirect call via the CallFunction builtin.
    Callable callable = CodeFactory::CallFunction(isolate(), convert_mode);
    node->InsertInput(graph()->zone(), 0,
                      jsgraph()->HeapConstant(callable.code()));
    node->InsertInput(graph()->zone(), 2, jsgraph()->Int32Constant(arity));
    NodeProperties::ChangeOp(
        node, common()->Call(Linkage::GetStubCallDescriptor(
                  isolate(), graph()->zone(), callable.descriptor(), 1 + arity,
                  flags)));
    return Changed(node);
  }

  // Maybe we did at least learn something about the {receiver}.
  if (p.convert_mode() != convert_mode) {
    NodeProperties::ChangeOp(
        node, javascript()->CallFunction(p.arity(), p.feedback(), convert_mode,
                                         p.tail_call_mode()));
    return Changed(node);
  }

  return NoChange();
}


Reduction JSTypedLowering::ReduceJSForInDone(Node* node) {
  DCHECK_EQ(IrOpcode::kJSForInDone, node->opcode());
  node->TrimInputCount(2);
  NodeProperties::ChangeOp(node, machine()->Word32Equal());
  return Changed(node);
}


Reduction JSTypedLowering::ReduceJSForInNext(Node* node) {
  DCHECK_EQ(IrOpcode::kJSForInNext, node->opcode());
  Node* receiver = NodeProperties::GetValueInput(node, 0);
  Node* cache_array = NodeProperties::GetValueInput(node, 1);
  Node* cache_type = NodeProperties::GetValueInput(node, 2);
  Node* index = NodeProperties::GetValueInput(node, 3);
  Node* context = NodeProperties::GetContextInput(node);
  Node* frame_state = NodeProperties::GetFrameStateInput(node);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  // Load the next {key} from the {cache_array}.
  Node* key = effect = graph()->NewNode(
      simplified()->LoadElement(AccessBuilder::ForFixedArrayElement()),
      cache_array, index, effect, control);

  // Load the map of the {receiver}.
  Node* receiver_map = effect =
      graph()->NewNode(simplified()->LoadField(AccessBuilder::ForMap()),
                       receiver, effect, control);

  // Check if the expected map still matches that of the {receiver}.
  Node* check0 = graph()->NewNode(simplified()->ReferenceEqual(), receiver_map,
                                  cache_type);
  Node* branch0 =
      graph()->NewNode(common()->Branch(BranchHint::kTrue), check0, control);

  Node* if_true0 = graph()->NewNode(common()->IfTrue(), branch0);
  Node* etrue0;
  Node* vtrue0;
  {
    // Don't need filtering since expected map still matches that of the
    // {receiver}.
    etrue0 = effect;
    vtrue0 = key;
  }

  Node* if_false0 = graph()->NewNode(common()->IfFalse(), branch0);
  Node* efalse0;
  Node* vfalse0;
  {
    // Filter the {key} to check if it's still a valid property of the
    // {receiver} (does the ToName conversion implicitly).
    Callable const callable = CodeFactory::ForInFilter(isolate());
    CallDescriptor const* const desc = Linkage::GetStubCallDescriptor(
        isolate(), graph()->zone(), callable.descriptor(), 0,
        CallDescriptor::kNeedsFrameState);
    vfalse0 = efalse0 = graph()->NewNode(
        common()->Call(desc), jsgraph()->HeapConstant(callable.code()), key,
        receiver, context, frame_state, effect, if_false0);
    if_false0 = graph()->NewNode(common()->IfSuccess(), vfalse0);
  }

  control = graph()->NewNode(common()->Merge(2), if_true0, if_false0);
  effect = graph()->NewNode(common()->EffectPhi(2), etrue0, efalse0, control);
  ReplaceWithValue(node, node, effect, control);
  node->ReplaceInput(0, vtrue0);
  node->ReplaceInput(1, vfalse0);
  node->ReplaceInput(2, control);
  node->TrimInputCount(3);
  NodeProperties::ChangeOp(node,
                           common()->Phi(MachineRepresentation::kTagged, 2));
  return Changed(node);
}


Reduction JSTypedLowering::ReduceJSForInStep(Node* node) {
  DCHECK_EQ(IrOpcode::kJSForInStep, node->opcode());
  node->ReplaceInput(1, jsgraph()->Int32Constant(1));
  NodeProperties::ChangeOp(node, machine()->Int32Add());
  return Changed(node);
}

Reduction JSTypedLowering::ReduceJSGeneratorStore(Node* node) {
  DCHECK_EQ(IrOpcode::kJSGeneratorStore, node->opcode());
  Node* generator = NodeProperties::GetValueInput(node, 0);
  Node* continuation = NodeProperties::GetValueInput(node, 1);
  Node* offset = NodeProperties::GetValueInput(node, 2);
  Node* context = NodeProperties::GetContextInput(node);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  int register_count = OpParameter<int>(node);

  FieldAccess array_field = AccessBuilder::ForJSGeneratorObjectOperandStack();
  FieldAccess context_field = AccessBuilder::ForJSGeneratorObjectContext();
  FieldAccess continuation_field =
      AccessBuilder::ForJSGeneratorObjectContinuation();
  FieldAccess input_or_debug_pos_field =
      AccessBuilder::ForJSGeneratorObjectInputOrDebugPos();

  Node* array = effect = graph()->NewNode(simplified()->LoadField(array_field),
                                          generator, effect, control);

  for (int i = 0; i < register_count; ++i) {
    Node* value = NodeProperties::GetValueInput(node, 3 + i);
    effect = graph()->NewNode(
        simplified()->StoreField(AccessBuilder::ForFixedArraySlot(i)), array,
        value, effect, control);
  }

  effect = graph()->NewNode(simplified()->StoreField(context_field), generator,
                            context, effect, control);
  effect = graph()->NewNode(simplified()->StoreField(continuation_field),
                            generator, continuation, effect, control);
  effect = graph()->NewNode(simplified()->StoreField(input_or_debug_pos_field),
                            generator, offset, effect, control);

  ReplaceWithValue(node, effect, effect, control);
  return Changed(effect);
}

Reduction JSTypedLowering::ReduceJSGeneratorRestoreContinuation(Node* node) {
  DCHECK_EQ(IrOpcode::kJSGeneratorRestoreContinuation, node->opcode());
  Node* generator = NodeProperties::GetValueInput(node, 0);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  FieldAccess continuation_field =
      AccessBuilder::ForJSGeneratorObjectContinuation();

  Node* continuation = effect = graph()->NewNode(
      simplified()->LoadField(continuation_field), generator, effect, control);
  Node* executing = jsgraph()->Constant(JSGeneratorObject::kGeneratorExecuting);
  effect = graph()->NewNode(simplified()->StoreField(continuation_field),
                            generator, executing, effect, control);

  ReplaceWithValue(node, continuation, effect, control);
  return Changed(continuation);
}

Reduction JSTypedLowering::ReduceJSGeneratorRestoreRegister(Node* node) {
  DCHECK_EQ(IrOpcode::kJSGeneratorRestoreRegister, node->opcode());
  Node* generator = NodeProperties::GetValueInput(node, 0);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  int index = OpParameter<int>(node);

  FieldAccess array_field = AccessBuilder::ForJSGeneratorObjectOperandStack();
  FieldAccess element_field = AccessBuilder::ForFixedArraySlot(index);

  Node* array = effect = graph()->NewNode(simplified()->LoadField(array_field),
                                          generator, effect, control);
  Node* element = effect = graph()->NewNode(
      simplified()->LoadField(element_field), array, effect, control);
  Node* stale = jsgraph()->StaleRegisterConstant();
  effect = graph()->NewNode(simplified()->StoreField(element_field), array,
                            stale, effect, control);

  ReplaceWithValue(node, element, effect, control);
  return Changed(element);
}

Reduction JSTypedLowering::ReduceSelect(Node* node) {
  DCHECK_EQ(IrOpcode::kSelect, node->opcode());
  Node* const condition = NodeProperties::GetValueInput(node, 0);
  Type* const condition_type = NodeProperties::GetType(condition);
  Node* const vtrue = NodeProperties::GetValueInput(node, 1);
  Type* const vtrue_type = NodeProperties::GetType(vtrue);
  Node* const vfalse = NodeProperties::GetValueInput(node, 2);
  Type* const vfalse_type = NodeProperties::GetType(vfalse);
  if (condition_type->Is(true_type_)) {
    // Select(condition:true, vtrue, vfalse) => vtrue
    return Replace(vtrue);
  }
  if (condition_type->Is(false_type_)) {
    // Select(condition:false, vtrue, vfalse) => vfalse
    return Replace(vfalse);
  }
  if (vtrue_type->Is(true_type_) && vfalse_type->Is(false_type_)) {
    // Select(condition, vtrue:true, vfalse:false) => condition
    return Replace(condition);
  }
  if (vtrue_type->Is(false_type_) && vfalse_type->Is(true_type_)) {
    // Select(condition, vtrue:false, vfalse:true) => BooleanNot(condition)
    node->TrimInputCount(1);
    NodeProperties::ChangeOp(node, simplified()->BooleanNot());
    return Changed(node);
  }
  return NoChange();
}

namespace {

MaybeHandle<Map> GetStableMapFromObjectType(Type* object_type) {
  if (object_type->IsConstant() &&
      object_type->AsConstant()->Value()->IsHeapObject()) {
    Handle<Map> object_map(
        Handle<HeapObject>::cast(object_type->AsConstant()->Value())->map());
    if (object_map->is_stable()) return object_map;
  } else if (object_type->IsClass()) {
    Handle<Map> object_map = object_type->AsClass()->Map();
    if (object_map->is_stable()) return object_map;
  }
  return MaybeHandle<Map>();
}

}  // namespace

Reduction JSTypedLowering::ReduceCheckMaps(Node* node) {
  // TODO(bmeurer): Find a better home for this thing!
  // The CheckMaps(o, ...map...) can be eliminated if map is stable and
  // either
  //  (a) o has type Constant(object) and map == object->map, or
  //  (b) o has type Class(map),
  // and either
  //  (1) map cannot transition further, or
  //  (2) we can add a code dependency on the stability of map
  //      (to guard the Constant type information).
  Node* const object = NodeProperties::GetValueInput(node, 0);
  Type* const object_type = NodeProperties::GetType(object);
  Node* const effect = NodeProperties::GetEffectInput(node);
  Handle<Map> object_map;
  if (GetStableMapFromObjectType(object_type).ToHandle(&object_map)) {
    for (int i = 1; i < node->op()->ValueInputCount(); ++i) {
      Node* const map = NodeProperties::GetValueInput(node, i);
      Type* const map_type = NodeProperties::GetType(map);
      if (map_type->IsConstant() &&
          map_type->AsConstant()->Value().is_identical_to(object_map)) {
        if (object_map->CanTransition()) {
          DCHECK(flags() & kDeoptimizationEnabled);
          dependencies()->AssumeMapStable(object_map);
        }
        return Replace(effect);
      }
    }
  }
  return NoChange();
}

Reduction JSTypedLowering::ReduceCheckString(Node* node) {
  // TODO(bmeurer): Find a better home for this thing!
  Node* const input = NodeProperties::GetValueInput(node, 0);
  Type* const input_type = NodeProperties::GetType(input);
  if (input_type->Is(Type::String())) {
    ReplaceWithValue(node, input);
    return Replace(input);
  }
  return NoChange();
}

Reduction JSTypedLowering::ReduceLoadField(Node* node) {
  // TODO(bmeurer): Find a better home for this thing!
  Node* const object = NodeProperties::GetValueInput(node, 0);
  Type* const object_type = NodeProperties::GetType(object);
  FieldAccess const& access = FieldAccessOf(node->op());
  if (access.base_is_tagged == kTaggedBase &&
      access.offset == HeapObject::kMapOffset) {
    // We can replace LoadField[Map](o) with map if is stable and either
    //  (a) o has type Constant(object) and map == object->map, or
    //  (b) o has type Class(map),
    // and either
    //  (1) map cannot transition further, or
    //  (2) deoptimization is enabled and we can add a code dependency on the
    //      stability of map (to guard the Constant type information).
    Handle<Map> object_map;
    if (GetStableMapFromObjectType(object_type).ToHandle(&object_map)) {
      if (object_map->CanTransition()) {
        if (flags() & kDeoptimizationEnabled) {
          dependencies()->AssumeMapStable(object_map);
        } else {
          return NoChange();
        }
      }
      Node* const value = jsgraph()->HeapConstant(object_map);
      ReplaceWithValue(node, value);
      return Replace(value);
    }
  }
  return NoChange();
}

Reduction JSTypedLowering::ReduceNumberRoundop(Node* node) {
  // TODO(bmeurer): Find a better home for this thing!
  Node* const input = NodeProperties::GetValueInput(node, 0);
  Type* const input_type = NodeProperties::GetType(input);
  if (input_type->Is(type_cache_.kIntegerOrMinusZeroOrNaN)) {
    return Replace(input);
  }
  return NoChange();
}

Reduction JSTypedLowering::Reduce(Node* node) {
  // Check if the output type is a singleton.  In that case we already know the
  // result value and can simply replace the node if it's eliminable.
  if (!NodeProperties::IsConstant(node) && NodeProperties::IsTyped(node) &&
      node->op()->HasProperty(Operator::kEliminatable)) {
    // We can only constant-fold nodes here, that are known to not cause any
    // side-effect, may it be a JavaScript observable side-effect or a possible
    // eager deoptimization exit (i.e. {node} has an operator that doesn't have
    // the Operator::kNoDeopt property).
    Type* upper = NodeProperties::GetType(node);
    if (upper->IsInhabited()) {
      if (upper->IsConstant()) {
        Node* replacement = jsgraph()->Constant(upper->AsConstant()->Value());
        ReplaceWithValue(node, replacement);
        return Changed(replacement);
      } else if (upper->Is(Type::MinusZero())) {
        Node* replacement = jsgraph()->Constant(factory()->minus_zero_value());
        ReplaceWithValue(node, replacement);
        return Changed(replacement);
      } else if (upper->Is(Type::NaN())) {
        Node* replacement = jsgraph()->NaNConstant();
        ReplaceWithValue(node, replacement);
        return Changed(replacement);
      } else if (upper->Is(Type::Null())) {
        Node* replacement = jsgraph()->NullConstant();
        ReplaceWithValue(node, replacement);
        return Changed(replacement);
      } else if (upper->Is(Type::PlainNumber()) &&
                 upper->Min() == upper->Max()) {
        Node* replacement = jsgraph()->Constant(upper->Min());
        ReplaceWithValue(node, replacement);
        return Changed(replacement);
      } else if (upper->Is(Type::Undefined())) {
        Node* replacement = jsgraph()->UndefinedConstant();
        ReplaceWithValue(node, replacement);
        return Changed(replacement);
      }
    }
  }
  switch (node->opcode()) {
    case IrOpcode::kJSEqual:
      return ReduceJSEqual(node, false);
    case IrOpcode::kJSNotEqual:
      return ReduceJSEqual(node, true);
    case IrOpcode::kJSStrictEqual:
      return ReduceJSStrictEqual(node, false);
    case IrOpcode::kJSStrictNotEqual:
      return ReduceJSStrictEqual(node, true);
    case IrOpcode::kJSLessThan:         // fall through
    case IrOpcode::kJSGreaterThan:      // fall through
    case IrOpcode::kJSLessThanOrEqual:  // fall through
    case IrOpcode::kJSGreaterThanOrEqual:
      return ReduceJSComparison(node);
    case IrOpcode::kJSBitwiseOr:
    case IrOpcode::kJSBitwiseXor:
    case IrOpcode::kJSBitwiseAnd:
      return ReduceInt32Binop(node);
    case IrOpcode::kJSShiftLeft:
    case IrOpcode::kJSShiftRight:
      return ReduceUI32Shift(node, kSigned);
    case IrOpcode::kJSShiftRightLogical:
      return ReduceUI32Shift(node, kUnsigned);
    case IrOpcode::kJSAdd:
      return ReduceJSAdd(node);
    case IrOpcode::kJSSubtract:
    case IrOpcode::kJSMultiply:
    case IrOpcode::kJSDivide:
    case IrOpcode::kJSModulus:
      return ReduceNumberBinop(node);
    case IrOpcode::kJSToBoolean:
      return ReduceJSToBoolean(node);
    case IrOpcode::kJSToInteger:
      return ReduceJSToInteger(node);
    case IrOpcode::kJSToLength:
      return ReduceJSToLength(node);
    case IrOpcode::kJSToNumber:
      return ReduceJSToNumber(node);
    case IrOpcode::kJSToString:
      return ReduceJSToString(node);
    case IrOpcode::kJSToObject:
      return ReduceJSToObject(node);
    case IrOpcode::kJSLoadNamed:
      return ReduceJSLoadNamed(node);
    case IrOpcode::kJSLoadProperty:
      return ReduceJSLoadProperty(node);
    case IrOpcode::kJSStoreProperty:
      return ReduceJSStoreProperty(node);
    case IrOpcode::kJSInstanceOf:
      return ReduceJSInstanceOf(node);
    case IrOpcode::kJSLoadContext:
      return ReduceJSLoadContext(node);
    case IrOpcode::kJSStoreContext:
      return ReduceJSStoreContext(node);
    case IrOpcode::kJSConvertReceiver:
      return ReduceJSConvertReceiver(node);
    case IrOpcode::kJSCallConstruct:
      return ReduceJSCallConstruct(node);
    case IrOpcode::kJSCallFunction:
      return ReduceJSCallFunction(node);
    case IrOpcode::kJSForInDone:
      return ReduceJSForInDone(node);
    case IrOpcode::kJSForInNext:
      return ReduceJSForInNext(node);
    case IrOpcode::kJSForInStep:
      return ReduceJSForInStep(node);
    case IrOpcode::kJSGeneratorStore:
      return ReduceJSGeneratorStore(node);
    case IrOpcode::kJSGeneratorRestoreContinuation:
      return ReduceJSGeneratorRestoreContinuation(node);
    case IrOpcode::kJSGeneratorRestoreRegister:
      return ReduceJSGeneratorRestoreRegister(node);
    case IrOpcode::kSelect:
      return ReduceSelect(node);
    case IrOpcode::kCheckMaps:
      return ReduceCheckMaps(node);
    case IrOpcode::kCheckString:
      return ReduceCheckString(node);
    case IrOpcode::kNumberCeil:
    case IrOpcode::kNumberFloor:
    case IrOpcode::kNumberRound:
    case IrOpcode::kNumberTrunc:
      return ReduceNumberRoundop(node);
    case IrOpcode::kLoadField:
      return ReduceLoadField(node);
    default:
      break;
  }
  return NoChange();
}


Factory* JSTypedLowering::factory() const { return jsgraph()->factory(); }


Graph* JSTypedLowering::graph() const { return jsgraph()->graph(); }


Isolate* JSTypedLowering::isolate() const { return jsgraph()->isolate(); }


JSOperatorBuilder* JSTypedLowering::javascript() const {
  return jsgraph()->javascript();
}


CommonOperatorBuilder* JSTypedLowering::common() const {
  return jsgraph()->common();
}

MachineOperatorBuilder* JSTypedLowering::machine() const {
  return jsgraph()->machine();
}

SimplifiedOperatorBuilder* JSTypedLowering::simplified() const {
  return jsgraph()->simplified();
}


CompilationDependencies* JSTypedLowering::dependencies() const {
  return dependencies_;
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
