// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SRC_IC_BINARY_OP_ASSEMBLER_H_
#define V8_SRC_IC_BINARY_OP_ASSEMBLER_H_

#include <functional>
#include "src/code-stub-assembler.h"

namespace v8 {
namespace internal {

namespace compiler {
class CodeAssemblerState;
}

class BinaryOpAssembler : public CodeStubAssembler {
 public:
  typedef compiler::Node Node;

  explicit BinaryOpAssembler(compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

  Node* Generate_AddWithFeedback(Node* context, Node* lhs, Node* rhs,
                                 Node* slot_id, Node* feedback_vector,
                                 Node* function, bool rhs_is_smi);

  Node* Generate_SubtractWithFeedback(Node* context, Node* lhs, Node* rhs,
                                      Node* slot_id, Node* feedback_vector,
                                      Node* function, bool rhs_is_smi);

  Node* Generate_MultiplyWithFeedback(Node* context, Node* lhs, Node* rhs,
                                      Node* slot_id, Node* feedback_vector,
                                      Node* function, bool rhs_is_smi);

  Node* Generate_DivideWithFeedback(Node* context, Node* dividend,
                                    Node* divisor, Node* slot_id,
                                    Node* feedback_vector, Node* function,
                                    bool rhs_is_smi);

  Node* Generate_ModulusWithFeedback(Node* context, Node* dividend,
                                     Node* divisor, Node* slot_id,
                                     Node* feedback_vector, Node* function,
                                     bool rhs_is_smi);

 private:
  typedef std::function<Node*(Node*, Node*, Variable*)> SmiOperation;
  typedef std::function<Node*(Node*, Node*)> FloatOperation;

  Node* Generate_BinaryOperationWithFeedback(
      Node* context, Node* lhs, Node* rhs, Node* slot_id, Node* feedback_vector,
      Node* function, const SmiOperation& smiOperation,
      const FloatOperation& floatOperation, Token::Value opcode,
      bool rhs_is_smi);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_SRC_IC_BINARY_OP_ASSEMBLER_H_
