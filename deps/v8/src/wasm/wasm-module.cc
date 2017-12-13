// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <functional>
#include <memory>

#include "src/api.h"
#include "src/assembler-inl.h"
#include "src/code-stubs.h"
#include "src/debug/interface-types.h"
#include "src/frames-inl.h"
#include "src/objects.h"
#include "src/property-descriptor.h"
#include "src/simulator.h"
#include "src/snapshot/snapshot.h"
#include "src/v8.h"

#include "src/compiler/wasm-compiler.h"
#include "src/wasm/compilation-manager.h"
#include "src/wasm/module-compiler.h"
#include "src/wasm/module-decoder.h"
#include "src/wasm/wasm-code-specialization.h"
#include "src/wasm/wasm-js.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects-inl.h"
#include "src/wasm/wasm-result.h"

namespace v8 {
namespace internal {
namespace wasm {

#define TRACE(...)                                      \
  do {                                                  \
    if (FLAG_trace_wasm_instances) PrintF(__VA_ARGS__); \
  } while (false)

#define TRACE_CHAIN(instance)        \
  do {                               \
    instance->PrintInstancesChain(); \
  } while (false)

#define TRACE_COMPILE(...)                             \
  do {                                                 \
    if (FLAG_trace_wasm_compiler) PrintF(__VA_ARGS__); \
  } while (false)

// static
const WasmExceptionSig WasmException::empty_sig_(0, 0, nullptr);

// static
constexpr const char* WasmException::kRuntimeIdStr;

// static
constexpr const char* WasmException::kRuntimeValuesStr;

void UnpackAndRegisterProtectedInstructions(Isolate* isolate,
                                            Handle<FixedArray> code_table) {
  DisallowHeapAllocation no_gc;
  std::vector<trap_handler::ProtectedInstructionData> unpacked;

  for (int i = 0; i < code_table->length(); ++i) {
    Object* maybe_code = code_table->get(i);
    // This is sometimes undefined when we're called from cctests.
    if (maybe_code->IsUndefined(isolate)) continue;
    Code* code = Code::cast(maybe_code);

    if (code->kind() != Code::WASM_FUNCTION) {
      continue;
    }

    if (code->trap_handler_index()->value() != trap_handler::kInvalidIndex) {
      // This function has already been registered.
      continue;
    }

    byte* base = code->entry();

    const int mode_mask =
        RelocInfo::ModeMask(RelocInfo::WASM_PROTECTED_INSTRUCTION_LANDING);
    for (RelocIterator it(code, mode_mask); !it.done(); it.next()) {
      trap_handler::ProtectedInstructionData data;
      data.instr_offset = static_cast<uint32_t>(it.rinfo()->data());
      data.landing_offset = static_cast<uint32_t>(it.rinfo()->pc() - base);
      // Check that now over-/underflow happened.
      DCHECK_EQ(it.rinfo()->data(), data.instr_offset);
      DCHECK_EQ(it.rinfo()->pc() - base, data.landing_offset);
      unpacked.emplace_back(data);
    }
    if (unpacked.empty()) continue;

    int size = code->CodeSize();
    const int index = RegisterHandlerData(reinterpret_cast<void*>(base), size,
                                          unpacked.size(), &unpacked[0]);
    unpacked.clear();
    // TODO(eholk): if index is negative, fail.
    DCHECK_LE(0, index);
    code->set_trap_handler_index(Smi::FromInt(index));
  }
}

std::ostream& operator<<(std::ostream& os, const WasmFunctionName& name) {
  os << "#" << name.function_->func_index;
  if (name.function_->name.is_set()) {
    if (name.name_.start()) {
      os << ":";
      os.write(name.name_.start(), name.name_.length());
    }
  } else {
    os << "?";
  }
  return os;
}

WasmModule::WasmModule(std::unique_ptr<Zone> owned)
    : signature_zone(std::move(owned)) {}

WasmFunction* GetWasmFunctionForExport(Isolate* isolate,
                                       Handle<Object> target) {
  if (target->IsJSFunction()) {
    Handle<JSFunction> func = Handle<JSFunction>::cast(target);
    if (func->code()->kind() == Code::JS_TO_WASM_FUNCTION) {
      auto exported = Handle<WasmExportedFunction>::cast(func);
      Handle<WasmInstanceObject> other_instance(exported->instance(), isolate);
      int func_index = exported->function_index();
      return &other_instance->module()->functions[func_index];
    }
  }
  return nullptr;
}

Handle<Code> UnwrapExportWrapper(Handle<JSFunction> export_wrapper) {
  Handle<Code> export_wrapper_code = handle(export_wrapper->code());
  DCHECK_EQ(export_wrapper_code->kind(), Code::JS_TO_WASM_FUNCTION);
  int mask = RelocInfo::ModeMask(RelocInfo::CODE_TARGET);
  for (RelocIterator it(*export_wrapper_code, mask);; it.next()) {
    DCHECK(!it.done());
    Code* target = Code::GetCodeFromTargetAddress(it.rinfo()->target_address());
    if (target->kind() != Code::WASM_FUNCTION &&
        target->kind() != Code::WASM_TO_JS_FUNCTION &&
        target->kind() != Code::WASM_INTERPRETER_ENTRY)
      continue;
// There should only be this one call to wasm code.
#ifdef DEBUG
    for (it.next(); !it.done(); it.next()) {
      Code* code = Code::GetCodeFromTargetAddress(it.rinfo()->target_address());
      DCHECK(code->kind() != Code::WASM_FUNCTION &&
             code->kind() != Code::WASM_TO_JS_FUNCTION &&
             code->kind() != Code::WASM_INTERPRETER_ENTRY);
    }
#endif
    return handle(target);
  }
  UNREACHABLE();
}

void UpdateDispatchTables(Isolate* isolate, Handle<FixedArray> dispatch_tables,
                          int index, WasmFunction* function,
                          Handle<Code> code) {
  DCHECK_EQ(0, dispatch_tables->length() % 4);
  for (int i = 0; i < dispatch_tables->length(); i += 4) {
    int table_index = Smi::ToInt(dispatch_tables->get(i + 1));
    Handle<FixedArray> function_table(
        FixedArray::cast(dispatch_tables->get(i + 2)), isolate);
    Handle<FixedArray> signature_table(
        FixedArray::cast(dispatch_tables->get(i + 3)), isolate);
    if (function) {
      // TODO(titzer): the signature might need to be copied to avoid
      // a dangling pointer in the signature map.
      Handle<WasmInstanceObject> instance(
          WasmInstanceObject::cast(dispatch_tables->get(i)), isolate);
      auto& func_table = instance->module()->function_tables[table_index];
      uint32_t sig_index = func_table.map.FindOrInsert(function->sig);
      signature_table->set(index, Smi::FromInt(static_cast<int>(sig_index)));
      function_table->set(index, *code);
    } else {
      signature_table->set(index, Smi::FromInt(-1));
      function_table->set(index, Smi::kZero);
    }
  }
}

bool IsWasmCodegenAllowed(Isolate* isolate, Handle<Context> context) {
  // TODO(wasm): Once wasm has its own CSP policy, we should introduce a
  // separate callback that includes information about the module about to be
  // compiled. For the time being, pass an empty string as placeholder for the
  // sources.
  return isolate->allow_code_gen_callback() == nullptr ||
         isolate->allow_code_gen_callback()(
             v8::Utils::ToLocal(context),
             v8::Utils::ToLocal(isolate->factory()->empty_string()));
}

Handle<JSArray> GetImports(Isolate* isolate,
                           Handle<WasmModuleObject> module_object) {
  Handle<WasmCompiledModule> compiled_module(module_object->compiled_module(),
                                             isolate);
  Factory* factory = isolate->factory();

  Handle<String> module_string = factory->InternalizeUtf8String("module");
  Handle<String> name_string = factory->InternalizeUtf8String("name");
  Handle<String> kind_string = factory->InternalizeUtf8String("kind");

  Handle<String> function_string = factory->InternalizeUtf8String("function");
  Handle<String> table_string = factory->InternalizeUtf8String("table");
  Handle<String> memory_string = factory->InternalizeUtf8String("memory");
  Handle<String> global_string = factory->InternalizeUtf8String("global");

  // Create the result array.
  WasmModule* module = compiled_module->module();
  int num_imports = static_cast<int>(module->import_table.size());
  Handle<JSArray> array_object = factory->NewJSArray(PACKED_ELEMENTS, 0, 0);
  Handle<FixedArray> storage = factory->NewFixedArray(num_imports);
  JSArray::SetContent(array_object, storage);
  array_object->set_length(Smi::FromInt(num_imports));

  Handle<JSFunction> object_function =
      Handle<JSFunction>(isolate->native_context()->object_function(), isolate);

  // Populate the result array.
  for (int index = 0; index < num_imports; ++index) {
    WasmImport& import = module->import_table[index];

    Handle<JSObject> entry = factory->NewJSObject(object_function);

    Handle<String> import_kind;
    switch (import.kind) {
      case kExternalFunction:
        import_kind = function_string;
        break;
      case kExternalTable:
        import_kind = table_string;
        break;
      case kExternalMemory:
        import_kind = memory_string;
        break;
      case kExternalGlobal:
        import_kind = global_string;
        break;
      default:
        UNREACHABLE();
    }

    MaybeHandle<String> import_module =
        WasmCompiledModule::ExtractUtf8StringFromModuleBytes(
            isolate, compiled_module, import.module_name);

    MaybeHandle<String> import_name =
        WasmCompiledModule::ExtractUtf8StringFromModuleBytes(
            isolate, compiled_module, import.field_name);

    JSObject::AddProperty(entry, module_string, import_module.ToHandleChecked(),
                          NONE);
    JSObject::AddProperty(entry, name_string, import_name.ToHandleChecked(),
                          NONE);
    JSObject::AddProperty(entry, kind_string, import_kind, NONE);

    storage->set(index, *entry);
  }

  return array_object;
}

Handle<JSArray> GetExports(Isolate* isolate,
                           Handle<WasmModuleObject> module_object) {
  Handle<WasmCompiledModule> compiled_module(module_object->compiled_module(),
                                             isolate);
  Factory* factory = isolate->factory();

  Handle<String> name_string = factory->InternalizeUtf8String("name");
  Handle<String> kind_string = factory->InternalizeUtf8String("kind");

  Handle<String> function_string = factory->InternalizeUtf8String("function");
  Handle<String> table_string = factory->InternalizeUtf8String("table");
  Handle<String> memory_string = factory->InternalizeUtf8String("memory");
  Handle<String> global_string = factory->InternalizeUtf8String("global");

  // Create the result array.
  WasmModule* module = compiled_module->module();
  int num_exports = static_cast<int>(module->export_table.size());
  Handle<JSArray> array_object = factory->NewJSArray(PACKED_ELEMENTS, 0, 0);
  Handle<FixedArray> storage = factory->NewFixedArray(num_exports);
  JSArray::SetContent(array_object, storage);
  array_object->set_length(Smi::FromInt(num_exports));

  Handle<JSFunction> object_function =
      Handle<JSFunction>(isolate->native_context()->object_function(), isolate);

  // Populate the result array.
  for (int index = 0; index < num_exports; ++index) {
    WasmExport& exp = module->export_table[index];

    Handle<String> export_kind;
    switch (exp.kind) {
      case kExternalFunction:
        export_kind = function_string;
        break;
      case kExternalTable:
        export_kind = table_string;
        break;
      case kExternalMemory:
        export_kind = memory_string;
        break;
      case kExternalGlobal:
        export_kind = global_string;
        break;
      default:
        UNREACHABLE();
    }

    Handle<JSObject> entry = factory->NewJSObject(object_function);

    MaybeHandle<String> export_name =
        WasmCompiledModule::ExtractUtf8StringFromModuleBytes(
            isolate, compiled_module, exp.name);

    JSObject::AddProperty(entry, name_string, export_name.ToHandleChecked(),
                          NONE);
    JSObject::AddProperty(entry, kind_string, export_kind, NONE);

    storage->set(index, *entry);
  }

  return array_object;
}

Handle<JSArray> GetCustomSections(Isolate* isolate,
                                  Handle<WasmModuleObject> module_object,
                                  Handle<String> name, ErrorThrower* thrower) {
  Handle<WasmCompiledModule> compiled_module(module_object->compiled_module(),
                                             isolate);
  Factory* factory = isolate->factory();

  std::vector<CustomSectionOffset> custom_sections;
  {
    DisallowHeapAllocation no_gc;  // for raw access to string bytes.
    Handle<SeqOneByteString> module_bytes(compiled_module->module_bytes(),
                                          isolate);
    const byte* start =
        reinterpret_cast<const byte*>(module_bytes->GetCharsAddress());
    const byte* end = start + module_bytes->length();
    custom_sections = DecodeCustomSections(start, end);
  }

  std::vector<Handle<Object>> matching_sections;

  // Gather matching sections.
  for (auto& section : custom_sections) {
    MaybeHandle<String> section_name =
        WasmCompiledModule::ExtractUtf8StringFromModuleBytes(
            isolate, compiled_module, section.name);

    if (!name->Equals(*section_name.ToHandleChecked())) continue;

    // Make a copy of the payload data in the section.
    size_t size = section.payload.length();
    void* memory =
        size == 0 ? nullptr : isolate->array_buffer_allocator()->Allocate(size);

    if (size && !memory) {
      thrower->RangeError("out of memory allocating custom section data");
      return Handle<JSArray>();
    }
    Handle<JSArrayBuffer> buffer = isolate->factory()->NewJSArrayBuffer();
    constexpr bool is_external = false;
    JSArrayBuffer::Setup(buffer, isolate, is_external, memory, size, memory,
                         size);
    DisallowHeapAllocation no_gc;  // for raw access to string bytes.
    Handle<SeqOneByteString> module_bytes(compiled_module->module_bytes(),
                                          isolate);
    const byte* start =
        reinterpret_cast<const byte*>(module_bytes->GetCharsAddress());
    memcpy(memory, start + section.payload.offset(), section.payload.length());

    matching_sections.push_back(buffer);
  }

  int num_custom_sections = static_cast<int>(matching_sections.size());
  Handle<JSArray> array_object = factory->NewJSArray(PACKED_ELEMENTS, 0, 0);
  Handle<FixedArray> storage = factory->NewFixedArray(num_custom_sections);
  JSArray::SetContent(array_object, storage);
  array_object->set_length(Smi::FromInt(num_custom_sections));

  for (int i = 0; i < num_custom_sections; i++) {
    storage->set(i, *matching_sections[i]);
  }

  return array_object;
}

Handle<FixedArray> DecodeLocalNames(
    Isolate* isolate, Handle<WasmCompiledModule> compiled_module) {
  Handle<SeqOneByteString> wire_bytes(compiled_module->module_bytes(), isolate);
  LocalNames decoded_locals;
  {
    DisallowHeapAllocation no_gc;
    DecodeLocalNames(wire_bytes->GetChars(),
                     wire_bytes->GetChars() + wire_bytes->length(),
                     &decoded_locals);
  }
  Handle<FixedArray> locals_names =
      isolate->factory()->NewFixedArray(decoded_locals.max_function_index + 1);
  for (LocalNamesPerFunction& func : decoded_locals.names) {
    Handle<FixedArray> func_locals_names =
        isolate->factory()->NewFixedArray(func.max_local_index + 1);
    locals_names->set(func.function_index, *func_locals_names);
    for (LocalName& name : func.names) {
      Handle<String> name_str =
          WasmCompiledModule::ExtractUtf8StringFromModuleBytes(
              isolate, compiled_module, name.name)
              .ToHandleChecked();
      func_locals_names->set(name.local_index, *name_str);
    }
  }
  return locals_names;
}

const char* ExternalKindName(WasmExternalKind kind) {
  switch (kind) {
    case kExternalFunction:
      return "function";
    case kExternalTable:
      return "table";
    case kExternalMemory:
      return "memory";
    case kExternalGlobal:
      return "global";
  }
  return "unknown";
}

#undef TRACE
#undef TRACE_CHAIN
#undef TRACE_COMPILE

}  // namespace wasm
}  // namespace internal
}  // namespace v8
