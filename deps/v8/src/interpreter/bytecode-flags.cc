// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/interpreter/bytecode-flags.h"

#include "src/ast/ast-value-factory.h"
#include "src/ast/ast.h"
#include "src/builtins/builtins-constructor.h"
#include "src/code-stubs.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {
namespace interpreter {

// static
uint8_t CreateArrayLiteralFlags::Encode(bool use_fast_shallow_clone,
                                        int runtime_flags) {
  uint8_t result = FlagsBits::encode(runtime_flags);
  result |= FastShallowCloneBit::encode(use_fast_shallow_clone);
  return result;
}

// static
uint8_t CreateObjectLiteralFlags::Encode(bool fast_clone_supported,
                                         int properties_count,
                                         int runtime_flags) {
  uint8_t result = FlagsBits::encode(runtime_flags);
  if (fast_clone_supported) {
    STATIC_ASSERT(
        ConstructorBuiltins::kMaximumClonedShallowObjectProperties <=
        1 << CreateObjectLiteralFlags::FastClonePropertiesCountBits::kShift);
    DCHECK_LE(properties_count,
              ConstructorBuiltins::kMaximumClonedShallowObjectProperties);
    result |= CreateObjectLiteralFlags::FastClonePropertiesCountBits::encode(
        properties_count);
  }
  return result;
}

// static
uint8_t CreateClosureFlags::Encode(bool pretenure, bool is_function_scope) {
  uint8_t result = PretenuredBit::encode(pretenure);
  if (!FLAG_always_opt && !FLAG_prepare_always_opt &&
      pretenure == NOT_TENURED && is_function_scope) {
    result |= FastNewClosureBit::encode(true);
  }
  return result;
}

// static
TestTypeOfFlags::LiteralFlag TestTypeOfFlags::GetFlagForLiteral(
    const AstStringConstants* ast_constants, Literal* literal) {
  const AstRawString* raw_literal = literal->raw_value()->AsString();
  if (raw_literal == ast_constants->number_string()) {
    return LiteralFlag::kNumber;
  } else if (raw_literal == ast_constants->string_string()) {
    return LiteralFlag::kString;
  } else if (raw_literal == ast_constants->symbol_string()) {
    return LiteralFlag::kSymbol;
  } else if (raw_literal == ast_constants->boolean_string()) {
    return LiteralFlag::kBoolean;
  } else if (raw_literal == ast_constants->undefined_string()) {
    return LiteralFlag::kUndefined;
  } else if (raw_literal == ast_constants->function_string()) {
    return LiteralFlag::kFunction;
  } else if (raw_literal == ast_constants->object_string()) {
    return LiteralFlag::kObject;
  } else {
    return LiteralFlag::kOther;
  }
}

// static
uint8_t TestTypeOfFlags::Encode(LiteralFlag literal_flag) {
  return static_cast<uint8_t>(literal_flag);
}

// static
TestTypeOfFlags::LiteralFlag TestTypeOfFlags::Decode(uint8_t raw_flag) {
  DCHECK_LE(raw_flag, static_cast<uint8_t>(LiteralFlag::kOther));
  return static_cast<LiteralFlag>(raw_flag);
}

// static
uint8_t SuspendGeneratorBytecodeFlags::Encode(SuspendFlags flags) {
  return FlagsBits::encode(flags);
}

// static
SuspendFlags SuspendGeneratorBytecodeFlags::Decode(uint8_t flags) {
  return FlagsBits::decode(flags);
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
