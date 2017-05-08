// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SRC_IC_ACCESSOR_ASSEMBLER_H_
#define V8_SRC_IC_ACCESSOR_ASSEMBLER_H_

#include "src/code-stub-assembler.h"

namespace v8 {
namespace internal {

namespace compiler {
class CodeAssemblerState;
}

class ExitPoint;

class AccessorAssembler : public CodeStubAssembler {
 public:
  typedef compiler::Node Node;

  explicit AccessorAssembler(compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

  void GenerateLoadIC();
  void GenerateLoadField();
  void GenerateLoadICTrampoline();
  void GenerateKeyedLoadIC();
  void GenerateKeyedLoadICTrampoline();
  void GenerateKeyedLoadIC_Megamorphic();
  void GenerateStoreIC();
  void GenerateStoreICTrampoline();

  void GenerateLoadICProtoArray(bool throw_reference_error_if_nonexistent);

  void GenerateLoadGlobalIC(TypeofMode typeof_mode);
  void GenerateLoadGlobalICTrampoline(TypeofMode typeof_mode);

  void GenerateKeyedStoreIC(LanguageMode language_mode);
  void GenerateKeyedStoreICTrampoline(LanguageMode language_mode);

  void TryProbeStubCache(StubCache* stub_cache, Node* receiver, Node* name,
                         Label* if_handler, Variable* var_handler,
                         Label* if_miss);

  Node* StubCachePrimaryOffsetForTesting(Node* name, Node* map) {
    return StubCachePrimaryOffset(name, map);
  }
  Node* StubCacheSecondaryOffsetForTesting(Node* name, Node* map) {
    return StubCacheSecondaryOffset(name, map);
  }

  struct LoadICParameters {
    LoadICParameters(Node* context, Node* receiver, Node* name, Node* slot,
                     Node* vector)
        : context(context),
          receiver(receiver),
          name(name),
          slot(slot),
          vector(vector) {}

    Node* context;
    Node* receiver;
    Node* name;
    Node* slot;
    Node* vector;
  };

  void LoadGlobalIC_TryPropertyCellCase(
      Node* vector, Node* slot, ExitPoint* exit_point, Label* try_handler,
      Label* miss, ParameterMode slot_mode = SMI_PARAMETERS);
  void LoadGlobalIC_TryHandlerCase(const LoadICParameters* p,
                                   TypeofMode typeof_mode,
                                   ExitPoint* exit_point, Label* miss);
  void LoadGlobalIC_MissCase(const LoadICParameters* p, ExitPoint* exit_point);

 protected:
  struct StoreICParameters : public LoadICParameters {
    StoreICParameters(Node* context, Node* receiver, Node* name, Node* value,
                      Node* slot, Node* vector)
        : LoadICParameters(context, receiver, name, slot, vector),
          value(value) {}
    Node* value;
  };

  enum ElementSupport { kOnlyProperties, kSupportElements };
  void HandleStoreICHandlerCase(
      const StoreICParameters* p, Node* handler, Label* miss,
      ElementSupport support_elements = kOnlyProperties);

 private:
  // Stub generation entry points.

  void LoadIC(const LoadICParameters* p);
  void LoadICProtoArray(const LoadICParameters* p, Node* handler,
                        bool throw_reference_error_if_nonexistent);
  void LoadGlobalIC(const LoadICParameters* p, TypeofMode typeof_mode);
  void KeyedLoadIC(const LoadICParameters* p);
  void KeyedLoadICGeneric(const LoadICParameters* p);
  void StoreIC(const StoreICParameters* p);
  void KeyedStoreIC(const StoreICParameters* p, LanguageMode language_mode);

  // IC dispatcher behavior.

  // Checks monomorphic case. Returns {feedback} entry of the vector.
  Node* TryMonomorphicCase(Node* slot, Node* vector, Node* receiver_map,
                           Label* if_handler, Variable* var_handler,
                           Label* if_miss);
  void HandlePolymorphicCase(Node* receiver_map, Node* feedback,
                             Label* if_handler, Variable* var_handler,
                             Label* if_miss, int unroll_count);
  void HandleKeyedStorePolymorphicCase(Node* receiver_map, Node* feedback,
                                       Label* if_handler, Variable* var_handler,
                                       Label* if_transition_handler,
                                       Variable* var_transition_map_cell,
                                       Label* if_miss);

  // LoadIC implementation.

  void HandleLoadICHandlerCase(
      const LoadICParameters* p, Node* handler, Label* miss,
      ElementSupport support_elements = kOnlyProperties);

  void HandleLoadICSmiHandlerCase(const LoadICParameters* p, Node* holder,
                                  Node* smi_handler, Label* miss,
                                  ExitPoint* exit_point,
                                  ElementSupport support_elements);

  void HandleLoadICProtoHandlerCase(const LoadICParameters* p, Node* handler,
                                    Variable* var_holder,
                                    Variable* var_smi_handler,
                                    Label* if_smi_handler, Label* miss,
                                    ExitPoint* exit_point,
                                    bool throw_reference_error_if_nonexistent);

  Node* EmitLoadICProtoArrayCheck(const LoadICParameters* p, Node* handler,
                                  Node* handler_length, Node* handler_flags,
                                  Label* miss,
                                  bool throw_reference_error_if_nonexistent);

  // LoadGlobalIC implementation.

  void HandleLoadGlobalICHandlerCase(const LoadICParameters* p, Node* handler,
                                     Label* miss, ExitPoint* exit_point,
                                     bool throw_reference_error_if_nonexistent);

  // StoreIC implementation.

  void HandleStoreICElementHandlerCase(const StoreICParameters* p,
                                       Node* handler, Label* miss);

  void HandleStoreICProtoHandler(const StoreICParameters* p, Node* handler,
                                 Label* miss);
  // If |transition| is nullptr then the normal field store is generated or
  // transitioning store otherwise.
  void HandleStoreICSmiHandlerCase(Node* handler_word, Node* holder,
                                   Node* value, Node* transition, Label* miss);
  // If |transition| is nullptr then the normal field store is generated or
  // transitioning store otherwise.
  void HandleStoreFieldAndReturn(Node* handler_word, Node* holder,
                                 Representation representation, Node* value,
                                 Node* transition, Label* miss);

  // KeyedLoadIC_Generic implementation.

  void GenericElementLoad(Node* receiver, Node* receiver_map,
                          Node* instance_type, Node* index, Label* slow);

  void GenericPropertyLoad(Node* receiver, Node* receiver_map,
                           Node* instance_type, Node* key,
                           const LoadICParameters* p, Label* slow);

  // Low-level helpers.

  Node* PrepareValueForStore(Node* handler_word, Node* holder,
                             Representation representation, Node* transition,
                             Node* value, Label* bailout);

  // Extends properties backing store by JSObject::kFieldsAdded elements.
  void ExtendPropertiesBackingStore(Node* object);

  void StoreNamedField(Node* handler_word, Node* object, bool is_inobject,
                       Representation representation, Node* value,
                       bool transition_to_field, Label* bailout);

  void EmitFastElementsBoundsCheck(Node* object, Node* elements,
                                   Node* intptr_index,
                                   Node* is_jsarray_condition, Label* miss);
  void EmitElementLoad(Node* object, Node* elements, Node* elements_kind,
                       Node* key, Node* is_jsarray_condition, Label* if_hole,
                       Label* rebox_double, Variable* var_double_value,
                       Label* unimplemented_elements_kind, Label* out_of_bounds,
                       Label* miss, ExitPoint* exit_point);
  void CheckPrototype(Node* prototype_cell, Node* name, Label* miss);
  void NameDictionaryNegativeLookup(Node* object, Node* name, Label* miss);

  // Stub cache access helpers.

  // This enum is used here as a replacement for StubCache::Table to avoid
  // including stub cache header.
  enum StubCacheTable : int;

  Node* StubCachePrimaryOffset(Node* name, Node* map);
  Node* StubCacheSecondaryOffset(Node* name, Node* seed);

  void TryProbeStubCacheTable(StubCache* stub_cache, StubCacheTable table_id,
                              Node* entry_offset, Node* name, Node* map,
                              Label* if_handler, Variable* var_handler,
                              Label* if_miss);
};

// Abstraction over direct and indirect exit points. Direct exits correspond to
// tailcalls and Return, while indirect exits store the result in a variable
// and then jump to an exit label.
class ExitPoint {
 private:
  typedef compiler::Node Node;
  typedef compiler::CodeAssemblerLabel CodeAssemblerLabel;
  typedef compiler::CodeAssemblerVariable CodeAssemblerVariable;

