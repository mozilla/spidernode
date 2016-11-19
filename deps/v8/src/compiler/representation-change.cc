// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/representation-change.h"

#include <sstream>

#include "src/base/bits.h"
#include "src/code-factory.h"
#include "src/compiler/machine-operator.h"

namespace v8 {
namespace internal {
namespace compiler {

const char* Truncation::description() const {
  switch (kind()) {
    case TruncationKind::kNone:
      return "no-value-use";
    case TruncationKind::kBool:
      return "truncate-to-bool";
    case TruncationKind::kWord32:
      return "truncate-to-word32";
    case TruncationKind::kWord64:
      return "truncate-to-word64";
    case TruncationKind::kFloat32:
      return "truncate-to-float32";
    case TruncationKind::kFloat64:
      return "truncate-to-float64";
    case TruncationKind::kAny:
      return "no-truncation";
  }
  UNREACHABLE();
  return nullptr;
}


// Partial order for truncations:
//
//  kWord64       kAny
//     ^            ^
//     \            |
//      \         kFloat64  <--+
//       \        ^    ^       |
//        \       /    |       |
//         kWord32  kFloat32  kBool
//               ^     ^      ^
//               \     |      /
//                \    |     /
//                 \   |    /
//                  \  |   /
//                   \ |  /
//                   kNone

// static
Truncation::TruncationKind Truncation::Generalize(TruncationKind rep1,
                                                  TruncationKind rep2) {
  if (LessGeneral(rep1, rep2)) return rep2;
  if (LessGeneral(rep2, rep1)) return rep1;
  // Handle the generalization of float64-representable values.
  if (LessGeneral(rep1, TruncationKind::kFloat64) &&
      LessGeneral(rep2, TruncationKind::kFloat64)) {
    return TruncationKind::kFloat64;
  }
  // Handle the generalization of any-representable values.
  if (LessGeneral(rep1, TruncationKind::kAny) &&
      LessGeneral(rep2, TruncationKind::kAny)) {
    return TruncationKind::kAny;
  }
  // All other combinations are illegal.
  FATAL("Tried to combine incompatible truncations");
  return TruncationKind::kNone;
}


// static
bool Truncation::LessGeneral(TruncationKind rep1, TruncationKind rep2) {
  switch (rep1) {
    case TruncationKind::kNone:
      return true;
    case TruncationKind::kBool:
      return rep2 == TruncationKind::kBool || rep2 == TruncationKind::kAny;
    case TruncationKind::kWord32:
      return rep2 == TruncationKind::kWord32 ||
             rep2 == TruncationKind::kWord64 ||
             rep2 == TruncationKind::kFloat64 || rep2 == TruncationKind::kAny;
    case TruncationKind::kWord64:
      return rep2 == TruncationKind::kWord64;
    case TruncationKind::kFloat32:
      return rep2 == TruncationKind::kFloat32 ||
             rep2 == TruncationKind::kFloat64 || rep2 == TruncationKind::kAny;
    case TruncationKind::kFloat64:
      return rep2 == TruncationKind::kFloat64 || rep2 == TruncationKind::kAny;
    case TruncationKind::kAny:
      return rep2 == TruncationKind::kAny;
  }
  UNREACHABLE();
  return false;
}


namespace {

bool IsWord(MachineRepresentation rep) {
  return rep == MachineRepresentation::kWord8 ||
         rep == MachineRepresentation::kWord16 ||
         rep == MachineRepresentation::kWord32;
}

}  // namespace

// Changes representation from {output_rep} to {use_rep}. The {truncation}
// parameter is only used for sanity checking - if the changer cannot figure
// out signedness for the word32->float64 conversion, then we check that the
// uses truncate to word32 (so they do not care about signedness).
Node* RepresentationChanger::GetRepresentationFor(
    Node* node, MachineRepresentation output_rep, Type* output_type,
    Node* use_node, UseInfo use_info) {
  if (output_rep == MachineRepresentation::kNone &&
      output_type->IsInhabited()) {
    // The output representation should be set if the type is inhabited (i.e.,
    // if the value is possible).
    return TypeError(node, output_rep, output_type, use_info.representation());
  }

  // Handle the no-op shortcuts when no checking is necessary.
  if (use_info.type_check() == TypeCheckKind::kNone ||
      output_rep != MachineRepresentation::kWord32) {
    if (use_info.representation() == output_rep) {
      // Representations are the same. That's a no-op.
      return node;
    }
    if (IsWord(use_info.representation()) && IsWord(output_rep)) {
      // Both are words less than or equal to 32-bits.
      // Since loads of integers from memory implicitly sign or zero extend the
      // value to the full machine word size and stores implicitly truncate,
      // no representation change is necessary.
      return node;
    }
  }

  switch (use_info.representation()) {
    case MachineRepresentation::kTaggedSigned:
    case MachineRepresentation::kTaggedPointer:
    case MachineRepresentation::kTagged:
      DCHECK(use_info.type_check() == TypeCheckKind::kNone);
      return GetTaggedRepresentationFor(node, output_rep, output_type);
    case MachineRepresentation::kFloat32:
      DCHECK(use_info.type_check() == TypeCheckKind::kNone);
      return GetFloat32RepresentationFor(node, output_rep, output_type,
                                         use_info.truncation());
    case MachineRepresentation::kFloat64:
      return GetFloat64RepresentationFor(node, output_rep, output_type,
                                         use_node, use_info);
    case MachineRepresentation::kBit:
      DCHECK(use_info.type_check() == TypeCheckKind::kNone);
      return GetBitRepresentationFor(node, output_rep, output_type);
    case MachineRepresentation::kWord8:
    case MachineRepresentation::kWord16:
    case MachineRepresentation::kWord32:
      return GetWord32RepresentationFor(node, output_rep, output_type, use_node,
                                        use_info);
    case MachineRepresentation::kWord64:
      DCHECK(use_info.type_check() == TypeCheckKind::kNone);
      return GetWord64RepresentationFor(node, output_rep, output_type);
    case MachineRepresentation::kSimd128:  // Fall through.
      // TODO(bbudge) Handle conversions between tagged and untagged.
      break;
    case MachineRepresentation::kNone:
      return node;
  }
  UNREACHABLE();
  return nullptr;
}

Node* RepresentationChanger::GetTaggedRepresentationFor(
    Node* node, MachineRepresentation output_rep, Type* output_type) {
  // Eagerly fold representation changes for constants.
  switch (node->opcode()) {
    case IrOpcode::kNumberConstant:
    case IrOpcode::kHeapConstant:
      return node;  // No change necessary.
    case IrOpcode::kInt32Constant:
      if (output_type->Is(Type::Signed32())) {
        int32_t value = OpParameter<int32_t>(node);
        return jsgraph()->Constant(value);
      } else if (output_type->Is(Type::Unsigned32())) {
        uint32_t value = static_cast<uint32_t>(OpParameter<int32_t>(node));
        return jsgraph()->Constant(static_cast<double>(value));
      } else if (output_type->Is(Type::Boolean())) {
        return OpParameter<int32_t>(node) == 0 ? jsgraph()->FalseConstant()
                                               : jsgraph()->TrueConstant();
      } else {
        return TypeError(node, output_rep, output_type,
                         MachineRepresentation::kTagged);
      }
    case IrOpcode::kFloat64Constant:
      return jsgraph()->Constant(OpParameter<double>(node));
    case IrOpcode::kFloat32Constant:
      return jsgraph()->Constant(OpParameter<float>(node));
    default:
      break;
  }
  // Select the correct X -> Tagged operator.
  const Operator* op;
  if (output_rep == MachineRepresentation::kNone) {
    // We should only asisgn this representation if the type is empty.
    CHECK(!output_type->IsInhabited());
    op = machine()->ImpossibleToTagged();
  } else if (output_rep == MachineRepresentation::kBit) {
    if (output_type->Is(Type::Boolean())) {
      op = simplified()->ChangeBitToTagged();
    } else {
      return TypeError(node, output_rep, output_type,
                       MachineRepresentation::kTagged);
    }
  } else if (IsWord(output_rep)) {
    if (output_type->Is(Type::Signed31())) {
      op = simplified()->ChangeInt31ToTaggedSigned();
    } else if (output_type->Is(Type::Signed32())) {
      op = simplified()->ChangeInt32ToTagged();
    } else if (output_type->Is(Type::Unsigned32())) {
      op = simplified()->ChangeUint32ToTagged();
    } else {
      return TypeError(node, output_rep, output_type,
                       MachineRepresentation::kTagged);
    }
  } else if (output_rep ==
             MachineRepresentation::kFloat32) {  // float32 -> float64 -> tagged
    node = InsertChangeFloat32ToFloat64(node);
    op = simplified()->ChangeFloat64ToTagged(
        output_type->Maybe(Type::MinusZero())
            ? CheckForMinusZeroMode::kCheckForMinusZero
            : CheckForMinusZeroMode::kDontCheckForMinusZero);
  } else if (output_rep == MachineRepresentation::kFloat64) {
    if (output_type->Is(Type::Signed31())) {  // float64 -> int32 -> tagged
      node = InsertChangeFloat64ToInt32(node);
      op = simplified()->ChangeInt31ToTaggedSigned();
    } else if (output_type->Is(
                   Type::Signed32())) {  // float64 -> int32 -> tagged
      node = InsertChangeFloat64ToInt32(node);
      op = simplified()->ChangeInt32ToTagged();
    } else if (output_type->Is(
                   Type::Unsigned32())) {  // float64 -> uint32 -> tagged
      node = InsertChangeFloat64ToUint32(node);
      op = simplified()->ChangeUint32ToTagged();
    } else {
      op = simplified()->ChangeFloat64ToTagged(
          output_type->Maybe(Type::MinusZero())
              ? CheckForMinusZeroMode::kCheckForMinusZero
              : CheckForMinusZeroMode::kDontCheckForMinusZero);
    }
  } else {
    return TypeError(node, output_rep, output_type,
                     MachineRepresentation::kTagged);
  }
  return jsgraph()->graph()->NewNode(op, node);
}


Node* RepresentationChanger::GetFloat32RepresentationFor(
    Node* node, MachineRepresentation output_rep, Type* output_type,
    Truncation truncation) {
  // Eagerly fold representation changes for constants.
  switch (node->opcode()) {
    case IrOpcode::kFloat64Constant:
    case IrOpcode::kNumberConstant:
      return jsgraph()->Float32Constant(
          DoubleToFloat32(OpParameter<double>(node)));
    case IrOpcode::kInt32Constant:
      if (output_type->Is(Type::Unsigned32())) {
        uint32_t value = static_cast<uint32_t>(OpParameter<int32_t>(node));
        return jsgraph()->Float32Constant(static_cast<float>(value));
      } else {
        int32_t value = OpParameter<int32_t>(node);
        return jsgraph()->Float32Constant(static_cast<float>(value));
      }
    case IrOpcode::kFloat32Constant:
      return node;  // No change necessary.
    default:
      break;
  }
  // Select the correct X -> Float32 operator.
  const Operator* op = nullptr;
  if (output_rep == MachineRepresentation::kNone) {
    // We should only use kNone representation if the type is empty.
    CHECK(!output_type->IsInhabited());
    op = machine()->ImpossibleToFloat32();
  } else if (IsWord(output_rep)) {
    if (output_type->Is(Type::Signed32())) {
      // int32 -> float64 -> float32
      op = machine()->ChangeInt32ToFloat64();
      node = jsgraph()->graph()->NewNode(op, node);
      op = machine()->TruncateFloat64ToFloat32();
    } else if (output_type->Is(Type::Unsigned32()) ||
               truncation.IsUsedAsWord32()) {
      // Either the output is uint32 or the uses only care about the
      // low 32 bits (so we can pick uint32 safely).

      // uint32 -> float64 -> float32
      op = machine()->ChangeUint32ToFloat64();
      node = jsgraph()->graph()->NewNode(op, node);
      op = machine()->TruncateFloat64ToFloat32();
    }
  } else if (output_rep == MachineRepresentation::kTagged) {
    if (output_type->Is(Type::NumberOrOddball())) {
      // tagged -> float64 -> float32
      if (output_type->Is(Type::Number())) {
        op = simplified()->ChangeTaggedToFloat64();
      } else {
        op = simplified()->TruncateTaggedToFloat64();
      }
      node = jsgraph()->graph()->NewNode(op, node);
      op = machine()->TruncateFloat64ToFloat32();
    }
  } else if (output_rep == MachineRepresentation::kFloat64) {
    op = machine()->TruncateFloat64ToFloat32();
  }
  if (op == nullptr) {
    return TypeError(node, output_rep, output_type,
                     MachineRepresentation::kFloat32);
  }
  return jsgraph()->graph()->NewNode(op, node);
}

Node* RepresentationChanger::GetFloat64RepresentationFor(
    Node* node, MachineRepresentation output_rep, Type* output_type,
    Node* use_node, UseInfo use_info) {
  // Eagerly fold representation changes for constants.
  if ((use_info.type_check() == TypeCheckKind::kNone)) {
    // TODO(jarin) Handle checked constant conversions.
    switch (node->opcode()) {
      case IrOpcode::kNumberConstant:
        return jsgraph()->Float64Constant(OpParameter<double>(node));
      case IrOpcode::kInt32Constant:
        if (output_type->Is(Type::Signed32())) {
          int32_t value = OpParameter<int32_t>(node);
          return jsgraph()->Float64Constant(value);
        } else {
          DCHECK(output_type->Is(Type::Unsigned32()));
          uint32_t value = static_cast<uint32_t>(OpParameter<int32_t>(node));
          return jsgraph()->Float64Constant(static_cast<double>(value));
        }
      case IrOpcode::kFloat64Constant:
        return node;  // No change necessary.
      case IrOpcode::kFloat32Constant:
        return jsgraph()->Float64Constant(OpParameter<float>(node));
      default:
        break;
    }
  }
  // Select the correct X -> Float64 operator.
  const Operator* op = nullptr;
  if (output_rep == MachineRepresentation::kNone) {
    // We should only use kNone representation if the type is empty.
    CHECK(!output_type->IsInhabited());
    op = machine()->ImpossibleToFloat64();
  } else if (IsWord(output_rep)) {
    if (output_type->Is(Type::Signed32())) {
      op = machine()->ChangeInt32ToFloat64();
    } else if (output_type->Is(Type::Unsigned32()) ||
               use_info.truncation().IsUsedAsWord32()) {
      // Either the output is uint32 or the uses only care about the
      // low 32 bits (so we can pick uint32 safely).
      op = machine()->ChangeUint32ToFloat64();
    }
  } else if (output_rep == MachineRepresentation::kBit) {
    op = machine()->ChangeUint32ToFloat64();
  } else if (output_rep == MachineRepresentation::kTagged) {
    if (output_type->Is(Type::Undefined())) {
      return jsgraph()->Float64Constant(
          std::numeric_limits<double>::quiet_NaN());
    } else if (output_type->Is(Type::TaggedSigned())) {
      node = InsertChangeTaggedSignedToInt32(node);
      op = machine()->ChangeInt32ToFloat64();
    } else if (output_type->Is(Type::Number())) {
      op = simplified()->ChangeTaggedToFloat64();
    } else if (output_type->Is(Type::NumberOrOddball())) {
      // TODO(jarin) Here we should check that truncation is Number.
      op = simplified()->TruncateTaggedToFloat64();
    } else if (use_info.type_check() == TypeCheckKind::kNumber ||
               (use_info.type_check() == TypeCheckKind::kNumberOrOddball &&
                !output_type->Maybe(Type::BooleanOrNullOrNumber()))) {
      op = simplified()->CheckedTaggedToFloat64(CheckTaggedInputMode::kNumber);
    } else if (use_info.type_check() == TypeCheckKind::kNumberOrOddball) {
      op = simplified()->CheckedTaggedToFloat64(
          CheckTaggedInputMode::kNumberOrOddball);
    }
  } else if (output_rep == MachineRepresentation::kFloat32) {
    op = machine()->ChangeFloat32ToFloat64();
  }
  if (op == nullptr) {
    return TypeError(node, output_rep, output_type,
                     MachineRepresentation::kFloat64);
  }
  return InsertConversion(node, op, use_node);
}

Node* RepresentationChanger::MakeTruncatedInt32Constant(double value) {
  return jsgraph()->Int32Constant(DoubleToInt32(value));
}

Node* RepresentationChanger::GetWord32RepresentationFor(
    Node* node, MachineRepresentation output_rep, Type* output_type,
    Node* use_node, UseInfo use_info) {
  // Eagerly fold representation changes for constants.
  switch (node->opcode()) {
    case IrOpcode::kInt32Constant:
      return node;  // No change necessary.
    case IrOpcode::kFloat32Constant: {
      float const fv = OpParameter<float>(node);
      if (use_info.type_check() == TypeCheckKind::kNone ||
          ((use_info.type_check() == TypeCheckKind::kSignedSmall ||
            use_info.type_check() == TypeCheckKind::kSigned32) &&
           IsInt32Double(fv))) {
        return MakeTruncatedInt32Constant(fv);
      }
      break;
    }
    case IrOpcode::kNumberConstant:
    case IrOpcode::kFloat64Constant: {
      double const fv = OpParameter<double>(node);
      if (use_info.type_check() == TypeCheckKind::kNone ||
          ((use_info.type_check() == TypeCheckKind::kSignedSmall ||
            use_info.type_check() == TypeCheckKind::kSigned32) &&
           IsInt32Double(fv))) {
        return MakeTruncatedInt32Constant(fv);
      }
      break;
    }
    default:
      break;
  }

  // Select the correct X -> Word32 operator.
  const Operator* op = nullptr;
  if (output_rep == MachineRepresentation::kNone) {
    // We should only use kNone representation if the type is empty.
    CHECK(!output_type->IsInhabited());
    op = machine()->ImpossibleToWord32();
  } else if (output_rep == MachineRepresentation::kBit) {
    return node;  // Sloppy comparison -> word32
  } else if (output_rep == MachineRepresentation::kFloat64) {
    if (output_type->Is(Type::Unsigned32())) {
      op = machine()->ChangeFloat64ToUint32();
    } else if (output_type->Is(Type::Signed32())) {
      op = machine()->ChangeFloat64ToInt32();
    } else if (use_info.truncation().IsUsedAsWord32()) {
      op = machine()->TruncateFloat64ToWord32();
    } else if (use_info.type_check() == TypeCheckKind::kSignedSmall ||
               use_info.type_check() == TypeCheckKind::kSigned32) {
      op = simplified()->CheckedFloat64ToInt32(
          output_type->Maybe(Type::MinusZero())
              ? CheckForMinusZeroMode::kCheckForMinusZero
              : CheckForMinusZeroMode::kDontCheckForMinusZero);
    }
  } else if (output_rep == MachineRepresentation::kFloat32) {
    node = InsertChangeFloat32ToFloat64(node);  // float32 -> float64 -> int32
    if (output_type->Is(Type::Unsigned32())) {
      op = machine()->ChangeFloat64ToUint32();
    } else if (output_type->Is(Type::Signed32())) {
      op = machine()->ChangeFloat64ToInt32();
    } else if (use_info.truncation().IsUsedAsWord32()) {
      op = machine()->TruncateFloat64ToWord32();
    } else if (use_info.type_check() == TypeCheckKind::kSignedSmall ||
               use_info.type_check() == TypeCheckKind::kSigned32) {
      op = simplified()->CheckedFloat64ToInt32(
          output_type->Maybe(Type::MinusZero())
              ? CheckForMinusZeroMode::kCheckForMinusZero
              : CheckForMinusZeroMode::kDontCheckForMinusZero);
    }
  } else if (output_rep == MachineRepresentation::kTagged) {
    if (output_type->Is(Type::TaggedSigned())) {
      op = simplified()->ChangeTaggedSignedToInt32();
    } else if (output_type->Is(Type::Unsigned32())) {
      op = simplified()->ChangeTaggedToUint32();
    } else if (output_type->Is(Type::Signed32())) {
      op = simplified()->ChangeTaggedToInt32();
    } else if (use_info.truncation().IsUsedAsWord32()) {
      if (use_info.type_check() != TypeCheckKind::kNone) {
        op = simplified()->CheckedTruncateTaggedToWord32();
      } else {
        op = simplified()->TruncateTaggedToWord32();
      }
    } else if (use_info.type_check() == TypeCheckKind::kSignedSmall) {
      op = simplified()->CheckedTaggedSignedToInt32();
    } else if (use_info.type_check() == TypeCheckKind::kSigned32) {
      op = simplified()->CheckedTaggedToInt32(
          output_type->Maybe(Type::MinusZero())
              ? CheckForMinusZeroMode::kCheckForMinusZero
              : CheckForMinusZeroMode::kDontCheckForMinusZero);
    }
  } else if (output_rep == MachineRepresentation::kWord32) {
    // Only the checked case should get here, the non-checked case is
    // handled in GetRepresentationFor.
    if (use_info.type_check() == TypeCheckKind::kSignedSmall ||
        use_info.type_check() == TypeCheckKind::kSigned32) {
      if (output_type->Is(Type::Signed32())) {
        return node;
      } else if (output_type->Is(Type::Unsigned32())) {
        op = simplified()->CheckedUint32ToInt32();
      }
    } else {
      DCHECK_EQ(TypeCheckKind::kNumberOrOddball, use_info.type_check());
      return node;
    }
  } else if (output_rep == MachineRepresentation::kWord8 ||
             output_rep == MachineRepresentation::kWord16) {
    DCHECK(use_info.representation() == MachineRepresentation::kWord32);
    DCHECK(use_info.type_check() == TypeCheckKind::kSignedSmall ||
           use_info.type_check() == TypeCheckKind::kSigned32);
    return node;
  }

  if (op == nullptr) {
    return TypeError(node, output_rep, output_type,
                     MachineRepresentation::kWord32);
  }
  return InsertConversion(node, op, use_node);
}

Node* RepresentationChanger::InsertConversion(Node* node, const Operator* op,
                                              Node* use_node) {
  if (op->ControlInputCount() > 0) {
    // If the operator can deoptimize (which means it has control
    // input), we need to connect it to the effect and control chains.
    Node* effect = NodeProperties::GetEffectInput(use_node);
    Node* control = NodeProperties::GetControlInput(use_node);
    Node* conversion = jsgraph()->graph()->NewNode(op, node, effect, control);
    NodeProperties::ReplaceEffectInput(use_node, conversion);
    return conversion;
  }
  return jsgraph()->graph()->NewNode(op, node);
}


Node* RepresentationChanger::GetBitRepresentationFor(
    Node* node, MachineRepresentation output_rep, Type* output_type) {
  // Eagerly fold representation changes for constants.
  switch (node->opcode()) {
    case IrOpcode::kHeapConstant: {
      Handle<HeapObject> value = OpParameter<Handle<HeapObject>>(node);
      DCHECK(value.is_identical_to(factory()->true_value()) ||
             value.is_identical_to(factory()->false_value()));
      return jsgraph()->Int32Constant(
          value.is_identical_to(factory()->true_value()) ? 1 : 0);
    }
    default:
      break;
  }
  // Select the correct X -> Bit operator.
  const Operator* op;
  if (output_rep == MachineRepresentation::kNone) {
    // We should only use kNone representation if the type is empty.
    CHECK(!output_type->IsInhabited());
    op = machine()->ImpossibleToBit();
  } else if (output_rep == MachineRepresentation::kTagged) {
    op = simplified()->ChangeTaggedToBit();
  } else {
    return TypeError(node, output_rep, output_type,
                     MachineRepresentation::kBit);
  }
  return jsgraph()->graph()->NewNode(op, node);
}

Node* RepresentationChanger::GetWord64RepresentationFor(
    Node* node, MachineRepresentation output_rep, Type* output_type) {
  if (output_rep == MachineRepresentation::kNone) {
    // We should only use kNone representation if the type is empty.
    CHECK(!output_type->IsInhabited());
    return jsgraph()->graph()->NewNode(machine()->ImpossibleToFloat64(), node);
  } else if (output_rep == MachineRepresentation::kBit) {
    return node;  // Sloppy comparison -> word64
  }
  // Can't really convert Word64 to anything else. Purported to be internal.
  return TypeError(node, output_rep, output_type,
                   MachineRepresentation::kWord64);
}

const Operator* RepresentationChanger::Int32OperatorFor(
    IrOpcode::Value opcode) {
  switch (opcode) {
    case IrOpcode::kSpeculativeNumberAdd:  // Fall through.
    case IrOpcode::kNumberAdd:
      return machine()->Int32Add();
    case IrOpcode::kSpeculativeNumberSubtract:  // Fall through.
    case IrOpcode::kNumberSubtract:
      return machine()->Int32Sub();
    case IrOpcode::kSpeculativeNumberMultiply:
    case IrOpcode::kNumberMultiply:
      return machine()->Int32Mul();
    case IrOpcode::kSpeculativeNumberDivide:
    case IrOpcode::kNumberDivide:
      return machine()->Int32Div();
    case IrOpcode::kSpeculativeNumberModulus:
    case IrOpcode::kNumberModulus:
      return machine()->Int32Mod();
    case IrOpcode::kSpeculativeNumberBitwiseOr:  // Fall through.
    case IrOpcode::kNumberBitwiseOr:
      return machine()->Word32Or();
    case IrOpcode::kSpeculativeNumberBitwiseXor:  // Fall through.
    case IrOpcode::kNumberBitwiseXor:
      return machine()->Word32Xor();
    case IrOpcode::kSpeculativeNumberBitwiseAnd:  // Fall through.
    case IrOpcode::kNumberBitwiseAnd:
      return machine()->Word32And();
    case IrOpcode::kNumberEqual:
    case IrOpcode::kSpeculativeNumberEqual:
      return machine()->Word32Equal();
    case IrOpcode::kNumberLessThan:
    case IrOpcode::kSpeculativeNumberLessThan:
      return machine()->Int32LessThan();
    case IrOpcode::kNumberLessThanOrEqual:
    case IrOpcode::kSpeculativeNumberLessThanOrEqual:
      return machine()->Int32LessThanOrEqual();
    default:
      UNREACHABLE();
      return nullptr;
  }
}

const Operator* RepresentationChanger::Int32OverflowOperatorFor(
    IrOpcode::Value opcode) {
  switch (opcode) {
    case IrOpcode::kSpeculativeNumberAdd:
      return simplified()->CheckedInt32Add();
    case IrOpcode::kSpeculativeNumberSubtract:
      return simplified()->CheckedInt32Sub();
    case IrOpcode::kSpeculativeNumberDivide:
      return simplified()->CheckedInt32Div();
    case IrOpcode::kSpeculativeNumberModulus:
      return simplified()->CheckedInt32Mod();
    default:
      UNREACHABLE();
      return nullptr;
  }
}

const Operator* RepresentationChanger::Uint32OperatorFor(
    IrOpcode::Value opcode) {
  switch (opcode) {
    case IrOpcode::kNumberAdd:
      return machine()->Int32Add();
    case IrOpcode::kNumberSubtract:
      return machine()->Int32Sub();
    case IrOpcode::kSpeculativeNumberMultiply:
    case IrOpcode::kNumberMultiply:
      return machine()->Int32Mul();
    case IrOpcode::kSpeculativeNumberDivide:
    case IrOpcode::kNumberDivide:
      return machine()->Uint32Div();
    case IrOpcode::kSpeculativeNumberModulus:
    case IrOpcode::kNumberModulus:
      return machine()->Uint32Mod();
    case IrOpcode::kNumberEqual:
    case IrOpcode::kSpeculativeNumberEqual:
      return machine()->Word32Equal();
    case IrOpcode::kNumberLessThan:
    case IrOpcode::kSpeculativeNumberLessThan:
      return machine()->Uint32LessThan();
    case IrOpcode::kNumberLessThanOrEqual:
    case IrOpcode::kSpeculativeNumberLessThanOrEqual:
      return machine()->Uint32LessThanOrEqual();
    case IrOpcode::kNumberClz32:
      return machine()->Word32Clz();
    case IrOpcode::kNumberImul:
      return machine()->Int32Mul();
    default:
      UNREACHABLE();
      return nullptr;
  }
}

const Operator* RepresentationChanger::Uint32OverflowOperatorFor(
    IrOpcode::Value opcode) {
  switch (opcode) {
    case IrOpcode::kSpeculativeNumberDivide:
      return simplified()->CheckedUint32Div();
    case IrOpcode::kSpeculativeNumberModulus:
      return simplified()->CheckedUint32Mod();
    default:
      UNREACHABLE();
      return nullptr;
  }
}

const Operator* RepresentationChanger::Float64OperatorFor(
    IrOpcode::Value opcode) {
  switch (opcode) {
    case IrOpcode::kSpeculativeNumberAdd:
    case IrOpcode::kNumberAdd:
      return machine()->Float64Add();
    case IrOpcode::kSpeculativeNumberSubtract:
    case IrOpcode::kNumberSubtract:
      return machine()->Float64Sub();
    case IrOpcode::kSpeculativeNumberMultiply:
    case IrOpcode::kNumberMultiply:
      return machine()->Float64Mul();
    case IrOpcode::kSpeculativeNumberDivide:
    case IrOpcode::kNumberDivide:
      return machine()->Float64Div();
    case IrOpcode::kSpeculativeNumberModulus:
    case IrOpcode::kNumberModulus:
      return machine()->Float64Mod();
    case IrOpcode::kNumberEqual:
    case IrOpcode::kSpeculativeNumberEqual:
      return machine()->Float64Equal();
    case IrOpcode::kNumberLessThan:
    case IrOpcode::kSpeculativeNumberLessThan:
      return machine()->Float64LessThan();
    case IrOpcode::kNumberLessThanOrEqual:
    case IrOpcode::kSpeculativeNumberLessThanOrEqual:
      return machine()->Float64LessThanOrEqual();
    case IrOpcode::kNumberAbs:
      return machine()->Float64Abs();
    case IrOpcode::kNumberAcos:
      return machine()->Float64Acos();
    case IrOpcode::kNumberAcosh:
      return machine()->Float64Acosh();
    case IrOpcode::kNumberAsin:
      return machine()->Float64Asin();
    case IrOpcode::kNumberAsinh:
      return machine()->Float64Asinh();
    case IrOpcode::kNumberAtan:
      return machine()->Float64Atan();
    case IrOpcode::kNumberAtanh:
      return machine()->Float64Atanh();
    case IrOpcode::kNumberAtan2:
      return machine()->Float64Atan2();
    case IrOpcode::kNumberCbrt:
      return machine()->Float64Cbrt();
    case IrOpcode::kNumberCeil:
      return machine()->Float64RoundUp().placeholder();
    case IrOpcode::kNumberCos:
      return machine()->Float64Cos();
    case IrOpcode::kNumberCosh:
      return machine()->Float64Cosh();
    case IrOpcode::kNumberExp:
      return machine()->Float64Exp();
    case IrOpcode::kNumberExpm1:
      return machine()->Float64Expm1();
    case IrOpcode::kNumberFloor:
      return machine()->Float64RoundDown().placeholder();
    case IrOpcode::kNumberFround:
      return machine()->TruncateFloat64ToFloat32();
    case IrOpcode::kNumberLog:
      return machine()->Float64Log();
    case IrOpcode::kNumberLog1p:
      return machine()->Float64Log1p();
    case IrOpcode::kNumberLog2:
      return machine()->Float64Log2();
    case IrOpcode::kNumberLog10:
      return machine()->Float64Log10();
    case IrOpcode::kNumberMax:
      return machine()->Float64Max();
    case IrOpcode::kNumberMin:
      return machine()->Float64Min();
    case IrOpcode::kNumberPow:
      return machine()->Float64Pow();
    case IrOpcode::kNumberSin:
      return machine()->Float64Sin();
    case IrOpcode::kNumberSinh:
      return machine()->Float64Sinh();
    case IrOpcode::kNumberSqrt:
      return machine()->Float64Sqrt();
    case IrOpcode::kNumberTan:
      return machine()->Float64Tan();
    case IrOpcode::kNumberTanh:
      return machine()->Float64Tanh();
    case IrOpcode::kNumberTrunc:
      return machine()->Float64RoundTruncate().placeholder();
    case IrOpcode::kNumberSilenceNaN:
      return machine()->Float64SilenceNaN();
    default:
      UNREACHABLE();
      return nullptr;
  }
}


Node* RepresentationChanger::TypeError(Node* node,
                                       MachineRepresentation output_rep,
                                       Type* output_type,
                                       MachineRepresentation use) {
  type_error_ = true;
  if (!testing_type_errors_) {
    std::ostringstream out_str;
    out_str << output_rep << " (";
    output_type->PrintTo(out_str, Type::SEMANTIC_DIM);
    out_str << ")";

    std::ostringstream use_str;
    use_str << use;

    V8_Fatal(__FILE__, __LINE__,
             "RepresentationChangerError: node #%d:%s of "
             "%s cannot be changed to %s",
             node->id(), node->op()->mnemonic(), out_str.str().c_str(),
             use_str.str().c_str());
  }
  return node;
}


Node* RepresentationChanger::InsertChangeFloat32ToFloat64(Node* node) {
  return jsgraph()->graph()->NewNode(machine()->ChangeFloat32ToFloat64(), node);
}

Node* RepresentationChanger::InsertChangeFloat64ToUint32(Node* node) {
  return jsgraph()->graph()->NewNode(machine()->ChangeFloat64ToUint32(), node);
}

Node* RepresentationChanger::InsertChangeFloat64ToInt32(Node* node) {
  return jsgraph()->graph()->NewNode(machine()->ChangeFloat64ToInt32(), node);
}

Node* RepresentationChanger::InsertChangeTaggedSignedToInt32(Node* node) {
  return jsgraph()->graph()->NewNode(simplified()->ChangeTaggedSignedToInt32(),
                                     node);
}

Node* RepresentationChanger::InsertChangeTaggedToFloat64(Node* node) {
  return jsgraph()->graph()->NewNode(simplified()->ChangeTaggedToFloat64(),
                                     node);
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
