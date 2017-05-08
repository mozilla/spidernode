// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "src/assembler-inl.h"
#include "src/base/adapters.h"
#include "src/base/atomic-utils.h"
#include "src/code-stubs.h"
#include "src/compiler/wasm-compiler.h"
#include "src/debug/interface-types.h"
#include "src/objects.h"
#include "src/property-descriptor.h"
#include "src/simulator.h"
#include "src/snapshot/snapshot.h"
#include "src/v8.h"

#include "src/asmjs/asm-wasm-builder.h"
#include "src/wasm/function-body-decoder.h"
#include "src/wasm/module-decoder.h"
#include "src/wasm/wasm-code-specialization.h"
#include "src/wasm/wasm-js.h"
#include "src/wasm/wasm-limits.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects.h"
#include "src/wasm/wasm-result.h"

using namespace v8::internal;
using namespace v8::internal::wasm;
namespace base = v8::base;

#define TRACE(...)                                      \
  do {                                                  \
    if (FLAG_trace_wasm_instances) PrintF(__VA_ARGS__); \
  } while (false)

#define TRACE_CHAIN(instance)        \
  do {                               \
    instance->PrintInstancesChain(); \
  } while (false)

namespace {

static const int kInvalidSigIndex = -1;

byte* raw_buffer_ptr(MaybeHandle<JSArrayBuffer> buffer, int offset) {
  return static_cast<byte*>(buffer.ToHandleChecked()->backing_store()) + offset;
}

static void MemoryFinalizer(const v8::WeakCallbackInfo<void>& data) {
  DisallowHeapAllocation no_gc;
  JSArrayBuffer** p = reinterpret_cast<JSArrayBuffer**>(data.GetParameter());
  JSArrayBuffer* buffer = *p;

  if (!buffer->was_neutered()) {
    void* memory = buffer->backing_store();
    DCHECK(memory != nullptr);
    base::OS::Free(memory,
                   RoundUp(kWasmMaxHeapOffset, base::OS::CommitPageSize()));

    data.GetIsolate()->AdjustAmountOfExternalAllocatedMemory(
        -buffer->byte_length()->Number());
  }

  GlobalHandles::Destroy(reinterpret_cast<Object**>(p));
}

#if V8_TARGET_ARCH_64_BIT
const bool kGuardRegionsSupported = true;
#else
const bool kGuardRegionsSupported = false;
#endif

bool EnableGuardRegions() {
  return FLAG_wasm_guard_pages && kGuardRegionsSupported;
}

static void RecordStats(Isolate* isolate, Code* code) {
  isolate->counters()->wasm_generated_code_size()->Increment(code->body_size());
  isolate->counters()->wasm_reloc_size()->Increment(
      code->relocation_info()->length());
}

static void RecordStats(Isolate* isolate, Handle<FixedArray> functions) {
  DisallowHeapAllocation no_gc;
  for (int i = 0; i < functions->length(); ++i) {
    RecordStats(isolate, Code::cast(functions->get(i)));
  }
}

void* TryAllocateBackingStore(Isolate* isolate, size_t size,
                              bool enable_guard_regions, bool& is_external) {
  is_external = false;
  // TODO(eholk): Right now enable_guard_regions has no effect on 32-bit
  // systems. It may be safer to fail instead, given that other code might do
  // things that would be unsafe if they expected guard pages where there
  // weren't any.
  if (enable_guard_regions && kGuardRegionsSupported) {
    // TODO(eholk): On Windows we want to make sure we don't commit the guard
    // pages yet.

    // We always allocate the largest possible offset into the heap, so the
    // addressable memory after the guard page can be made inaccessible.
    const size_t alloc_size =
        RoundUp(kWasmMaxHeapOffset, base::OS::CommitPageSize());
    DCHECK_EQ(0, size % base::OS::CommitPageSize());

    // AllocateGuarded makes the whole region inaccessible by default.
    void* memory = base::OS::AllocateGuarded(alloc_size);
    if (memory == nullptr) {
      return nullptr;
    }

    // Make the part we care about accessible.
    base::OS::Unprotect(memory, size);

    reinterpret_cast<v8::Isolate*>(isolate)
        ->AdjustAmountOfExternalAllocatedMemory(size);

    is_external = true;
    return memory;
  } else {
    void* memory = isolate->array_buffer_allocator()->Allocate(size);
    return memory;
  }
}

void FlushICache(Isolate* isolate, Handle<FixedArray> code_table) {
  for (int i = 0; i < code_table->length(); ++i) {
    Handle<Code> code = code_table->GetValueChecked<Code>(isolate, i);
    Assembler::FlushICache(isolate, code->instruction_start(),
                           code->instruction_size());
  }
}

Handle<Script> CreateWasmScript(Isolate* isolate,
                                const ModuleWireBytes& wire_bytes) {
  Handle<Script> script =
      isolate->factory()->NewScript(isolate->factory()->empty_string());
  FixedArray* array = isolate->native_context()->embedder_data();
  script->set_context_data(array->get(v8::Context::kDebugIdIndex));
  script->set_type(Script::TYPE_WASM);

  int hash = StringHasher::HashSequentialString(
      reinterpret_cast<const char*>(wire_bytes.start()), wire_bytes.length(),
      kZeroHashSeed);

  const int kBufferSize = 32;
  char buffer[kBufferSize];
  int url_chars = SNPrintF(ArrayVector(buffer), "wasm://wasm/%08x", hash);
  DCHECK(url_chars >= 0 && url_chars < kBufferSize);
  MaybeHandle<String> url_str = isolate->factory()->NewStringFromOneByte(
      Vector<const uint8_t>(reinterpret_cast<uint8_t*>(buffer), url_chars),
      TENURED);
  script->set_source_url(*url_str.ToHandleChecked());

  int name_chars = SNPrintF(ArrayVector(buffer), "wasm-%08x", hash);
  DCHECK(name_chars >= 0 && name_chars < kBufferSize);
  MaybeHandle<String> name_str = isolate->factory()->NewStringFromOneByte(
      Vector<const uint8_t>(reinterpret_cast<uint8_t*>(buffer), name_chars),
      TENURED);
  script->set_name(*name_str.ToHandleChecked());

  return script;
}

class JSToWasmWrapperCache {
 public:
  Handle<Code> CloneOrCompileJSToWasmWrapper(Isolate* isolate,
                                             const wasm::WasmModule* module,
                                             Handle<Code> wasm_code,
                                             uint32_t index) {
    const wasm::WasmFunction* func = &module->functions[index];
    int cached_idx = sig_map_.Find(func->sig);
    if (cached_idx >= 0) {
      Handle<Code> code = isolate->factory()->CopyCode(code_cache_[cached_idx]);
      // Now patch the call to wasm code.
      for (RelocIterator it(*code, RelocInfo::kCodeTargetMask);; it.next()) {
        DCHECK(!it.done());
        Code* target =
            Code::GetCodeFromTargetAddress(it.rinfo()->target_address());
        if (target->kind() == Code::WASM_FUNCTION ||
            target->kind() == Code::WASM_TO_JS_FUNCTION ||
            target->builtin_index() == Builtins::kIllegal) {
          it.rinfo()->set_target_address(wasm_code->instruction_start());
          break;
        }
      }
      return code;
    }

    Handle<Code> code =
        compiler::CompileJSToWasmWrapper(isolate, module, wasm_code, index);
    uint32_t new_cache_idx = sig_map_.FindOrInsert(func->sig);
    DCHECK_EQ(code_cache_.size(), new_cache_idx);
    USE(new_cache_idx);
    code_cache_.push_back(code);
    return code;
  }

 private:
  // sig_map_ maps signatures to an index in code_cache_.
  wasm::SignatureMap sig_map_;
  std::vector<Handle<Code>> code_cache_;
};

// A helper for compiling an entire module.
class CompilationHelper {
 public:
  CompilationHelper(Isolate* isolate, WasmModule* module)
      : isolate_(isolate), module_(module) {}

  // The actual runnable task that performs compilations in the background.
  class CompilationTask : public CancelableTask {
   public:
    CompilationHelper* helper_;
    explicit CompilationTask(CompilationHelper* helper)
        : CancelableTask(helper->isolate_), helper_(helper) {}

    void RunInternal() override {
      while (helper_->FetchAndExecuteCompilationUnit()) {
      }
      helper_->module_->pending_tasks.get()->Signal();
    }
  };

  Isolate* isolate_;
  WasmModule* module_;
  std::vector<compiler::WasmCompilationUnit*> compilation_units_;
  std::queue<compiler::WasmCompilationUnit*> executed_units_;
  base::Mutex result_mutex_;
  base::AtomicNumber<size_t> next_unit_;

  // Run by each compilation task and by the main thread.
  bool FetchAndExecuteCompilationUnit() {
    DisallowHeapAllocation no_allocation;
    DisallowHandleAllocation no_handles;
    DisallowHandleDereference no_deref;
    DisallowCodeDependencyChange no_dependency_change;

    // - 1 because AtomicIncrement returns the value after the atomic increment.
    size_t index = next_unit_.Increment(1) - 1;
    if (index >= compilation_units_.size()) {
      return false;
    }

    compiler::WasmCompilationUnit* unit = compilation_units_.at(index);
    if (unit != nullptr) {
      unit->ExecuteCompilation();
      base::LockGuard<base::Mutex> guard(&result_mutex_);
      executed_units_.push(unit);
    }
    return true;
  }

  void InitializeParallelCompilation(const std::vector<WasmFunction>& functions,
                                     ModuleBytesEnv& module_env,
                                     ErrorThrower* thrower) {
    compilation_units_.reserve(functions.size());
    for (uint32_t i = FLAG_skip_compiling_wasm_funcs; i < functions.size();
         ++i) {
      const WasmFunction* func = &functions[i];
      compilation_units_.push_back(
          func->imported ? nullptr
                         : new compiler::WasmCompilationUnit(
                               thrower, isolate_, &module_env, func, i));
    }
  }

  uint32_t* StartCompilationTasks() {
    const size_t num_tasks =
        Min(static_cast<size_t>(FLAG_wasm_num_compilation_tasks),
            V8::GetCurrentPlatform()->NumberOfAvailableBackgroundThreads());
    uint32_t* task_ids = new uint32_t[num_tasks];
    for (size_t i = 0; i < num_tasks; ++i) {
      CompilationTask* task = new CompilationTask(this);
      task_ids[i] = task->id();
      V8::GetCurrentPlatform()->CallOnBackgroundThread(
          task, v8::Platform::kShortRunningTask);
    }
    return task_ids;
  }

  void WaitForCompilationTasks(uint32_t* task_ids) {
    const size_t num_tasks =
        Min(static_cast<size_t>(FLAG_wasm_num_compilation_tasks),
            V8::GetCurrentPlatform()->NumberOfAvailableBackgroundThreads());
    for (size_t i = 0; i < num_tasks; ++i) {
      // If the task has not started yet, then we abort it. Otherwise we wait
      // for
      // it to finish.
      if (isolate_->cancelable_task_manager()->TryAbort(task_ids[i]) !=
          CancelableTaskManager::kTaskAborted) {
        module_->pending_tasks.get()->Wait();
      }
    }
  }

  void FinishCompilationUnits(std::vector<Handle<Code>>& results) {
    while (true) {
      compiler::WasmCompilationUnit* unit = nullptr;
      {
        base::LockGuard<base::Mutex> guard(&result_mutex_);
        if (executed_units_.empty()) {
          break;
        }
        unit = executed_units_.front();
        executed_units_.pop();
      }
      int j = unit->index();
      results[j] = unit->FinishCompilation();
      delete unit;
    }
  }

  void CompileInParallel(ModuleBytesEnv* module_env,
                         std::vector<Handle<Code>>& results,
                         ErrorThrower* thrower) {
    const WasmModule* module = module_env->module_env.module;
    // Data structures for the parallel compilation.

    //-----------------------------------------------------------------------
    // For parallel compilation:
    // 1) The main thread allocates a compilation unit for each wasm function
    //    and stores them in the vector {compilation_units}.
    // 2) The main thread spawns {CompilationTask} instances which run on
    //    the background threads.
    // 3.a) The background threads and the main thread pick one compilation
    //      unit at a time and execute the parallel phase of the compilation
    //      unit. After finishing the execution of the parallel phase, the
    //      result is enqueued in {executed_units}.
    // 3.b) If {executed_units} contains a compilation unit, the main thread
    //      dequeues it and finishes the compilation.
    // 4) After the parallel phase of all compilation units has started, the
    //    main thread waits for all {CompilationTask} instances to finish.
    // 5) The main thread finishes the compilation.

    // Turn on the {CanonicalHandleScope} so that the background threads can
    // use the node cache.
    CanonicalHandleScope canonical(isolate_);

    // 1) The main thread allocates a compilation unit for each wasm function
    //    and stores them in the vector {compilation_units}.
    InitializeParallelCompilation(module->functions, *module_env, thrower);

    // Objects for the synchronization with the background threads.
    base::AtomicNumber<size_t> next_unit(
        static_cast<size_t>(FLAG_skip_compiling_wasm_funcs));

    // 2) The main thread spawns {CompilationTask} instances which run on
    //    the background threads.
    std::unique_ptr<uint32_t[]> task_ids(StartCompilationTasks());

    // 3.a) The background threads and the main thread pick one compilation
    //      unit at a time and execute the parallel phase of the compilation
    //      unit. After finishing the execution of the parallel phase, the
    //      result is enqueued in {executed_units}.
    while (FetchAndExecuteCompilationUnit()) {
      // 3.b) If {executed_units} contains a compilation unit, the main thread
      //      dequeues it and finishes the compilation unit. Compilation units
      //      are finished concurrently to the background threads to save
      //      memory.
      FinishCompilationUnits(results);
    }
    // 4) After the parallel phase of all compilation units has started, the
    //    main thread waits for all {CompilationTask} instances to finish.
    WaitForCompilationTasks(task_ids.get());
    // Finish the compilation of the remaining compilation units.
    FinishCompilationUnits(results);
  }

