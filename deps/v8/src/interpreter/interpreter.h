// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTERPRETER_INTERPRETER_H_
#define V8_INTERPRETER_INTERPRETER_H_

#include <memory>

// Clients of this interface shouldn't depend on lots of interpreter internals.
// Do not include anything from src/interpreter other than
// src/interpreter/bytecodes.h here!
#include "src/base/macros.h"
#include "src/builtins/builtins.h"
#include "src/interpreter/bytecodes.h"
#include "src/parsing/token.h"
#include "src/runtime/runtime.h"

namespace v8 {
namespace internal {

class Isolate;
class Callable;
class CompilationInfo;

namespace compiler {
class Node;
}  // namespace compiler

namespace interpreter {

class InterpreterAssembler;

class Interpreter {
 public:
  explicit Interpreter(Isolate* isolate);
  virtual ~Interpreter() {}

  // Initializes the interpreter dispatch table.
  void Initialize();

  // Returns the interrupt budget which should be used for the profiler counter.
  static int InterruptBudget();

  // Generate bytecode for |info|.
  static bool MakeBytecode(CompilationInfo* info);

  // Return bytecode handler for |bytecode|.
  Code* GetBytecodeHandler(Bytecode bytecode, OperandScale operand_scale);

  // GC support.
  void IterateDispatchTable(ObjectVisitor* v);

  // Disassembler support (only useful with ENABLE_DISASSEMBLER defined).
  void TraceCodegen(Handle<Code> code);
  const char* LookupNameOfBytecodeHandler(Code* code);

  Local<v8::Object> GetDispatchCountersObject();

  Address dispatch_table_address() {
    return reinterpret_cast<Address>(&dispatch_table_[0]);
  }

  Address bytecode_dispatch_counters_table() {
    return reinterpret_cast<Address>(bytecode_dispatch_counters_table_.get());
  }

  // TODO(ignition): Tune code size multiplier.
  static const int kCodeSizeMultiplier = 32;

 private:
// Bytecode handler generator functions.
#define DECLARE_BYTECODE_HANDLER_GENERATOR(Name, ...) \
  void Do##Name(InterpreterAssembler* assembler);
  BYTECODE_LIST(DECLARE_BYTECODE_HANDLER_GENERATOR)
#undef DECLARE_BYTECODE_HANDLER_GENERATOR

  // Generates code to perform the binary operation via |Generator|.
  template <class Generator>
  void DoBinaryOp(InterpreterAssembler* assembler);

  // Generates code to perform the binary operation via |Generator|.
  template <class Generator>
  void DoBinaryOpWithFeedback(InterpreterAssembler* assembler);

  // Generates code to perform the bitwise binary operation corresponding to
  // |bitwise_op| while gathering type feedback.
  void DoBitwiseBinaryOp(Token::Value bitwise_op,
                         InterpreterAssembler* assembler);

  // Generates code to perform the binary operation via |Generator| using
  // an immediate value rather the accumulator as the rhs operand.
  template <class Generator>
  void DoBinaryOpWithImmediate(InterpreterAssembler* assembler);

  // Generates code to perform the unary operation via |Generator|.
  template <class Generator>
  void DoUnaryOp(InterpreterAssembler* assembler);

  // Generates code to perform the unary operation via |Generator| while
  // gatering type feedback.
  template <class Generator>
  void DoUnaryOpWithFeedback(InterpreterAssembler* assembler);

  // Generates code to perform the comparison operation associated with
  // |compare_op|.
  void DoCompareOp(Token::Value compare_op, InterpreterAssembler* assembler);

  // Generates code to perform a global store via |ic|.
  void DoStaGlobal(Callable ic, InterpreterAssembler* assembler);

  // Generates code to perform a named property store via |ic|.
  void DoStoreIC(Callable ic, InterpreterAssembler* assembler);

  // Generates code to perform a keyed property store via |ic|.
  void DoKeyedStoreIC(Callable ic, InterpreterAssembler* assembler);

  // Generates code to perform a JS call that collects type feedback.
  void DoJSCall(InterpreterAssembler* assembler, TailCallMode tail_call_mode);

  // Generates code to perform a runtime call.
  void DoCallRuntimeCommon(InterpreterAssembler* assembler);

  // Generates code to perform a runtime call returning a pair.
  void DoCallRuntimeForPairCommon(InterpreterAssembler* assembler);

  // Generates code to perform a JS runtime call.
  void DoCallJSRuntimeCommon(InterpreterAssembler* assembler);

  // Generates code to perform a constructor call.
  void DoCallConstruct(InterpreterAssembler* assembler);

  // Generates code to perform delete via function_id.
  void DoDelete(Runtime::FunctionId function_id,
                InterpreterAssembler* assembler);

  // Generates code to perform a lookup slot load via |function_id|.
  void DoLdaLookupSlot(Runtime::FunctionId function_id,
                       InterpreterAssembler* assembler);

  // Generates code to perform a lookup slot store depending on |language_mode|.
  void DoStaLookupSlot(LanguageMode language_mode,
                       InterpreterAssembler* assembler);

  // Generates a node with the undefined constant.
  compiler::Node* BuildLoadUndefined(InterpreterAssembler* assembler);

  // Generates code to load a context slot.
  compiler::Node* BuildLoadContextSlot(InterpreterAssembler* assembler);

  // Generates code to load a global.
  compiler::Node* BuildLoadGlobal(Callable ic, InterpreterAssembler* assembler);

  // Generates code to load a named property.
  compiler::Node* BuildLoadNamedProperty(Callable ic,
                                         InterpreterAssembler* assembler);

  // Generates code to load a keyed property.
  compiler::Node* BuildLoadKeyedProperty(Callable ic,
                                         InterpreterAssembler* assembler);

  // Generates code to prepare the result for ForInPrepare. Cache data
  // are placed into the consecutive series of registers starting at
  // |output_register|.
  void BuildForInPrepareResult(compiler::Node* output_register,
                               compiler::Node* cache_type,
                               compiler::Node* cache_array,
                               compiler::Node* cache_length,
                               InterpreterAssembler* assembler);

  // Generates code to perform the unary operation via |callable|.
  compiler::Node* BuildUnaryOp(Callable callable,
                               InterpreterAssembler* assembler);

  uintptr_t GetDispatchCounter(Bytecode from, Bytecode to) const;

  // Get dispatch table index of bytecode.
  static size_t GetDispatchTableIndex(Bytecode bytecode,
                                      OperandScale operand_scale);

  bool IsDispatchTableInitialized();

  static const int kNumberOfWideVariants = 3;
  static const int kDispatchTableSize = kNumberOfWideVariants * (kMaxUInt8 + 1);
  static const int kNumberOfBytecodes = static_cast<int>(Bytecode::kLast) + 1;

  Isolate* isolate_;
  Address dispatch_table_[kDispatchTableSize];
  std::unique_ptr<uintptr_t[]> bytecode_dispatch_counters_table_;

  DISALLOW_COPY_AND_ASSIGN(Interpreter);
};

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

#endif  // V8_INTERPRETER_INTERPRETER_H_