 public:
  explicit ExitPoint(CodeStubAssembler* assembler)
      : ExitPoint(assembler, nullptr, nullptr) {}
  ExitPoint(CodeStubAssembler* assembler, CodeAssemblerLabel* out,
            CodeAssemblerVariable* var_result)
      : out_(out), var_result_(var_result), asm_(assembler) {
    DCHECK_EQ(out != nullptr, var_result != nullptr);
  }

  template <class... TArgs>
  void ReturnCallRuntime(Runtime::FunctionId function, Node* context,
                         TArgs... args) {
    if (IsDirect()) {
      asm_->TailCallRuntime(function, context, args...);
    } else {
      IndirectReturn(asm_->CallRuntime(function, context, args...));
    }
  }

  template <class... TArgs>
  void ReturnCallStub(Callable const& callable, Node* context, TArgs... args) {
    if (IsDirect()) {
      asm_->TailCallStub(callable, context, args...);
    } else {
      IndirectReturn(asm_->CallStub(callable, context, args...));
    }
  }

  template <class... TArgs>
  void ReturnCallStub(const CallInterfaceDescriptor& descriptor, Node* target,
                      Node* context, TArgs... args) {
    if (IsDirect()) {
      asm_->TailCallStub(descriptor, target, context, args...);
    } else {
      IndirectReturn(asm_->CallStub(descriptor, target, context, args...));
    }
  }

  void Return(Node* const result) {
    if (IsDirect()) {
      asm_->Return(result);
    } else {
      IndirectReturn(result);
    }
  }

  bool IsDirect() const { return out_ == nullptr; }

 private:
  void IndirectReturn(Node* const result) {
    var_result_->Bind(result);
    asm_->Goto(out_);
  }

  CodeAssemblerLabel* const out_;
  CodeAssemblerVariable* const var_result_;
  CodeStubAssembler* const asm_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_SRC_IC_ACCESSOR_ASSEMBLER_H_