  void CompileSequentially(ModuleBytesEnv* module_env,
                           std::vector<Handle<Code>>& results,
                           ErrorThrower* thrower) {
    DCHECK(!thrower->error());

    const WasmModule* module = module_env->module_env.module;
    for (uint32_t i = FLAG_skip_compiling_wasm_funcs;
         i < module->functions.size(); ++i) {
      const WasmFunction& func = module->functions[i];
      if (func.imported)
        continue;  // Imports are compiled at instantiation time.

      Handle<Code> code = Handle<Code>::null();
      // Compile the function.
      code = compiler::WasmCompilationUnit::CompileWasmFunction(
          thrower, isolate_, module_env, &func);
      if (code.is_null()) {
        WasmName str = module_env->wire_bytes.GetName(&func);
        thrower->CompileError("Compilation of #%d:%.*s failed.", i,
                              str.length(), str.start());
        break;
      }
      results[i] = code;
    }
  }

  MaybeHandle<WasmModuleObject> CompileToModuleObject(
      ErrorThrower* thrower, const ModuleWireBytes& wire_bytes,
      Handle<Script> asm_js_script,
      Vector<const byte> asm_js_offset_table_bytes) {
    Factory* factory = isolate_->factory();
    // The {module_wrapper} will take ownership of the {WasmModule} object,
    // and it will be destroyed when the GC reclaims the wrapper object.
    Handle<WasmModuleWrapper> module_wrapper =
        WasmModuleWrapper::New(isolate_, module_);
    WasmInstance temp_instance(module_);
    temp_instance.context = isolate_->native_context();
    temp_instance.mem_size = WasmModule::kPageSize * module_->min_mem_pages;
    temp_instance.mem_start = nullptr;
    temp_instance.globals_start = nullptr;

    // Initialize the indirect tables with placeholders.
    int function_table_count =
        static_cast<int>(module_->function_tables.size());
    Handle<FixedArray> function_tables =
        factory->NewFixedArray(function_table_count, TENURED);
    Handle<FixedArray> signature_tables =
        factory->NewFixedArray(function_table_count, TENURED);
    for (int i = 0; i < function_table_count; ++i) {
      temp_instance.function_tables[i] = factory->NewFixedArray(1, TENURED);
      temp_instance.signature_tables[i] = factory->NewFixedArray(1, TENURED);
      function_tables->set(i, *temp_instance.function_tables[i]);
      signature_tables->set(i, *temp_instance.signature_tables[i]);
    }

    HistogramTimerScope wasm_compile_module_time_scope(
        isolate_->counters()->wasm_compile_module_time());

    ModuleBytesEnv module_env(module_, &temp_instance, wire_bytes);

    // The {code_table} array contains import wrappers and functions (which
    // are both included in {functions.size()}, and export wrappers.
    int code_table_size = static_cast<int>(module_->functions.size() +
                                           module_->num_exported_functions);
    Handle<FixedArray> code_table =
        factory->NewFixedArray(static_cast<int>(code_table_size), TENURED);

    // Initialize the code table with the illegal builtin. All call sites will
    // be
    // patched at instantiation.
    Handle<Code> illegal_builtin = isolate_->builtins()->Illegal();
    for (uint32_t i = 0; i < module_->functions.size(); ++i) {
      code_table->set(static_cast<int>(i), *illegal_builtin);
      temp_instance.function_code[i] = illegal_builtin;
    }

    isolate_->counters()->wasm_functions_per_module()->AddSample(
        static_cast<int>(module_->functions.size()));
    CompilationHelper helper(isolate_, module_);
    if (!FLAG_trace_wasm_decoder && FLAG_wasm_num_compilation_tasks != 0) {
      // Avoid a race condition by collecting results into a second vector.
      std::vector<Handle<Code>> results(temp_instance.function_code);
      helper.CompileInParallel(&module_env, results, thrower);
      temp_instance.function_code.swap(results);
    } else {
      helper.CompileSequentially(&module_env, temp_instance.function_code,
                                 thrower);
    }
    if (thrower->error()) return {};

    // At this point, compilation has completed. Update the code table.
    for (size_t i = FLAG_skip_compiling_wasm_funcs;
         i < temp_instance.function_code.size(); ++i) {
      Code* code = *temp_instance.function_code[i];
      code_table->set(static_cast<int>(i), code);
      RecordStats(isolate_, code);
    }

    // Create heap objects for script, module bytes and asm.js offset table to
    // be
    // stored in the shared module data.
    Handle<Script> script;
    Handle<ByteArray> asm_js_offset_table;
    if (asm_js_script.is_null()) {
      script = CreateWasmScript(isolate_, wire_bytes);
    } else {
      script = asm_js_script;
      asm_js_offset_table =
          isolate_->factory()->NewByteArray(asm_js_offset_table_bytes.length());
      asm_js_offset_table->copy_in(0, asm_js_offset_table_bytes.start(),
                                   asm_js_offset_table_bytes.length());
    }
    // TODO(wasm): only save the sections necessary to deserialize a
    // {WasmModule}. E.g. function bodies could be omitted.
    Handle<String> module_bytes =
        factory
            ->NewStringFromOneByte({wire_bytes.start(), wire_bytes.length()},
                                   TENURED)
            .ToHandleChecked();
    DCHECK(module_bytes->IsSeqOneByteString());

    // Create the shared module data.
    // TODO(clemensh): For the same module (same bytes / same hash), we should
    // only have one WasmSharedModuleData. Otherwise, we might only set
    // breakpoints on a (potentially empty) subset of the instances.

    Handle<WasmSharedModuleData> shared = WasmSharedModuleData::New(
        isolate_, module_wrapper, Handle<SeqOneByteString>::cast(module_bytes),
        script, asm_js_offset_table);

    // Create the compiled module object, and populate with compiled functions
    // and information needed at instantiation time. This object needs to be
    // serializable. Instantiation may occur off a deserialized version of this
    // object.
    Handle<WasmCompiledModule> compiled_module =
        WasmCompiledModule::New(isolate_, shared);
    compiled_module->set_num_imported_functions(
        module_->num_imported_functions);
    compiled_module->set_code_table(code_table);
    compiled_module->set_min_mem_pages(module_->min_mem_pages);
    compiled_module->set_max_mem_pages(module_->max_mem_pages);
    if (function_table_count > 0) {
      compiled_module->set_function_tables(function_tables);
      compiled_module->set_signature_tables(signature_tables);
      compiled_module->set_empty_function_tables(function_tables);
    }

    // If we created a wasm script, finish it now and make it public to the
    // debugger.
    if (asm_js_script.is_null()) {
      script->set_wasm_compiled_module(*compiled_module);
      isolate_->debug()->OnAfterCompile(script);
    }

    // Compile JS->WASM wrappers for exported functions.
    JSToWasmWrapperCache js_to_wasm_cache;
    int func_index = 0;
    for (auto exp : module_->export_table) {
      if (exp.kind != kExternalFunction) continue;
      Handle<Code> wasm_code(Code::cast(code_table->get(exp.index)), isolate_);
      Handle<Code> wrapper_code =
          js_to_wasm_cache.CloneOrCompileJSToWasmWrapper(isolate_, module_,
                                                         wasm_code, exp.index);
      int export_index =
          static_cast<int>(module_->functions.size() + func_index);
      code_table->set(export_index, *wrapper_code);
      RecordStats(isolate_, *wrapper_code);
      func_index++;
    }

    return WasmModuleObject::New(isolate_, compiled_module);
}
};

static void ResetCompiledModule(Isolate* isolate, WasmInstanceObject* owner,
                                WasmCompiledModule* compiled_module) {
  TRACE("Resetting %d\n", compiled_module->instance_id());
  Object* undefined = *isolate->factory()->undefined_value();
  Object* fct_obj = compiled_module->ptr_to_code_table();
  if (fct_obj != nullptr && fct_obj != undefined) {
    uint32_t old_mem_size = compiled_module->mem_size();
    uint32_t default_mem_size = compiled_module->default_mem_size();
    Object* mem_start = compiled_module->maybe_ptr_to_memory();

    // Patch code to update memory references, global references, and function
    // table references.
    Zone specialization_zone(isolate->allocator(), ZONE_NAME);
    CodeSpecialization code_specialization(isolate, &specialization_zone);

    if (old_mem_size > 0) {
      CHECK_NE(mem_start, undefined);
      Address old_mem_address =
          static_cast<Address>(JSArrayBuffer::cast(mem_start)->backing_store());
      code_specialization.RelocateMemoryReferences(
          old_mem_address, old_mem_size, nullptr, default_mem_size);
    }

    if (owner->has_globals_buffer()) {
      Address globals_start =
          static_cast<Address>(owner->globals_buffer()->backing_store());
      code_specialization.RelocateGlobals(globals_start, nullptr);
    }

    // Reset function tables.
    if (compiled_module->has_function_tables()) {
      FixedArray* function_tables = compiled_module->ptr_to_function_tables();
      FixedArray* empty_function_tables =
          compiled_module->ptr_to_empty_function_tables();
      DCHECK_EQ(function_tables->length(), empty_function_tables->length());
      for (int i = 0, e = function_tables->length(); i < e; ++i) {
        code_specialization.RelocateObject(
            handle(function_tables->get(i), isolate),
            handle(empty_function_tables->get(i), isolate));
      }
      compiled_module->set_ptr_to_function_tables(empty_function_tables);
    }

    FixedArray* functions = FixedArray::cast(fct_obj);
    for (int i = compiled_module->num_imported_functions(),
             end = functions->length();
         i < end; ++i) {
      Code* code = Code::cast(functions->get(i));
      if (code->kind() != Code::WASM_FUNCTION) {
        // From here on, there should only be wrappers for exported functions.
        for (; i < end; ++i) {
          DCHECK_EQ(Code::JS_TO_WASM_FUNCTION,
                    Code::cast(functions->get(i))->kind());
        }
        break;
      }
      bool changed =
          code_specialization.ApplyToWasmCode(code, SKIP_ICACHE_FLUSH);
      // TODO(wasm): Check if this is faster than passing FLUSH_ICACHE_IF_NEEDED
      // above.
      if (changed) {
        Assembler::FlushICache(isolate, code->instruction_start(),
                               code->instruction_size());
      }
    }
  }
  compiled_module->reset_memory();
}

static void MemoryInstanceFinalizer(Isolate* isolate,
                                    WasmInstanceObject* instance) {
  DisallowHeapAllocation no_gc;
  // If the memory object is destroyed, nothing needs to be done here.
  if (!instance->has_memory_object()) return;
  Handle<WasmInstanceWrapper> instance_wrapper =
      handle(instance->instance_wrapper());
  DCHECK(WasmInstanceWrapper::IsWasmInstanceWrapper(*instance_wrapper));
  DCHECK(instance_wrapper->has_instance());
  bool has_prev = instance_wrapper->has_previous();
  bool has_next = instance_wrapper->has_next();
  Handle<WasmMemoryObject> memory_object(instance->memory_object());

  if (!has_prev && !has_next) {
    memory_object->ResetInstancesLink(isolate);
    return;
  } else {
    Handle<WasmInstanceWrapper> next_wrapper, prev_wrapper;
    if (!has_prev) {
      Handle<WasmInstanceWrapper> next_wrapper =
          instance_wrapper->next_wrapper();
      next_wrapper->reset_previous_wrapper();
      // As this is the first link in the memory object, destroying
      // without updating memory object would corrupt the instance chain in
      // the memory object.
      memory_object->set_instances_link(*next_wrapper);
    } else if (!has_next) {
      instance_wrapper->previous_wrapper()->reset_next_wrapper();
    } else {
      DCHECK(has_next && has_prev);
      Handle<WasmInstanceWrapper> prev_wrapper =
          instance_wrapper->previous_wrapper();
      Handle<WasmInstanceWrapper> next_wrapper =
          instance_wrapper->next_wrapper();
      prev_wrapper->set_next_wrapper(*next_wrapper);
      next_wrapper->set_previous_wrapper(*prev_wrapper);
    }
    // Reset to avoid dangling pointers
    instance_wrapper->reset();
  }
}

static void InstanceFinalizer(const v8::WeakCallbackInfo<void>& data) {
  DisallowHeapAllocation no_gc;
  JSObject** p = reinterpret_cast<JSObject**>(data.GetParameter());
  WasmInstanceObject* owner = reinterpret_cast<WasmInstanceObject*>(*p);
  Isolate* isolate = reinterpret_cast<Isolate*>(data.GetIsolate());
  // If a link to shared memory instances exists, update the list of memory
  // instances before the instance is destroyed.
  if (owner->has_instance_wrapper()) MemoryInstanceFinalizer(isolate, owner);
  WasmCompiledModule* compiled_module = owner->compiled_module();
  TRACE("Finalizing %d {\n", compiled_module->instance_id());
  DCHECK(compiled_module->has_weak_wasm_module());
  WeakCell* weak_wasm_module = compiled_module->ptr_to_weak_wasm_module();

  // weak_wasm_module may have been cleared, meaning the module object
  // was GC-ed. In that case, there won't be any new instances created,
  // and we don't need to maintain the links between instances.
  if (!weak_wasm_module->cleared()) {
    JSObject* wasm_module = JSObject::cast(weak_wasm_module->value());
    WasmCompiledModule* current_template =
        WasmCompiledModule::cast(wasm_module->GetInternalField(0));

    TRACE("chain before {\n");
    TRACE_CHAIN(current_template);
    TRACE("}\n");

    DCHECK(!current_template->has_weak_prev_instance());
    WeakCell* next = compiled_module->maybe_ptr_to_weak_next_instance();
    WeakCell* prev = compiled_module->maybe_ptr_to_weak_prev_instance();

    if (current_template == compiled_module) {
      if (next == nullptr) {
        ResetCompiledModule(isolate, owner, compiled_module);
      } else {
        DCHECK(next->value()->IsFixedArray());
        wasm_module->SetInternalField(0, next->value());
        DCHECK_NULL(prev);
        WasmCompiledModule::cast(next->value())->reset_weak_prev_instance();
      }
    } else {
      DCHECK(!(prev == nullptr && next == nullptr));
      // the only reason prev or next would be cleared is if the
      // respective objects got collected, but if that happened,
      // we would have relinked the list.
      if (prev != nullptr) {
        DCHECK(!prev->cleared());
        if (next == nullptr) {
          WasmCompiledModule::cast(prev->value())->reset_weak_next_instance();
        } else {
          WasmCompiledModule::cast(prev->value())
              ->set_ptr_to_weak_next_instance(next);
        }
      }
      if (next != nullptr) {
        DCHECK(!next->cleared());
        if (prev == nullptr) {
          WasmCompiledModule::cast(next->value())->reset_weak_prev_instance();
        } else {
          WasmCompiledModule::cast(next->value())
              ->set_ptr_to_weak_prev_instance(prev);
        }
      }
    }
    TRACE("chain after {\n");
    TRACE_CHAIN(WasmCompiledModule::cast(wasm_module->GetInternalField(0)));
    TRACE("}\n");
  }
  compiled_module->reset_weak_owning_instance();
  GlobalHandles::Destroy(reinterpret_cast<Object**>(p));
  TRACE("}\n");
}

std::pair<int, int> GetFunctionOffsetAndLength(
    Handle<WasmCompiledModule> compiled_module, int func_index) {
  WasmModule* module = compiled_module->module();
  if (func_index < 0 ||
      static_cast<size_t>(func_index) > module->functions.size()) {
    return {0, 0};
  }
  WasmFunction& func = module->functions[func_index];
  return {static_cast<int>(func.code_start_offset),
          static_cast<int>(func.code_end_offset - func.code_start_offset)};
}
}  // namespace

