// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils-gen.h"
#include "src/builtins/builtins.h"
#include "src/code-stub-assembler.h"
#include "src/ic/handler-compiler.h"
#include "src/ic/ic.h"
#include "src/ic/keyed-store-generic.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

TF_BUILTIN(KeyedLoadIC_IndexedString, CodeStubAssembler) {
  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* index = Parameter(Descriptor::kName);
  Node* slot = Parameter(Descriptor::kSlot);
  Node* vector = Parameter(Descriptor::kVector);
  Node* context = Parameter(Descriptor::kContext);

  Label miss(this);

  Node* index_intptr = TryToIntptr(index, &miss);
  Node* length = SmiUntag(LoadStringLength(receiver));
  GotoIf(UintPtrGreaterThanOrEqual(index_intptr, length), &miss);

  Node* code = StringCharCodeAt(receiver, index_intptr, INTPTR_PARAMETERS);
  Node* result = StringFromCharCode(code);
  Return(result);

  BIND(&miss);
  TailCallRuntime(Runtime::kKeyedLoadIC_Miss, context, receiver, index, slot,
                  vector);
}

TF_BUILTIN(KeyedLoadIC_Miss, CodeStubAssembler) {
  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* name = Parameter(Descriptor::kName);
  Node* slot = Parameter(Descriptor::kSlot);
  Node* vector = Parameter(Descriptor::kVector);
  Node* context = Parameter(Descriptor::kContext);

  TailCallRuntime(Runtime::kKeyedLoadIC_Miss, context, receiver, name, slot,
                  vector);
}

TF_BUILTIN(KeyedLoadIC_Slow, CodeStubAssembler) {
  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* name = Parameter(Descriptor::kName);
  Node* context = Parameter(Descriptor::kContext);

  TailCallRuntime(Runtime::kKeyedGetProperty, context, receiver, name);
}

void Builtins::Generate_KeyedStoreIC_Megamorphic(
    compiler::CodeAssemblerState* state) {
  KeyedStoreGenericGenerator::Generate(state, SLOPPY);
}

void Builtins::Generate_KeyedStoreIC_Megamorphic_Strict(
    compiler::CodeAssemblerState* state) {
  KeyedStoreGenericGenerator::Generate(state, STRICT);
}

void Builtins::Generate_StoreIC_Uninitialized(
    compiler::CodeAssemblerState* state) {
  StoreICUninitializedGenerator::Generate(state, SLOPPY);
}

void Builtins::Generate_StoreICStrict_Uninitialized(
    compiler::CodeAssemblerState* state) {
  StoreICUninitializedGenerator::Generate(state, STRICT);
}

TF_BUILTIN(KeyedStoreIC_Miss, CodeStubAssembler) {
  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* name = Parameter(Descriptor::kName);
  Node* value = Parameter(Descriptor::kValue);
  Node* slot = Parameter(Descriptor::kSlot);
  Node* vector = Parameter(Descriptor::kVector);
  Node* context = Parameter(Descriptor::kContext);

  TailCallRuntime(Runtime::kKeyedStoreIC_Miss, context, value, slot, vector,
                  receiver, name);
}

TF_BUILTIN(KeyedStoreIC_Slow, CodeStubAssembler) {
  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* name = Parameter(Descriptor::kName);
  Node* value = Parameter(Descriptor::kValue);
  Node* slot = Parameter(Descriptor::kSlot);
  Node* vector = Parameter(Descriptor::kVector);
  Node* context = Parameter(Descriptor::kContext);

  // The slow case calls into the runtime to complete the store without causing
  // an IC miss that would otherwise cause a transition to the generic stub.
  TailCallRuntime(Runtime::kKeyedStoreIC_Slow, context, value, slot, vector,
                  receiver, name);
}

TF_BUILTIN(LoadGlobalIC_Miss, CodeStubAssembler) {
  Node* name = Parameter(Descriptor::kName);
  Node* slot = Parameter(Descriptor::kSlot);
  Node* vector = Parameter(Descriptor::kVector);
  Node* context = Parameter(Descriptor::kContext);

  TailCallRuntime(Runtime::kLoadGlobalIC_Miss, context, name, slot, vector);
}

TF_BUILTIN(LoadGlobalIC_Slow, CodeStubAssembler) {
  Node* name = Parameter(Descriptor::kName);
  Node* slot = Parameter(Descriptor::kSlot);
  Node* vector = Parameter(Descriptor::kVector);
  Node* context = Parameter(Descriptor::kContext);

  TailCallRuntime(Runtime::kLoadGlobalIC_Slow, context, name, slot, vector);
}

void Builtins::Generate_LoadIC_Getter_ForDeopt(MacroAssembler* masm) {
  NamedLoadHandlerCompiler::GenerateLoadViaGetterForDeopt(masm);
}

TF_BUILTIN(LoadIC_FunctionPrototype, CodeStubAssembler) {
  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* name = Parameter(Descriptor::kName);
  Node* slot = Parameter(Descriptor::kSlot);
  Node* vector = Parameter(Descriptor::kVector);
  Node* context = Parameter(Descriptor::kContext);

  Label miss(this, Label::kDeferred);
  Return(LoadJSFunctionPrototype(receiver, &miss));

  BIND(&miss);
  TailCallRuntime(Runtime::kLoadIC_Miss, context, receiver, name, slot, vector);
}

TF_BUILTIN(LoadIC_Miss, CodeStubAssembler) {
  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* name = Parameter(Descriptor::kName);
  Node* slot = Parameter(Descriptor::kSlot);
  Node* vector = Parameter(Descriptor::kVector);
  Node* context = Parameter(Descriptor::kContext);

  TailCallRuntime(Runtime::kLoadIC_Miss, context, receiver, name, slot, vector);
}

TF_BUILTIN(LoadIC_Slow, CodeStubAssembler) {
  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* name = Parameter(Descriptor::kName);
  Node* context = Parameter(Descriptor::kContext);

  TailCallRuntime(Runtime::kGetProperty, context, receiver, name);
}

TF_BUILTIN(StoreIC_Miss, CodeStubAssembler) {
  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* name = Parameter(Descriptor::kName);
  Node* value = Parameter(Descriptor::kValue);
  Node* slot = Parameter(Descriptor::kSlot);
  Node* vector = Parameter(Descriptor::kVector);
  Node* context = Parameter(Descriptor::kContext);

  TailCallRuntime(Runtime::kStoreIC_Miss, context, value, slot, vector,
                  receiver, name);
}

void Builtins::Generate_StoreIC_Setter_ForDeopt(MacroAssembler* masm) {
  NamedStoreHandlerCompiler::GenerateStoreViaSetterForDeopt(masm);
}

}  // namespace internal
}  // namespace v8