Handle<JSArrayBuffer> SetupArrayBuffer(Isolate* isolate, void* backing_store,
                                       size_t size, bool is_external,
                                       bool enable_guard_regions) {
  Handle<JSArrayBuffer> buffer = isolate->factory()->NewJSArrayBuffer();
  JSArrayBuffer::Setup(buffer, isolate, is_external, backing_store,
                       static_cast<int>(size));
  buffer->set_is_neuterable(false);
  buffer->set_has_guard_region(enable_guard_regions);

  if (is_external) {
    // We mark the buffer as external if we allocated it here with guard
    // pages. That means we need to arrange for it to be freed.

    // TODO(eholk): Finalizers may not run when the main thread is shutting
    // down, which means we may leak memory here.
    Handle<Object> global_handle = isolate->global_handles()->Create(*buffer);
    GlobalHandles::MakeWeak(global_handle.location(), global_handle.location(),
                            &MemoryFinalizer, v8::WeakCallbackType::kFinalizer);
  }
  return buffer;
}

Handle<JSArrayBuffer> wasm::NewArrayBuffer(Isolate* isolate, size_t size,
                                           bool enable_guard_regions) {
  if (size > (FLAG_wasm_max_mem_pages * WasmModule::kPageSize)) {
    // TODO(titzer): lift restriction on maximum memory allocated here.
    return Handle<JSArrayBuffer>::null();
  }

  enable_guard_regions = enable_guard_regions && kGuardRegionsSupported;

  bool is_external;  // Set by TryAllocateBackingStore
  void* memory =
      TryAllocateBackingStore(isolate, size, enable_guard_regions, is_external);

  if (memory == nullptr) {
    return Handle<JSArrayBuffer>::null();
  }

#if DEBUG
  // Double check the API allocator actually zero-initialized the memory.
  const byte* bytes = reinterpret_cast<const byte*>(memory);
  for (size_t i = 0; i < size; ++i) {
    DCHECK_EQ(0, bytes[i]);
  }
#endif

  return SetupArrayBuffer(isolate, memory, size, is_external,
                          enable_guard_regions);
}

std::ostream& wasm::operator<<(std::ostream& os, const WasmModule& module) {
  os << "WASM module with ";
  os << (module.min_mem_pages * module.kPageSize) << " min mem";
  os << (module.max_mem_pages * module.kPageSize) << " max mem";
  os << module.functions.size() << " functions";
  os << module.functions.size() << " globals";
  os << module.functions.size() << " data segments";
  return os;
}

std::ostream& wasm::operator<<(std::ostream& os, const WasmFunction& function) {
  os << "WASM function with signature " << *function.sig;

  os << " code bytes: "
     << (function.code_end_offset - function.code_start_offset);
  return os;
}

std::ostream& wasm::operator<<(std::ostream& os, const WasmFunctionName& name) {
  os << "#" << name.function_->func_index;
  if (name.function_->name_offset > 0) {
    if (name.name_.start()) {
      os << ":";
      os.write(name.name_.start(), name.name_.length());
    }
  } else {
    os << "?";
  }
  return os;
}

WasmInstanceObject* wasm::GetOwningWasmInstance(Code* code) {
  DisallowHeapAllocation no_gc;
  DCHECK(code->kind() == Code::WASM_FUNCTION ||
         code->kind() == Code::WASM_INTERPRETER_ENTRY);
  FixedArray* deopt_data = code->deoptimization_data();
  DCHECK_NOT_NULL(deopt_data);
  DCHECK_EQ(code->kind() == Code::WASM_INTERPRETER_ENTRY ? 1 : 2,
            deopt_data->length());
  Object* weak_link = deopt_data->get(0);
  DCHECK(weak_link->IsWeakCell());
  WeakCell* cell = WeakCell::cast(weak_link);
  if (cell->cleared()) return nullptr;
  return WasmInstanceObject::cast(cell->value());
}

int wasm::GetFunctionCodeOffset(Handle<WasmCompiledModule> compiled_module,
                                int func_index) {
  return GetFunctionOffsetAndLength(compiled_module, func_index).first;
}

WasmModule::WasmModule(Zone* owned)
    : owned_zone(owned), pending_tasks(new base::Semaphore(0)) {}

static WasmFunction* GetWasmFunctionForImportWrapper(Isolate* isolate,
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

static Handle<Code> UnwrapImportWrapper(Handle<Object> target) {
  Handle<JSFunction> func = Handle<JSFunction>::cast(target);
  Handle<Code> export_wrapper_code = handle(func->code());
  int found = 0;
  int mask = RelocInfo::ModeMask(RelocInfo::CODE_TARGET);
  Handle<Code> code;
  for (RelocIterator it(*export_wrapper_code, mask); !it.done(); it.next()) {
    RelocInfo* rinfo = it.rinfo();
    Address target_address = rinfo->target_address();
    Code* target = Code::GetCodeFromTargetAddress(target_address);
    if (target->kind() == Code::WASM_FUNCTION ||
        target->kind() == Code::WASM_TO_JS_FUNCTION) {
      ++found;
      code = handle(target);
    }
  }
  DCHECK_EQ(1, found);
  return code;
}

static Handle<Code> CompileImportWrapper(Isolate* isolate, int index,
                                         FunctionSig* sig,
                                         Handle<JSReceiver> target,
                                         Handle<String> module_name,
                                         MaybeHandle<String> import_name,
                                         ModuleOrigin origin) {
  WasmFunction* other_func = GetWasmFunctionForImportWrapper(isolate, target);
  if (other_func) {
    if (sig->Equals(other_func->sig)) {
      // Signature matched. Unwrap the JS->WASM wrapper and return the raw
      // WASM function code.
      return UnwrapImportWrapper(target);
    } else {
      return Handle<Code>::null();
    }
  } else {
    // Signature mismatch. Compile a new wrapper for the new signature.
    return compiler::CompileWasmToJSWrapper(isolate, target, sig, index,
                                            module_name, import_name, origin);
  }
}

static void UpdateDispatchTablesInternal(Isolate* isolate,
                                         Handle<FixedArray> dispatch_tables,
                                         int index, WasmFunction* function,
                                         Handle<Code> code) {
  DCHECK_EQ(0, dispatch_tables->length() % 4);
  for (int i = 0; i < dispatch_tables->length(); i += 4) {
    int table_index = Smi::cast(dispatch_tables->get(i + 1))->value();
    Handle<FixedArray> function_table(
        FixedArray::cast(dispatch_tables->get(i + 2)), isolate);
    Handle<FixedArray> signature_table(
        FixedArray::cast(dispatch_tables->get(i + 3)), isolate);
    if (function) {
      // TODO(titzer): the signature might need to be copied to avoid
      // a dangling pointer in the signature map.
      Handle<WasmInstanceObject> instance(
          WasmInstanceObject::cast(dispatch_tables->get(i)), isolate);
      int sig_index = static_cast<int>(
          instance->module()->function_tables[table_index].map.FindOrInsert(
              function->sig));
      signature_table->set(index, Smi::FromInt(sig_index));
      function_table->set(index, *code);
    } else {
      Code* code = nullptr;
      signature_table->set(index, Smi::FromInt(-1));
      function_table->set(index, code);
    }
  }
}

void wasm::UpdateDispatchTables(Isolate* isolate,
                                Handle<FixedArray> dispatch_tables, int index,
                                Handle<JSFunction> function) {
  if (function.is_null()) {
    UpdateDispatchTablesInternal(isolate, dispatch_tables, index, nullptr,
                                 Handle<Code>::null());
  } else {
    UpdateDispatchTablesInternal(
        isolate, dispatch_tables, index,
        GetWasmFunctionForImportWrapper(isolate, function),
        UnwrapImportWrapper(function));
  }
}

// A helper class to simplify instantiating a module from a compiled module.
// It closes over the {Isolate}, the {ErrorThrower}, the {WasmCompiledModule},
// etc.
class InstantiationHelper {
 public:
  InstantiationHelper(Isolate* isolate, ErrorThrower* thrower,
                      Handle<WasmModuleObject> module_object,
                      MaybeHandle<JSReceiver> ffi,
                      MaybeHandle<JSArrayBuffer> memory)
      : isolate_(isolate),
        module_(module_object->compiled_module()->module()),
        thrower_(thrower),
        module_object_(module_object),
        ffi_(ffi.is_null() ? Handle<JSReceiver>::null()
                           : ffi.ToHandleChecked()),
        memory_(memory.is_null() ? Handle<JSArrayBuffer>::null()
                                 : memory.ToHandleChecked()) {}

  // Build an instance, in all of its glory.
  MaybeHandle<WasmInstanceObject> Build() {
    // Check that an imports argument was provided, if the module requires it.
    // No point in continuing otherwise.
    if (!module_->import_table.empty() && ffi_.is_null()) {
      thrower_->TypeError(
          "Imports argument must be present and must be an object");
      return {};
    }

    HistogramTimerScope wasm_instantiate_module_time_scope(
        isolate_->counters()->wasm_instantiate_module_time());
    Factory* factory = isolate_->factory();

    //--------------------------------------------------------------------------
    // Reuse the compiled module (if no owner), otherwise clone.
    //--------------------------------------------------------------------------
    Handle<FixedArray> code_table;
    Handle<FixedArray> old_code_table;
    MaybeHandle<WasmInstanceObject> owner;

    TRACE("Starting new module instantiation\n");
    {
      // Root the owner, if any, before doing any allocations, which
      // may trigger GC.
      // Both owner and original template need to be in sync. Even
      // after we lose the original template handle, the code
      // objects we copied from it have data relative to the
      // instance - such as globals addresses.
      Handle<WasmCompiledModule> original;
      {
        DisallowHeapAllocation no_gc;
        original = handle(module_object_->compiled_module());
        if (original->has_weak_owning_instance()) {
          owner = handle(WasmInstanceObject::cast(
              original->weak_owning_instance()->value()));
        }
      }
      DCHECK(!original.is_null());
      // Always make a new copy of the code_table, since the old_code_table
      // may still have placeholders for imports.
      old_code_table = original->code_table();
      code_table = factory->CopyFixedArray(old_code_table);

      if (original->has_weak_owning_instance()) {
        // Clone, but don't insert yet the clone in the instances chain.
        // We do that last. Since we are holding on to the owner instance,
        // the owner + original state used for cloning and patching
        // won't be mutated by possible finalizer runs.
        DCHECK(!owner.is_null());
        TRACE("Cloning from %d\n", original->instance_id());
        compiled_module_ = WasmCompiledModule::Clone(isolate_, original);
        // Avoid creating too many handles in the outer scope.
        HandleScope scope(isolate_);

        // Clone the code for WASM functions and exports.
        for (int i = 0; i < code_table->length(); ++i) {
          Handle<Code> orig_code =
              code_table->GetValueChecked<Code>(isolate_, i);
          switch (orig_code->kind()) {
            case Code::WASM_TO_JS_FUNCTION:
              // Imports will be overwritten with newly compiled wrappers.
              break;
            case Code::JS_TO_WASM_FUNCTION:
            case Code::WASM_FUNCTION: {
              Handle<Code> code = factory->CopyCode(orig_code);
              code_table->set(i, *code);
              break;
            }
            default:
              UNREACHABLE();
          }
        }
        RecordStats(isolate_, code_table);
      } else {
        // There was no owner, so we can reuse the original.
        compiled_module_ = original;
        TRACE("Reusing existing instance %d\n",
              compiled_module_->instance_id());
      }
      compiled_module_->set_code_table(code_table);
      compiled_module_->set_native_context(isolate_->native_context());
    }

    //--------------------------------------------------------------------------
    // Allocate the instance object.
    //--------------------------------------------------------------------------
    Zone instantiation_zone(isolate_->allocator(), ZONE_NAME);
    CodeSpecialization code_specialization(isolate_, &instantiation_zone);
    Handle<WasmInstanceObject> instance =
        WasmInstanceObject::New(isolate_, compiled_module_);

    //--------------------------------------------------------------------------
    // Set up the globals for the new instance.
    //--------------------------------------------------------------------------
    MaybeHandle<JSArrayBuffer> old_globals;
    uint32_t globals_size = module_->globals_size;
    if (globals_size > 0) {
      const bool enable_guard_regions = false;
      Handle<JSArrayBuffer> global_buffer =
          NewArrayBuffer(isolate_, globals_size, enable_guard_regions);
      globals_ = global_buffer;
      if (globals_.is_null()) {
        thrower_->RangeError("Out of memory: wasm globals");
        return {};
      }
      Address old_globals_start = nullptr;
      if (!owner.is_null()) {
        DCHECK(owner.ToHandleChecked()->has_globals_buffer());
        old_globals_start = static_cast<Address>(
            owner.ToHandleChecked()->globals_buffer()->backing_store());
      }
      Address new_globals_start =
          static_cast<Address>(global_buffer->backing_store());
      code_specialization.RelocateGlobals(old_globals_start, new_globals_start);
      instance->set_globals_buffer(*global_buffer);
    }

    //--------------------------------------------------------------------------
    // Prepare for initialization of function tables.
    //--------------------------------------------------------------------------
    int function_table_count =
        static_cast<int>(module_->function_tables.size());
    table_instances_.reserve(module_->function_tables.size());
    for (int index = 0; index < function_table_count; ++index) {
      table_instances_.push_back(
          {Handle<WasmTableObject>::null(), Handle<FixedArray>::null(),
           Handle<FixedArray>::null(), Handle<FixedArray>::null()});
    }

    //--------------------------------------------------------------------------
    // Process the imports for the module.
    //--------------------------------------------------------------------------
    int num_imported_functions = ProcessImports(code_table, instance);
    if (num_imported_functions < 0) return {};

    //--------------------------------------------------------------------------
    // Process the initialization for the module's globals.
    //--------------------------------------------------------------------------
    InitGlobals();

    //--------------------------------------------------------------------------
    // Set up the indirect function tables for the new instance.
    //--------------------------------------------------------------------------
    if (function_table_count > 0)
      InitializeTables(code_table, instance, &code_specialization);

    //--------------------------------------------------------------------------
    // Set up the memory for the new instance.
    //--------------------------------------------------------------------------
    MaybeHandle<JSArrayBuffer> old_memory;

    uint32_t min_mem_pages = module_->min_mem_pages;
    isolate_->counters()->wasm_min_mem_pages_count()->AddSample(min_mem_pages);

    if (!memory_.is_null()) {
      // Set externally passed ArrayBuffer non neuterable.
      memory_->set_is_neuterable(false);

      DCHECK_IMPLIES(EnableGuardRegions(), module_->origin == kAsmJsOrigin ||
                                               memory_->has_guard_region());
    } else if (min_mem_pages > 0) {
      memory_ = AllocateMemory(min_mem_pages);
      if (memory_.is_null()) return {};  // failed to allocate memory
    }

    //--------------------------------------------------------------------------
    // Check that indirect function table segments are within bounds.
    //--------------------------------------------------------------------------
    for (WasmTableInit& table_init : module_->table_inits) {
      DCHECK(table_init.table_index < table_instances_.size());
      uint32_t base = EvalUint32InitExpr(table_init.offset);
      uint32_t table_size =
          table_instances_[table_init.table_index].function_table->length();
      if (!in_bounds(base, static_cast<uint32_t>(table_init.entries.size()),
                     table_size)) {
        thrower_->LinkError("table initializer is out of bounds");
        return {};
      }
    }

    //--------------------------------------------------------------------------
    // Check that memory segments are within bounds.
    //--------------------------------------------------------------------------
    for (WasmDataSegment& seg : module_->data_segments) {
      uint32_t base = EvalUint32InitExpr(seg.dest_addr);
      uint32_t mem_size = memory_.is_null()
          ? 0 : static_cast<uint32_t>(memory_->byte_length()->Number());
      if (!in_bounds(base, seg.source_size, mem_size)) {
        thrower_->LinkError("data segment is out of bounds");
        return {};
      }
    }

    //--------------------------------------------------------------------------
    // Initialize memory.
    //--------------------------------------------------------------------------
    if (!memory_.is_null()) {
      instance->set_memory_buffer(*memory_);
      Address mem_start = static_cast<Address>(memory_->backing_store());
      uint32_t mem_size =
          static_cast<uint32_t>(memory_->byte_length()->Number());
      LoadDataSegments(mem_start, mem_size);

      uint32_t old_mem_size = compiled_module_->mem_size();
      Address old_mem_start =
          compiled_module_->has_memory()
              ? static_cast<Address>(
                    compiled_module_->memory()->backing_store())
              : nullptr;
      // We might get instantiated again with the same memory. No patching
      // needed in this case.
      if (old_mem_start != mem_start || old_mem_size != mem_size) {
        code_specialization.RelocateMemoryReferences(
            old_mem_start, old_mem_size, mem_start, mem_size);
      }
      compiled_module_->set_memory(memory_);
    }

    //--------------------------------------------------------------------------
    // Set up the runtime support for the new instance.
    //--------------------------------------------------------------------------
    Handle<WeakCell> weak_link = factory->NewWeakCell(instance);

    for (int i = num_imported_functions + FLAG_skip_compiling_wasm_funcs;
         i < code_table->length(); ++i) {
      Handle<Code> code = code_table->GetValueChecked<Code>(isolate_, i);
      if (code->kind() == Code::WASM_FUNCTION) {
        Handle<FixedArray> deopt_data = factory->NewFixedArray(2, TENURED);
        deopt_data->set(0, *weak_link);
        deopt_data->set(1, Smi::FromInt(static_cast<int>(i)));
        deopt_data->set_length(2);
        code->set_deoptimization_data(*deopt_data);
      }
    }

    //--------------------------------------------------------------------------
    // Set up the exports object for the new instance.
    //--------------------------------------------------------------------------
    ProcessExports(code_table, instance, compiled_module_);

    //--------------------------------------------------------------------------
    // Add instance to Memory object
    //--------------------------------------------------------------------------
    DCHECK(wasm::IsWasmInstance(*instance));
    if (instance->has_memory_object()) {
      instance->memory_object()->AddInstance(isolate_, instance);
    }

    //--------------------------------------------------------------------------
    // Initialize the indirect function tables.
    //--------------------------------------------------------------------------
    if (function_table_count > 0) LoadTableSegments(code_table, instance);

    // Patch all code with the relocations registered in code_specialization.
    {
      code_specialization.RelocateDirectCalls(instance);
      code_specialization.ApplyToWholeInstance(*instance, SKIP_ICACHE_FLUSH);
    }

    FlushICache(isolate_, code_table);

    //--------------------------------------------------------------------------
    // Unpack and notify signal handler of protected instructions.
    //--------------------------------------------------------------------------
    if (FLAG_wasm_trap_handler) {
      for (int i = 0; i < code_table->length(); ++i) {
        Handle<Code> code = code_table->GetValueChecked<Code>(isolate_, i);

        if (code->kind() != Code::WASM_FUNCTION) {
          continue;
        }

        const intptr_t base = reinterpret_cast<intptr_t>(code->entry());

        Zone zone(isolate_->allocator(), "Wasm Module");
        ZoneVector<trap_handler::ProtectedInstructionData> unpacked(&zone);
        const int mode_mask =
            RelocInfo::ModeMask(RelocInfo::WASM_PROTECTED_INSTRUCTION_LANDING);
        for (RelocIterator it(*code, mode_mask); !it.done(); it.next()) {
          trap_handler::ProtectedInstructionData data;
          data.instr_offset = it.rinfo()->data();
          data.landing_offset =
              reinterpret_cast<intptr_t>(it.rinfo()->pc()) - base;
          unpacked.emplace_back(data);
        }
        // TODO(eholk): Register the protected instruction information once the
        // trap handler is in place.
      }
    }

    //--------------------------------------------------------------------------
    // Set up and link the new instance.
    //--------------------------------------------------------------------------
    {
      Handle<Object> global_handle =
          isolate_->global_handles()->Create(*instance);
      Handle<WeakCell> link_to_clone = factory->NewWeakCell(compiled_module_);
      Handle<WeakCell> link_to_owning_instance = factory->NewWeakCell(instance);
      MaybeHandle<WeakCell> link_to_original;
      MaybeHandle<WasmCompiledModule> original;
      if (!owner.is_null()) {
        // prepare the data needed for publishing in a chain, but don't link
        // just yet, because
        // we want all the publishing to happen free from GC interruptions, and
        // so we do it in
        // one GC-free scope afterwards.
        original = handle(owner.ToHandleChecked()->compiled_module());
        link_to_original = factory->NewWeakCell(original.ToHandleChecked());
      }
      // Publish the new instance to the instances chain.
      {
        DisallowHeapAllocation no_gc;
        if (!link_to_original.is_null()) {
          compiled_module_->set_weak_next_instance(
              link_to_original.ToHandleChecked());
          original.ToHandleChecked()->set_weak_prev_instance(link_to_clone);
          compiled_module_->set_weak_wasm_module(
              original.ToHandleChecked()->weak_wasm_module());
        }
        module_object_->SetInternalField(0, *compiled_module_);
        compiled_module_->set_weak_owning_instance(link_to_owning_instance);
        GlobalHandles::MakeWeak(global_handle.location(),
                                global_handle.location(), &InstanceFinalizer,
                                v8::WeakCallbackType::kFinalizer);
      }
    }

    //--------------------------------------------------------------------------
    // Set all breakpoints that were set on the shared module.
    //--------------------------------------------------------------------------
    WasmSharedModuleData::SetBreakpointsOnNewInstance(
        compiled_module_->shared(), instance);

    //--------------------------------------------------------------------------
    // Run the start function if one was specified.
    //--------------------------------------------------------------------------
    if (module_->start_function_index >= 0) {
      HandleScope scope(isolate_);
      int start_index = module_->start_function_index;
      Handle<Code> startup_code =
          code_table->GetValueChecked<Code>(isolate_, start_index);
      FunctionSig* sig = module_->functions[start_index].sig;
      Handle<Code> wrapper_code =
          js_to_wasm_cache_.CloneOrCompileJSToWasmWrapper(
              isolate_, module_, startup_code, start_index);
      Handle<WasmExportedFunction> startup_fct = WasmExportedFunction::New(
          isolate_, instance, MaybeHandle<String>(), start_index,
          static_cast<int>(sig->parameter_count()), wrapper_code);
      RecordStats(isolate_, *startup_code);
      // Call the JS function.
      Handle<Object> undefined = factory->undefined_value();
      MaybeHandle<Object> retval =
          Execution::Call(isolate_, startup_fct, undefined, 0, nullptr);

      if (retval.is_null()) {
        DCHECK(isolate_->has_pending_exception());
        isolate_->OptionalRescheduleException(false);
        // It's unfortunate that the new instance is already linked in the
        // chain. However, we need to set up everything before executing the
        // start function, such that stack trace information can be generated
        // correctly already in the start function.
        return {};
      }
    }

    DCHECK(!isolate_->has_pending_exception());
    TRACE("Finishing instance %d\n", compiled_module_->instance_id());
    TRACE_CHAIN(module_object_->compiled_module());
    return instance;
  }

 private:
  // Represents the initialized state of a table.
  struct TableInstance {
    Handle<WasmTableObject> table_object;    // WebAssembly.Table instance
    Handle<FixedArray> js_wrappers;          // JSFunctions exported
    Handle<FixedArray> function_table;       // internal code array
    Handle<FixedArray> signature_table;      // internal sig array
  };

  Isolate* isolate_;
  WasmModule* const module_;
  ErrorThrower* thrower_;
  Handle<WasmModuleObject> module_object_;
  Handle<JSReceiver> ffi_;        // TODO(titzer): Use MaybeHandle
  Handle<JSArrayBuffer> memory_;  // TODO(titzer): Use MaybeHandle
  Handle<JSArrayBuffer> globals_;
  Handle<WasmCompiledModule> compiled_module_;
  std::vector<TableInstance> table_instances_;
  std::vector<Handle<JSFunction>> js_wrappers_;
  JSToWasmWrapperCache js_to_wasm_cache_;

  // Helper routines to print out errors with imports.
  void ReportLinkError(const char* error, uint32_t index,
                       Handle<String> module_name, Handle<String> import_name) {
    thrower_->LinkError(
        "Import #%d module=\"%.*s\" function=\"%.*s\" error: %s", index,
        module_name->length(), module_name->ToCString().get(),
        import_name->length(), import_name->ToCString().get(), error);
  }

  MaybeHandle<Object> ReportLinkError(const char* error, uint32_t index,
                                      Handle<String> module_name) {
    thrower_->LinkError("Import #%d module=\"%.*s\" error: %s", index,
                        module_name->length(), module_name->ToCString().get(),
                        error);
    return MaybeHandle<Object>();
  }

  // Look up an import value in the {ffi_} object.
  MaybeHandle<Object> LookupImport(uint32_t index, Handle<String> module_name,
                                   Handle<String> import_name) {
    // We pre-validated in the js-api layer that the ffi object is present, and
    // a JSObject, if the module has imports.
    DCHECK(!ffi_.is_null());

    // Look up the module first.
    MaybeHandle<Object> result =
        Object::GetPropertyOrElement(ffi_, module_name);
    if (result.is_null()) {
      return ReportLinkError("module not found", index, module_name);
    }

    Handle<Object> module = result.ToHandleChecked();

    // Look up the value in the module.
    if (!module->IsJSReceiver()) {
      return ReportLinkError("module is not an object or function", index,
                             module_name);
    }

    result = Object::GetPropertyOrElement(module, import_name);
    if (result.is_null()) {
      ReportLinkError("import not found", index, module_name, import_name);
      return MaybeHandle<JSFunction>();
    }

    return result;
  }

  uint32_t EvalUint32InitExpr(const WasmInitExpr& expr) {
    switch (expr.kind) {
      case WasmInitExpr::kI32Const:
        return expr.val.i32_const;
      case WasmInitExpr::kGlobalIndex: {
        uint32_t offset = module_->globals[expr.val.global_index].offset;
        return *reinterpret_cast<uint32_t*>(raw_buffer_ptr(globals_, offset));
      }
      default:
        UNREACHABLE();
        return 0;
    }
  }

  bool in_bounds(uint32_t offset, uint32_t size, uint32_t upper) {
    return offset + size <= upper && offset + size >= offset;
  }

  // Load data segments into the memory.
  void LoadDataSegments(Address mem_addr, size_t mem_size) {
    Handle<SeqOneByteString> module_bytes(compiled_module_->module_bytes(),
                                          isolate_);
    for (const WasmDataSegment& segment : module_->data_segments) {
      uint32_t source_size = segment.source_size;
      // Segments of size == 0 are just nops.
      if (source_size == 0) continue;
      uint32_t dest_offset = EvalUint32InitExpr(segment.dest_addr);
      DCHECK(in_bounds(dest_offset, source_size,
                       static_cast<uint32_t>(mem_size)));
      byte* dest = mem_addr + dest_offset;
      const byte* src = reinterpret_cast<const byte*>(
          module_bytes->GetCharsAddress() + segment.source_offset);
      memcpy(dest, src, source_size);
    }
  }

  void WriteGlobalValue(WasmGlobal& global, Handle<Object> value) {
    double num = 0;
    if (value->IsSmi()) {
      num = Smi::cast(*value)->value();
    } else if (value->IsHeapNumber()) {
      num = HeapNumber::cast(*value)->value();
    } else {
      UNREACHABLE();
    }
    TRACE("init [globals+%u] = %lf, type = %s\n", global.offset, num,
          WasmOpcodes::TypeName(global.type));
    switch (global.type) {
      case kWasmI32:
        *GetRawGlobalPtr<int32_t>(global) = static_cast<int32_t>(num);
        break;
      case kWasmI64:
        // TODO(titzer): initialization of imported i64 globals.
        UNREACHABLE();
        break;
      case kWasmF32:
        *GetRawGlobalPtr<float>(global) = static_cast<float>(num);
        break;
      case kWasmF64:
        *GetRawGlobalPtr<double>(global) = static_cast<double>(num);
        break;
      default:
        UNREACHABLE();
    }
  }

  // Process the imports, including functions, tables, globals, and memory, in
  // order, loading them from the {ffi_} object. Returns the number of imported
  // functions.
  int ProcessImports(Handle<FixedArray> code_table,
                     Handle<WasmInstanceObject> instance) {
    int num_imported_functions = 0;
    int num_imported_tables = 0;
    for (int index = 0; index < static_cast<int>(module_->import_table.size());
         ++index) {
      WasmImport& import = module_->import_table[index];

      Handle<String> module_name;
      MaybeHandle<String> maybe_module_name =
          WasmCompiledModule::ExtractUtf8StringFromModuleBytes(
              isolate_, compiled_module_, import.module_name_offset,
              import.module_name_length);
      if (!maybe_module_name.ToHandle(&module_name)) return -1;

      Handle<String> import_name;
      MaybeHandle<String> maybe_import_name =
          WasmCompiledModule::ExtractUtf8StringFromModuleBytes(
              isolate_, compiled_module_, import.field_name_offset,
              import.field_name_length);
      if (!maybe_import_name.ToHandle(&import_name)) return -1;

      MaybeHandle<Object> result =
          LookupImport(index, module_name, import_name);
      if (thrower_->error()) return -1;
      Handle<Object> value = result.ToHandleChecked();

      switch (import.kind) {
        case kExternalFunction: {
          // Function imports must be callable.
          if (!value->IsCallable()) {
            ReportLinkError("function import requires a callable", index,
                            module_name, import_name);
            return -1;
          }

          Handle<Code> import_wrapper = CompileImportWrapper(
              isolate_, index, module_->functions[import.index].sig,
              Handle<JSReceiver>::cast(value), module_name, import_name,
              module_->origin);
          if (import_wrapper.is_null()) {
            ReportLinkError(
                "imported function does not match the expected type", index,
                module_name, import_name);
            return -1;
          }
          code_table->set(num_imported_functions, *import_wrapper);
          RecordStats(isolate_, *import_wrapper);
          num_imported_functions++;
          break;
        }
        case kExternalTable: {
          if (!WasmJs::IsWasmTableObject(isolate_, value)) {
            ReportLinkError("table import requires a WebAssembly.Table", index,
                            module_name, import_name);
            return -1;
          }
          WasmIndirectFunctionTable& table =
              module_->function_tables[num_imported_tables];
          TableInstance& table_instance = table_instances_[num_imported_tables];
          table_instance.table_object = Handle<WasmTableObject>::cast(value);
          table_instance.js_wrappers = Handle<FixedArray>(
              table_instance.table_object->functions(), isolate_);

          int imported_cur_size = table_instance.js_wrappers->length();
          if (imported_cur_size < static_cast<int>(table.min_size)) {
            thrower_->LinkError(
                "table import %d is smaller than minimum %d, got %u", index,
                table.min_size, imported_cur_size);
            return -1;
          }

          if (table.has_max) {
            int64_t imported_max_size =
                table_instance.table_object->maximum_length();
            if (imported_max_size < 0) {
              thrower_->LinkError(
                  "table import %d has no maximum length, expected %d", index,
                  table.max_size);
              return -1;
            }
            if (imported_max_size > table.max_size) {
              thrower_->LinkError(
                  "table import %d has maximum larger than maximum %d, "
                  "got %" PRIx64,
                  index, table.max_size, imported_max_size);
              return -1;
            }
          }

          // Allocate a new dispatch table and signature table.
          int table_size = imported_cur_size;
          table_instance.function_table =
              isolate_->factory()->NewFixedArray(table_size);
          table_instance.signature_table =
              isolate_->factory()->NewFixedArray(table_size);
          for (int i = 0; i < table_size; ++i) {
            table_instance.signature_table->set(i,
                                                Smi::FromInt(kInvalidSigIndex));
          }
          // Initialize the dispatch table with the (foreign) JS functions
          // that are already in the table.
          for (int i = 0; i < table_size; ++i) {
            Handle<Object> val(table_instance.js_wrappers->get(i), isolate_);
            if (!val->IsJSFunction()) continue;
            WasmFunction* function =
                GetWasmFunctionForImportWrapper(isolate_, val);
            if (function == nullptr) {
              thrower_->LinkError("table import %d[%d] is not a WASM function",
                                  index, i);
              return -1;
            }
            int sig_index = table.map.FindOrInsert(function->sig);
            table_instance.signature_table->set(i, Smi::FromInt(sig_index));
            table_instance.function_table->set(i, *UnwrapImportWrapper(val));
          }

          num_imported_tables++;
          break;
        }
        case kExternalMemory: {
          // Validation should have failed if more than one memory object was
          // provided.
          DCHECK(!instance->has_memory_object());
          if (!WasmJs::IsWasmMemoryObject(isolate_, value)) {
            ReportLinkError("memory import must be a WebAssembly.Memory object",
                            index, module_name, import_name);
            return -1;
          }
          auto memory = Handle<WasmMemoryObject>::cast(value);
          DCHECK(WasmJs::IsWasmMemoryObject(isolate_, memory));
          instance->set_memory_object(*memory);
          memory_ = Handle<JSArrayBuffer>(memory->buffer(), isolate_);
          uint32_t imported_cur_pages = static_cast<uint32_t>(
              memory_->byte_length()->Number() / WasmModule::kPageSize);
          if (imported_cur_pages < module_->min_mem_pages) {
            thrower_->LinkError(
                "memory import %d is smaller than maximum %u, got %u", index,
                module_->min_mem_pages, imported_cur_pages);
          }
          int32_t imported_max_pages = memory->maximum_pages();
          if (module_->has_max_mem) {
            if (imported_max_pages < 0) {
              thrower_->LinkError(
                  "memory import %d has no maximum limit, expected at most %u",
                  index, imported_max_pages);
              return -1;
            }
            if (static_cast<uint32_t>(imported_max_pages) >
                module_->max_mem_pages) {
              thrower_->LinkError(
                  "memory import %d has larger maximum than maximum %u, got %d",
                  index, module_->max_mem_pages, imported_max_pages);
              return -1;
            }
          }
          break;
        }
        case kExternalGlobal: {
          // Global imports are converted to numbers and written into the
          // {globals_} array buffer.
          if (module_->globals[import.index].type == kWasmI64) {
            ReportLinkError("global import cannot have type i64", index,
                            module_name, import_name);
            return -1;
          }
          if (!value->IsNumber()) {
            ReportLinkError("global import must be a number", index,
                            module_name, import_name);
            return -1;
          }
          WriteGlobalValue(module_->globals[import.index], value);
          break;
        }
        default:
          UNREACHABLE();
          break;
      }
    }
    return num_imported_functions;
  }

  template <typename T>
  T* GetRawGlobalPtr(WasmGlobal& global) {
    return reinterpret_cast<T*>(raw_buffer_ptr(globals_, global.offset));
  }

  // Process initialization of globals.
  void InitGlobals() {
    for (auto global : module_->globals) {
      switch (global.init.kind) {
        case WasmInitExpr::kI32Const:
          *GetRawGlobalPtr<int32_t>(global) = global.init.val.i32_const;
          break;
        case WasmInitExpr::kI64Const:
          *GetRawGlobalPtr<int64_t>(global) = global.init.val.i64_const;
          break;
        case WasmInitExpr::kF32Const:
          *GetRawGlobalPtr<float>(global) = global.init.val.f32_const;
          break;
        case WasmInitExpr::kF64Const:
          *GetRawGlobalPtr<double>(global) = global.init.val.f64_const;
          break;
        case WasmInitExpr::kGlobalIndex: {
          // Initialize with another global.
          uint32_t new_offset = global.offset;
          uint32_t old_offset =
              module_->globals[global.init.val.global_index].offset;
          TRACE("init [globals+%u] = [globals+%d]\n", global.offset,
                old_offset);
          size_t size = (global.type == kWasmI64 || global.type == kWasmF64)
                            ? sizeof(double)
                            : sizeof(int32_t);
          memcpy(raw_buffer_ptr(globals_, new_offset),
                 raw_buffer_ptr(globals_, old_offset), size);
          break;
        }
        case WasmInitExpr::kNone:
          // Happens with imported globals.
          break;
        default:
          UNREACHABLE();
          break;
      }
    }
  }

  // Allocate memory for a module instance as a new JSArrayBuffer.
  Handle<JSArrayBuffer> AllocateMemory(uint32_t min_mem_pages) {
    if (min_mem_pages > FLAG_wasm_max_mem_pages) {
      thrower_->RangeError("Out of memory: wasm memory too large");
      return Handle<JSArrayBuffer>::null();
    }
    const bool enable_guard_regions = EnableGuardRegions();
    Handle<JSArrayBuffer> mem_buffer = NewArrayBuffer(
        isolate_, min_mem_pages * WasmModule::kPageSize, enable_guard_regions);

    if (mem_buffer.is_null()) {
      thrower_->RangeError("Out of memory: wasm memory");
    }
    return mem_buffer;
  }

  bool NeedsWrappers() {
    if (module_->num_exported_functions > 0) return true;
    for (auto table_instance : table_instances_) {
      if (!table_instance.js_wrappers.is_null()) return true;
    }
    for (auto table : module_->function_tables) {
      if (table.exported) return true;
    }
    return false;
  }

  // Process the exports, creating wrappers for functions, tables, memories,
  // and globals.
  void ProcessExports(Handle<FixedArray> code_table,
                      Handle<WasmInstanceObject> instance,
                      Handle<WasmCompiledModule> compiled_module) {
    if (NeedsWrappers()) {
      // Fill the table to cache the exported JSFunction wrappers.
      js_wrappers_.insert(js_wrappers_.begin(), module_->functions.size(),
                          Handle<JSFunction>::null());
    }

    Handle<JSObject> exports_object;
    if (module_->origin == kWasmOrigin) {
      // Create the "exports" object.
      exports_object = isolate_->factory()->NewJSObjectWithNullProto();
    } else if (module_->origin == kAsmJsOrigin) {
      Handle<JSFunction> object_function = Handle<JSFunction>(
          isolate_->native_context()->object_function(), isolate_);
      exports_object = isolate_->factory()->NewJSObject(object_function);
    } else {
      UNREACHABLE();
    }
    Handle<String> exports_name =
        isolate_->factory()->InternalizeUtf8String("exports");
    JSObject::AddProperty(instance, exports_name, exports_object, NONE);

    Handle<String> foreign_init_name =
        isolate_->factory()->InternalizeUtf8String(
            wasm::AsmWasmBuilder::foreign_init_name);
    Handle<String> single_function_name =
        isolate_->factory()->InternalizeUtf8String(
            wasm::AsmWasmBuilder::single_function_name);

    PropertyDescriptor desc;
    desc.set_writable(module_->origin == kAsmJsOrigin);
    desc.set_enumerable(true);

    // Count up export indexes.
    int export_index = 0;
    for (auto exp : module_->export_table) {
      if (exp.kind == kExternalFunction) {
        ++export_index;
      }
    }

    // Store weak references to all exported functions.
    Handle<FixedArray> weak_exported_functions;
    if (compiled_module->has_weak_exported_functions()) {
      weak_exported_functions = compiled_module->weak_exported_functions();
    } else {
      weak_exported_functions =
          isolate_->factory()->NewFixedArray(export_index);
      compiled_module->set_weak_exported_functions(weak_exported_functions);
    }
    DCHECK_EQ(export_index, weak_exported_functions->length());

    // Process each export in the export table (go in reverse so asm.js
    // can skip duplicates).
    for (auto exp : base::Reversed(module_->export_table)) {
      Handle<String> name =
          WasmCompiledModule::ExtractUtf8StringFromModuleBytes(
              isolate_, compiled_module_, exp.name_offset, exp.name_length)
              .ToHandleChecked();
      Handle<JSObject> export_to;
      if (module_->origin == kAsmJsOrigin && exp.kind == kExternalFunction &&
          (String::Equals(name, foreign_init_name) ||
           String::Equals(name, single_function_name))) {
        export_to = instance;
      } else {
        export_to = exports_object;
      }

      switch (exp.kind) {
        case kExternalFunction: {
          // Wrap and export the code as a JSFunction.
          WasmFunction& function = module_->functions[exp.index];
          int func_index =
              static_cast<int>(module_->functions.size() + --export_index);
          Handle<JSFunction> js_function = js_wrappers_[exp.index];
          if (js_function.is_null()) {
            // Wrap the exported code as a JSFunction.
            Handle<Code> export_code =
                code_table->GetValueChecked<Code>(isolate_, func_index);
            MaybeHandle<String> func_name;
            if (module_->origin == kAsmJsOrigin) {
              // For modules arising from asm.js, honor the names section.
              func_name = WasmCompiledModule::ExtractUtf8StringFromModuleBytes(
                              isolate_, compiled_module_, function.name_offset,
                              function.name_length)
                              .ToHandleChecked();
            }
            js_function = WasmExportedFunction::New(
                isolate_, instance, func_name, function.func_index,
                static_cast<int>(function.sig->parameter_count()), export_code);
            js_wrappers_[exp.index] = js_function;
          }
          desc.set_value(js_function);
          Handle<WeakCell> weak_export =
              isolate_->factory()->NewWeakCell(js_function);
          DCHECK_GT(weak_exported_functions->length(), export_index);
          weak_exported_functions->set(export_index, *weak_export);
          break;
        }
        case kExternalTable: {
          // Export a table as a WebAssembly.Table object.
          TableInstance& table_instance = table_instances_[exp.index];
          WasmIndirectFunctionTable& table =
              module_->function_tables[exp.index];
          if (table_instance.table_object.is_null()) {
            uint32_t maximum =
                table.has_max ? table.max_size : FLAG_wasm_max_table_size;
            table_instance.table_object = WasmTableObject::New(
                isolate_, table.min_size, maximum, &table_instance.js_wrappers);
          }
          desc.set_value(table_instance.table_object);
          break;
        }
        case kExternalMemory: {
          // Export the memory as a WebAssembly.Memory object.
          Handle<WasmMemoryObject> memory_object;
          if (!instance->has_memory_object()) {
            // If there was no imported WebAssembly.Memory object, create one.
            Handle<JSArrayBuffer> buffer(instance->memory_buffer(), isolate_);
            memory_object = WasmMemoryObject::New(
                isolate_, buffer,
                (module_->max_mem_pages != 0) ? module_->max_mem_pages : -1);
            instance->set_memory_object(*memory_object);
          } else {
            memory_object =
                Handle<WasmMemoryObject>(instance->memory_object(), isolate_);
            DCHECK(WasmJs::IsWasmMemoryObject(isolate_, memory_object));
            memory_object->ResetInstancesLink(isolate_);
          }

          desc.set_value(memory_object);
          break;
        }
        case kExternalGlobal: {
          // Export the value of the global variable as a number.
          WasmGlobal& global = module_->globals[exp.index];
          double num = 0;
          switch (global.type) {
            case kWasmI32:
              num = *GetRawGlobalPtr<int32_t>(global);
              break;
            case kWasmF32:
              num = *GetRawGlobalPtr<float>(global);
              break;
            case kWasmF64:
              num = *GetRawGlobalPtr<double>(global);
              break;
            case kWasmI64:
              thrower_->LinkError(
                  "export of globals of type I64 is not allowed.");
              break;
            default:
              UNREACHABLE();
          }
          desc.set_value(isolate_->factory()->NewNumber(num));
          break;
        }
        default:
          UNREACHABLE();
          break;
      }

      // Skip duplicates for asm.js.
      if (module_->origin == kAsmJsOrigin) {
        v8::Maybe<bool> status = JSReceiver::HasOwnProperty(export_to, name);
        if (status.FromMaybe(false)) {
          continue;
        }
      }
      v8::Maybe<bool> status = JSReceiver::DefineOwnProperty(
          isolate_, export_to, name, &desc, Object::THROW_ON_ERROR);
      if (!status.IsJust()) {
        thrower_->LinkError("export of %.*s failed.", name->length(),
                            name->ToCString().get());
        return;
      }
    }

    if (module_->origin == kWasmOrigin) {
      v8::Maybe<bool> success = JSReceiver::SetIntegrityLevel(
          exports_object, FROZEN, Object::DONT_THROW);
      DCHECK(success.FromMaybe(false));
      USE(success);
    }
  }

  void InitializeTables(Handle<FixedArray> code_table,
                        Handle<WasmInstanceObject> instance,
                        CodeSpecialization* code_specialization) {
    int function_table_count =
        static_cast<int>(module_->function_tables.size());
    Handle<FixedArray> new_function_tables =
        isolate_->factory()->NewFixedArray(function_table_count);
    Handle<FixedArray> new_signature_tables =
        isolate_->factory()->NewFixedArray(function_table_count);
    for (int index = 0; index < function_table_count; ++index) {
      WasmIndirectFunctionTable& table = module_->function_tables[index];
      TableInstance& table_instance = table_instances_[index];
      int table_size = static_cast<int>(table.min_size);

      if (table_instance.function_table.is_null()) {
        // Create a new dispatch table if necessary.
        table_instance.function_table =
            isolate_->factory()->NewFixedArray(table_size);
        table_instance.signature_table =
            isolate_->factory()->NewFixedArray(table_size);
        for (int i = 0; i < table_size; ++i) {
          // Fill the table with invalid signature indexes so that
          // uninitialized entries will always fail the signature check.
          table_instance.signature_table->set(i,
                                              Smi::FromInt(kInvalidSigIndex));
        }
      } else {
        // Table is imported, patch table bounds check
        DCHECK(table_size <= table_instance.function_table->length());
        if (table_size < table_instance.function_table->length()) {
          code_specialization->PatchTableSize(
              table_size, table_instance.function_table->length());
        }
      }

      new_function_tables->set(static_cast<int>(index),
                               *table_instance.function_table);
      new_signature_tables->set(static_cast<int>(index),
                                *table_instance.signature_table);
    }

    FixedArray* old_function_tables =
        compiled_module_->ptr_to_function_tables();
    DCHECK_EQ(old_function_tables->length(), new_function_tables->length());
    for (int i = 0, e = new_function_tables->length(); i < e; ++i) {
      code_specialization->RelocateObject(
          handle(old_function_tables->get(i), isolate_),
          handle(new_function_tables->get(i), isolate_));
    }
    FixedArray* old_signature_tables =
        compiled_module_->ptr_to_signature_tables();
    DCHECK_EQ(old_signature_tables->length(), new_signature_tables->length());
    for (int i = 0, e = new_signature_tables->length(); i < e; ++i) {
      code_specialization->RelocateObject(
          handle(old_signature_tables->get(i), isolate_),
          handle(new_signature_tables->get(i), isolate_));
    }

    compiled_module_->set_function_tables(new_function_tables);
    compiled_module_->set_signature_tables(new_signature_tables);
  }

  void LoadTableSegments(Handle<FixedArray> code_table,
                         Handle<WasmInstanceObject> instance) {
    int function_table_count =
        static_cast<int>(module_->function_tables.size());
    for (int index = 0; index < function_table_count; ++index) {
      WasmIndirectFunctionTable& table = module_->function_tables[index];
      TableInstance& table_instance = table_instances_[index];

      Handle<FixedArray> all_dispatch_tables;
      if (!table_instance.table_object.is_null()) {
        // Get the existing dispatch table(s) with the WebAssembly.Table object.
        all_dispatch_tables = WasmTableObject::AddDispatchTable(
            isolate_, table_instance.table_object,
            Handle<WasmInstanceObject>::null(), index,
            Handle<FixedArray>::null(), Handle<FixedArray>::null());
      }

      // TODO(titzer): this does redundant work if there are multiple tables,
      // since initializations are not sorted by table index.
      for (auto table_init : module_->table_inits) {
        uint32_t base = EvalUint32InitExpr(table_init.offset);
        DCHECK(in_bounds(base, static_cast<uint32_t>(table_init.entries.size()),
                         table_instance.function_table->length()));
        for (int i = 0; i < static_cast<int>(table_init.entries.size()); ++i) {
          uint32_t func_index = table_init.entries[i];
          WasmFunction* function = &module_->functions[func_index];
          int table_index = static_cast<int>(i + base);
          int32_t sig_index = table.map.Find(function->sig);
          DCHECK_GE(sig_index, 0);
          table_instance.signature_table->set(table_index,
                                              Smi::FromInt(sig_index));
          table_instance.function_table->set(table_index,
                                             code_table->get(func_index));

          if (!all_dispatch_tables.is_null()) {
            Handle<Code> wasm_code(Code::cast(code_table->get(func_index)),
                                   isolate_);
            if (js_wrappers_[func_index].is_null()) {
              // No JSFunction entry yet exists for this function. Create one.
              // TODO(titzer): We compile JS->WASM wrappers for functions are
              // not exported but are in an exported table. This should be done
              // at module compile time and cached instead.

              Handle<Code> wrapper_code =
                  js_to_wasm_cache_.CloneOrCompileJSToWasmWrapper(
                      isolate_, module_, wasm_code, func_index);
              MaybeHandle<String> func_name;
              if (module_->origin == kAsmJsOrigin) {
                // For modules arising from asm.js, honor the names section.
                func_name =
                    WasmCompiledModule::ExtractUtf8StringFromModuleBytes(
                        isolate_, compiled_module_, function->name_offset,
                        function->name_length)
                        .ToHandleChecked();
              }
              Handle<WasmExportedFunction> js_function =
                  WasmExportedFunction::New(
                      isolate_, instance, func_name, func_index,
                      static_cast<int>(function->sig->parameter_count()),
                      wrapper_code);
              js_wrappers_[func_index] = js_function;
            }
            table_instance.js_wrappers->set(table_index,
                                            *js_wrappers_[func_index]);

            UpdateDispatchTablesInternal(isolate_, all_dispatch_tables,
                                         table_index, function, wasm_code);
          }
        }
      }

      // TODO(titzer): we add the new dispatch table at the end to avoid
      // redundant work and also because the new instance is not yet fully
      // initialized.
      if (!table_instance.table_object.is_null()) {
        // Add the new dispatch table to the WebAssembly.Table object.
        all_dispatch_tables = WasmTableObject::AddDispatchTable(
            isolate_, table_instance.table_object, instance, index,
            table_instance.function_table, table_instance.signature_table);
      }
    }
  }
};

bool wasm::IsWasmInstance(Object* object) {
  return WasmInstanceObject::IsWasmInstanceObject(object);
}

Handle<Script> wasm::GetScript(Handle<JSObject> instance) {
  WasmCompiledModule* compiled_module =
      WasmInstanceObject::cast(*instance)->compiled_module();
  return handle(compiled_module->script());
}

bool wasm::IsWasmCodegenAllowed(Isolate* isolate, Handle<Context> context) {
  return isolate->allow_code_gen_callback() == nullptr ||
         isolate->allow_code_gen_callback()(v8::Utils::ToLocal(context));
}

MaybeHandle<JSArrayBuffer> wasm::GetInstanceMemory(
    Isolate* isolate, Handle<WasmInstanceObject> object) {
  auto instance = Handle<WasmInstanceObject>::cast(object);
  if (instance->has_memory_buffer()) {
    return Handle<JSArrayBuffer>(instance->memory_buffer(), isolate);
  }
  return MaybeHandle<JSArrayBuffer>();
}

void SetInstanceMemory(Handle<WasmInstanceObject> instance,
                       JSArrayBuffer* buffer) {
  DisallowHeapAllocation no_gc;
  instance->set_memory_buffer(buffer);
  instance->compiled_module()->set_ptr_to_memory(buffer);
}

int32_t wasm::GetInstanceMemorySize(Isolate* isolate,
                                    Handle<WasmInstanceObject> instance) {
  DCHECK(IsWasmInstance(*instance));
  MaybeHandle<JSArrayBuffer> maybe_mem_buffer =
      GetInstanceMemory(isolate, instance);
  Handle<JSArrayBuffer> buffer;
  if (!maybe_mem_buffer.ToHandle(&buffer)) {
    return 0;
  } else {
    return buffer->byte_length()->Number() / WasmModule::kPageSize;
  }
}

uint32_t GetMaxInstanceMemoryPages(Isolate* isolate,
                                   Handle<WasmInstanceObject> instance) {
  if (instance->has_memory_object()) {
    Handle<WasmMemoryObject> memory_object(instance->memory_object(), isolate);
    if (memory_object->has_maximum_pages()) {
      uint32_t maximum = static_cast<uint32_t>(memory_object->maximum_pages());
      if (maximum < FLAG_wasm_max_mem_pages) return maximum;
    }
  }
  uint32_t compiled_max_pages = instance->compiled_module()->max_mem_pages();
  isolate->counters()->wasm_max_mem_pages_count()->AddSample(
      compiled_max_pages);
  if (compiled_max_pages != 0) return compiled_max_pages;
  return FLAG_wasm_max_mem_pages;
}

Handle<JSArrayBuffer> GrowMemoryBuffer(Isolate* isolate,
                                       MaybeHandle<JSArrayBuffer> buffer,
                                       uint32_t pages, uint32_t max_pages) {
  Handle<JSArrayBuffer> old_buffer;
  Address old_mem_start = nullptr;
  uint32_t old_size = 0;
  if (buffer.ToHandle(&old_buffer) && old_buffer->backing_store() != nullptr &&
      old_buffer->byte_length()->IsNumber()) {
    old_mem_start = static_cast<Address>(old_buffer->backing_store());
    DCHECK_NOT_NULL(old_mem_start);
    old_size = old_buffer->byte_length()->Number();
  }
  DCHECK(old_size + pages * WasmModule::kPageSize <=
         std::numeric_limits<uint32_t>::max());
  uint32_t new_size = old_size + pages * WasmModule::kPageSize;
  if (new_size <= old_size || max_pages * WasmModule::kPageSize < new_size ||
      FLAG_wasm_max_mem_pages * WasmModule::kPageSize < new_size) {
    return Handle<JSArrayBuffer>::null();
  }

  // TODO(gdeepti): Change the protection here instead of allocating a new
  // buffer before guard regions are turned on, see issue #5886.
  const bool enable_guard_regions =
      !old_buffer.is_null() && old_buffer->has_guard_region();
  Handle<JSArrayBuffer> new_buffer =
      NewArrayBuffer(isolate, new_size, enable_guard_regions);
  if (new_buffer.is_null()) return new_buffer;
  Address new_mem_start = static_cast<Address>(new_buffer->backing_store());
  if (old_size != 0) {
    memcpy(new_mem_start, old_mem_start, old_size);
  }
  return new_buffer;
}

void UncheckedUpdateInstanceMemory(Isolate* isolate,
                                   Handle<WasmInstanceObject> instance,
                                   Address old_mem_start, uint32_t old_size) {
  DCHECK(instance->has_memory_buffer());
  Handle<JSArrayBuffer> mem_buffer(instance->memory_buffer());
  uint32_t new_size = mem_buffer->byte_length()->Number();
  Address new_mem_start = static_cast<Address>(mem_buffer->backing_store());
  DCHECK_NOT_NULL(new_mem_start);
  Zone specialization_zone(isolate->allocator(), ZONE_NAME);
  CodeSpecialization code_specialization(isolate, &specialization_zone);
  code_specialization.RelocateMemoryReferences(old_mem_start, old_size,
                                               new_mem_start, new_size);
  code_specialization.ApplyToWholeInstance(*instance);
}

void wasm::DetachWebAssemblyMemoryBuffer(Isolate* isolate,
                                         Handle<JSArrayBuffer> buffer) {
  int64_t byte_length =
      buffer->byte_length()->IsNumber()
          ? static_cast<uint32_t>(buffer->byte_length()->Number())
          : 0;
  if (buffer.is_null() || byte_length == 0) return;
  const bool has_guard_regions = buffer->has_guard_region();
  const bool is_external = buffer->is_external();
  void* backing_store = buffer->backing_store();
  DCHECK(!buffer->is_neuterable());
  if (!has_guard_regions && !is_external) {
    buffer->set_is_external(true);
    isolate->heap()->UnregisterArrayBuffer(*buffer);
  }
  buffer->set_is_neuterable(true);
  buffer->Neuter();
  if (has_guard_regions) {
    base::OS::Free(backing_store, RoundUp(i::wasm::kWasmMaxHeapOffset,
                                          base::OS::CommitPageSize()));
    reinterpret_cast<v8::Isolate*>(isolate)
        ->AdjustAmountOfExternalAllocatedMemory(-byte_length);
  } else if (!has_guard_regions && !is_external) {
    isolate->array_buffer_allocator()->Free(backing_store, byte_length);
  }
}

int32_t wasm::GrowWebAssemblyMemory(Isolate* isolate,
                                    Handle<WasmMemoryObject> receiver,
                                    uint32_t pages) {
  DCHECK(WasmJs::IsWasmMemoryObject(isolate, receiver));
  Handle<WasmMemoryObject> memory_object =
      handle(WasmMemoryObject::cast(*receiver));
  MaybeHandle<JSArrayBuffer> memory_buffer = handle(memory_object->buffer());
  Handle<JSArrayBuffer> old_buffer;
  uint32_t old_size = 0;
  Address old_mem_start = nullptr;
  // Force byte_length to 0, if byte_length fails IsNumber() check.
  if (memory_buffer.ToHandle(&old_buffer) &&
      old_buffer->backing_store() != nullptr &&
      old_buffer->byte_length()->IsNumber()) {
    old_size = old_buffer->byte_length()->Number();
    old_mem_start = static_cast<Address>(old_buffer->backing_store());
  }
  Handle<JSArrayBuffer> new_buffer;
  // Return current size if grow by 0
  if (pages == 0) {
    if (!old_buffer.is_null() && old_buffer->backing_store() != nullptr) {
      new_buffer = SetupArrayBuffer(isolate, old_buffer->backing_store(),
                                    old_size, old_buffer->is_external(),
                                    old_buffer->has_guard_region());
      memory_object->set_buffer(*new_buffer);
      old_buffer->set_is_neuterable(true);
      if (!old_buffer->has_guard_region()) {
        old_buffer->set_is_external(true);
        isolate->heap()->UnregisterArrayBuffer(*old_buffer);
      }
      // Neuter but don't free the memory because it is now being used by
      // new_buffer.
      old_buffer->Neuter();
    }
    DCHECK(old_size % WasmModule::kPageSize == 0);
    return (old_size / WasmModule::kPageSize);
  }
  if (!memory_object->has_instances_link()) {
    // Memory object does not have an instance associated with it, just grow
    uint32_t max_pages;
    if (memory_object->has_maximum_pages()) {
      max_pages = static_cast<uint32_t>(memory_object->maximum_pages());
      if (FLAG_wasm_max_mem_pages < max_pages) return -1;
    } else {
      max_pages = FLAG_wasm_max_mem_pages;
    }
    new_buffer = GrowMemoryBuffer(isolate, memory_buffer, pages, max_pages);
    if (new_buffer.is_null()) return -1;
  } else {
    Handle<WasmInstanceWrapper> instance_wrapper(
        memory_object->instances_link());
    DCHECK(WasmInstanceWrapper::IsWasmInstanceWrapper(*instance_wrapper));
    DCHECK(instance_wrapper->has_instance());
    Handle<WasmInstanceObject> instance = instance_wrapper->instance_object();
    DCHECK(IsWasmInstance(*instance));
    uint32_t max_pages = GetMaxInstanceMemoryPages(isolate, instance);

    // Grow memory object buffer and update instances associated with it.
    new_buffer = GrowMemoryBuffer(isolate, memory_buffer, pages, max_pages);
    if (new_buffer.is_null()) return -1;
    DCHECK(!instance_wrapper->has_previous());
    SetInstanceMemory(instance, *new_buffer);
    UncheckedUpdateInstanceMemory(isolate, instance, old_mem_start, old_size);
    while (instance_wrapper->has_next()) {
      instance_wrapper = instance_wrapper->next_wrapper();
      DCHECK(WasmInstanceWrapper::IsWasmInstanceWrapper(*instance_wrapper));
      Handle<WasmInstanceObject> instance = instance_wrapper->instance_object();
      DCHECK(IsWasmInstance(*instance));
      SetInstanceMemory(instance, *new_buffer);
      UncheckedUpdateInstanceMemory(isolate, instance, old_mem_start, old_size);
    }
  }
  memory_object->set_buffer(*new_buffer);
  DCHECK(old_size % WasmModule::kPageSize == 0);
  return (old_size / WasmModule::kPageSize);
}

int32_t wasm::GrowMemory(Isolate* isolate, Handle<WasmInstanceObject> instance,
                         uint32_t pages) {
  if (!IsWasmInstance(*instance)) return -1;
  if (pages == 0) return GetInstanceMemorySize(isolate, instance);
  Handle<WasmInstanceObject> instance_obj(WasmInstanceObject::cast(*instance));
  if (!instance_obj->has_memory_object()) {
    // No other instances to grow, grow just the one.
    MaybeHandle<JSArrayBuffer> instance_buffer =
        GetInstanceMemory(isolate, instance);
    Handle<JSArrayBuffer> old_buffer;
    uint32_t old_size = 0;
    Address old_mem_start = nullptr;
    if (instance_buffer.ToHandle(&old_buffer) &&
        old_buffer->backing_store() != nullptr) {
      old_size = old_buffer->byte_length()->Number();
      old_mem_start = static_cast<Address>(old_buffer->backing_store());
    }
    uint32_t max_pages = GetMaxInstanceMemoryPages(isolate, instance_obj);
    Handle<JSArrayBuffer> buffer =
        GrowMemoryBuffer(isolate, instance_buffer, pages, max_pages);
    if (buffer.is_null()) return -1;
    SetInstanceMemory(instance, *buffer);
    UncheckedUpdateInstanceMemory(isolate, instance, old_mem_start, old_size);
    DCHECK(old_size % WasmModule::kPageSize == 0);
    return (old_size / WasmModule::kPageSize);
  } else {
    return GrowWebAssemblyMemory(isolate, handle(instance_obj->memory_object()),
                                 pages);
  }
}

void wasm::GrowDispatchTables(Isolate* isolate,
                              Handle<FixedArray> dispatch_tables,
                              uint32_t old_size, uint32_t count) {
  DCHECK_EQ(0, dispatch_tables->length() % 4);

  Zone specialization_zone(isolate->allocator(), ZONE_NAME);
  for (int i = 0; i < dispatch_tables->length(); i += 4) {
    Handle<FixedArray> old_function_table(
        FixedArray::cast(dispatch_tables->get(i + 2)));
    Handle<FixedArray> old_signature_table(
        FixedArray::cast(dispatch_tables->get(i + 3)));
    Handle<FixedArray> new_function_table =
        isolate->factory()->CopyFixedArrayAndGrow(old_function_table, count);
    Handle<FixedArray> new_signature_table =
        isolate->factory()->CopyFixedArrayAndGrow(old_signature_table, count);

    // Update dispatch tables with new function/signature tables
    dispatch_tables->set(i + 2, *new_function_table);
    dispatch_tables->set(i + 3, *new_signature_table);

    // Patch the code of the respective instance.
    CodeSpecialization code_specialization(isolate, &specialization_zone);
    code_specialization.PatchTableSize(old_size, old_size + count);
    code_specialization.RelocateObject(old_function_table, new_function_table);
    code_specialization.RelocateObject(old_signature_table,
                                       new_signature_table);
    code_specialization.ApplyToWholeInstance(
        WasmInstanceObject::cast(dispatch_tables->get(i)));
  }
}

void testing::ValidateInstancesChain(Isolate* isolate,
                                     Handle<WasmModuleObject> module_obj,
                                     int instance_count) {
  CHECK_GE(instance_count, 0);
  DisallowHeapAllocation no_gc;
  WasmCompiledModule* compiled_module = module_obj->compiled_module();
  CHECK_EQ(JSObject::cast(compiled_module->ptr_to_weak_wasm_module()->value()),
           *module_obj);
  Object* prev = nullptr;
  int found_instances = compiled_module->has_weak_owning_instance() ? 1 : 0;
  WasmCompiledModule* current_instance = compiled_module;
  while (current_instance->has_weak_next_instance()) {
    CHECK((prev == nullptr && !current_instance->has_weak_prev_instance()) ||
          current_instance->ptr_to_weak_prev_instance()->value() == prev);
    CHECK_EQ(current_instance->ptr_to_weak_wasm_module()->value(), *module_obj);
    CHECK(IsWasmInstance(
        current_instance->ptr_to_weak_owning_instance()->value()));
    prev = current_instance;
    current_instance = WasmCompiledModule::cast(
        current_instance->ptr_to_weak_next_instance()->value());
    ++found_instances;
    CHECK_LE(found_instances, instance_count);
  }
  CHECK_EQ(found_instances, instance_count);
}

void testing::ValidateModuleState(Isolate* isolate,
                                  Handle<WasmModuleObject> module_obj) {
  DisallowHeapAllocation no_gc;
  WasmCompiledModule* compiled_module = module_obj->compiled_module();
  CHECK(compiled_module->has_weak_wasm_module());
  CHECK_EQ(compiled_module->ptr_to_weak_wasm_module()->value(), *module_obj);
  CHECK(!compiled_module->has_weak_prev_instance());
  CHECK(!compiled_module->has_weak_next_instance());
  CHECK(!compiled_module->has_weak_owning_instance());
}

void testing::ValidateOrphanedInstance(Isolate* isolate,
                                       Handle<WasmInstanceObject> instance) {
  DisallowHeapAllocation no_gc;
  WasmCompiledModule* compiled_module = instance->compiled_module();
  CHECK(compiled_module->has_weak_wasm_module());
  CHECK(compiled_module->ptr_to_weak_wasm_module()->cleared());
}

Handle<JSArray> wasm::GetImports(Isolate* isolate,
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
  Handle<JSArray> array_object = factory->NewJSArray(FAST_ELEMENTS, 0, 0);
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
            isolate, compiled_module, import.module_name_offset,
            import.module_name_length);

    MaybeHandle<String> import_name =
        WasmCompiledModule::ExtractUtf8StringFromModuleBytes(
            isolate, compiled_module, import.field_name_offset,
            import.field_name_length);

    JSObject::AddProperty(entry, module_string, import_module.ToHandleChecked(),
                          NONE);
    JSObject::AddProperty(entry, name_string, import_name.ToHandleChecked(),
                          NONE);
    JSObject::AddProperty(entry, kind_string, import_kind, NONE);

    storage->set(index, *entry);
  }

  return array_object;
}

Handle<JSArray> wasm::GetExports(Isolate* isolate,
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
  Handle<JSArray> array_object = factory->NewJSArray(FAST_ELEMENTS, 0, 0);
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
            isolate, compiled_module, exp.name_offset, exp.name_length);

    JSObject::AddProperty(entry, name_string, export_name.ToHandleChecked(),
                          NONE);
    JSObject::AddProperty(entry, kind_string, export_kind, NONE);

    storage->set(index, *entry);
  }

  return array_object;
}

Handle<JSArray> wasm::GetCustomSections(Isolate* isolate,
                                        Handle<WasmModuleObject> module_object,
                                        Handle<String> name,
                                        ErrorThrower* thrower) {
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
  for (auto section : custom_sections) {
    MaybeHandle<String> section_name =
        WasmCompiledModule::ExtractUtf8StringFromModuleBytes(
            isolate, compiled_module, section.name_offset, section.name_length);

    if (!name->Equals(*section_name.ToHandleChecked())) continue;

    // Make a copy of the payload data in the section.
    bool is_external;  // Set by TryAllocateBackingStore
    void* memory = TryAllocateBackingStore(isolate, section.payload_length,
                                           false, is_external);

    Handle<Object> section_data = factory->undefined_value();
    if (memory) {
      Handle<JSArrayBuffer> buffer = isolate->factory()->NewJSArrayBuffer();
      JSArrayBuffer::Setup(buffer, isolate, is_external, memory,
                           static_cast<int>(section.payload_length));
      DisallowHeapAllocation no_gc;  // for raw access to string bytes.
      Handle<SeqOneByteString> module_bytes(compiled_module->module_bytes(),
                                            isolate);
      const byte* start =
          reinterpret_cast<const byte*>(module_bytes->GetCharsAddress());
      memcpy(memory, start + section.payload_offset, section.payload_length);
      section_data = buffer;
    } else {
      thrower->RangeError("out of memory allocating custom section data");
      return Handle<JSArray>();
    }

    matching_sections.push_back(section_data);
  }

  int num_custom_sections = static_cast<int>(matching_sections.size());
  Handle<JSArray> array_object = factory->NewJSArray(FAST_ELEMENTS, 0, 0);
  Handle<FixedArray> storage = factory->NewFixedArray(num_custom_sections);
  JSArray::SetContent(array_object, storage);
  array_object->set_length(Smi::FromInt(num_custom_sections));

  for (int i = 0; i < num_custom_sections; i++) {
    storage->set(i, *matching_sections[i]);
  }

  return array_object;
}

bool wasm::SyncValidate(Isolate* isolate, ErrorThrower* thrower,
                        const ModuleWireBytes& bytes) {
  if (bytes.start() == nullptr || bytes.length() == 0) return false;
  ModuleResult result =
      DecodeWasmModule(isolate, bytes.start(), bytes.end(), true, kWasmOrigin);
  if (result.val) delete result.val;
  return result.ok();
}

MaybeHandle<WasmModuleObject> wasm::SyncCompileTranslatedAsmJs(
    Isolate* isolate, ErrorThrower* thrower, const ModuleWireBytes& bytes,
    Handle<Script> asm_js_script,
    Vector<const byte> asm_js_offset_table_bytes) {

  ModuleResult result = DecodeWasmModule(isolate, bytes.start(), bytes.end(),
                                         false, kAsmJsOrigin);
  if (result.failed()) {
    // TODO(titzer): use Result<std::unique_ptr<const WasmModule*>>?
    if (result.val) delete result.val;
    thrower->CompileFailed("Wasm decoding failed", result);
    return {};
  }

  CompilationHelper helper(isolate, const_cast<WasmModule*>(result.val));
  return helper.CompileToModuleObject(thrower, bytes, asm_js_script,
                                      asm_js_offset_table_bytes);
}

MaybeHandle<WasmModuleObject> wasm::SyncCompile(Isolate* isolate,
                                                ErrorThrower* thrower,
                                                const ModuleWireBytes& bytes) {
  if (!IsWasmCodegenAllowed(isolate, isolate->native_context())) {
    thrower->CompileError("Wasm code generation disallowed in this context");
    return {};
  }

  ModuleResult result =
      DecodeWasmModule(isolate, bytes.start(), bytes.end(), false, kWasmOrigin);
  if (result.failed()) {
    if (result.val) delete result.val;
    thrower->CompileFailed("Wasm decoding failed", result);
    return {};
  }

  CompilationHelper helper(isolate, const_cast<WasmModule*>(result.val));
  return helper.CompileToModuleObject(thrower, bytes, Handle<Script>(),
                                      Vector<const byte>());
}

MaybeHandle<WasmInstanceObject> wasm::SyncInstantiate(
    Isolate* isolate, ErrorThrower* thrower,
    Handle<WasmModuleObject> module_object, MaybeHandle<JSReceiver> imports,
    MaybeHandle<JSArrayBuffer> memory) {
  InstantiationHelper helper(isolate, thrower, module_object, imports, memory);
  return helper.Build();
}

void RejectPromise(Isolate* isolate, ErrorThrower* thrower,
                   Handle<JSPromise> promise) {
  v8::Local<v8::Promise::Resolver> resolver =
      v8::Utils::PromiseToLocal(promise).As<v8::Promise::Resolver>();
  Handle<Context> context(isolate->context(), isolate);
  resolver->Reject(v8::Utils::ToLocal(context),
                   v8::Utils::ToLocal(thrower->Reify()));
}

void ResolvePromise(Isolate* isolate, Handle<JSPromise> promise,
                    Handle<Object> result) {
  v8::Local<v8::Promise::Resolver> resolver =
      v8::Utils::PromiseToLocal(promise).As<v8::Promise::Resolver>();
  Handle<Context> context(isolate->context(), isolate);
  resolver->Resolve(v8::Utils::ToLocal(context), v8::Utils::ToLocal(result));
}

void wasm::AsyncCompile(Isolate* isolate, Handle<JSPromise> promise,
                        const ModuleWireBytes& bytes) {
  ErrorThrower thrower(isolate, nullptr);
  MaybeHandle<WasmModuleObject> module_object =
      SyncCompile(isolate, &thrower, bytes);
  if (thrower.error()) {
    RejectPromise(isolate, &thrower, promise);
    return;
  }
  ResolvePromise(isolate, promise, module_object.ToHandleChecked());
}

void wasm::AsyncInstantiate(Isolate* isolate, Handle<JSPromise> promise,
                            Handle<WasmModuleObject> module_object,
                            MaybeHandle<JSReceiver> imports) {
  ErrorThrower thrower(isolate, nullptr);
  MaybeHandle<WasmInstanceObject> instance_object = SyncInstantiate(
      isolate, &thrower, module_object, imports, Handle<JSArrayBuffer>::null());
  if (thrower.error()) {
    RejectPromise(isolate, &thrower, promise);
    return;
  }
  ResolvePromise(isolate, promise, instance_object.ToHandleChecked());
}

void wasm::AsyncCompileAndInstantiate(Isolate* isolate,
                                      Handle<JSPromise> promise,
                                      const ModuleWireBytes& bytes,
                                      MaybeHandle<JSReceiver> imports) {
  ErrorThrower thrower(isolate, nullptr);

  // Compile the module.
  MaybeHandle<WasmModuleObject> module_object =
      SyncCompile(isolate, &thrower, bytes);
  if (thrower.error()) {
    RejectPromise(isolate, &thrower, promise);
    return;
  }
  Handle<WasmModuleObject> module = module_object.ToHandleChecked();

  // Instantiate the module.
  MaybeHandle<WasmInstanceObject> instance_object = SyncInstantiate(
      isolate, &thrower, module, imports, Handle<JSArrayBuffer>::null());
  if (thrower.error()) {
    RejectPromise(isolate, &thrower, promise);
    return;
  }

  Handle<JSFunction> object_function =
      Handle<JSFunction>(isolate->native_context()->object_function(), isolate);
  Handle<JSObject> ret =
      isolate->factory()->NewJSObject(object_function, TENURED);
  Handle<String> module_property_name =
      isolate->factory()->InternalizeUtf8String("module");
  Handle<String> instance_property_name =
      isolate->factory()->InternalizeUtf8String("instance");
  JSObject::AddProperty(ret, module_property_name, module, NONE);
  JSObject::AddProperty(ret, instance_property_name,
                        instance_object.ToHandleChecked(), NONE);

  ResolvePromise(isolate, promise, ret);
}
