/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 *
 * Copyright 2015 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "wasm/WasmGenerator.h"

#include "mozilla/CheckedInt.h"
#include "mozilla/EnumeratedRange.h"
#include "mozilla/SHA1.h"

#include <algorithm>

#include "wasm/WasmBaselineCompile.h"
#include "wasm/WasmCompile.h"
#include "wasm/WasmIonCompile.h"
#include "wasm/WasmStubs.h"

#include "jit/MacroAssembler-inl.h"

using namespace js;
using namespace js::jit;
using namespace js::wasm;

using mozilla::CheckedInt;
using mozilla::MakeEnumeratedRange;

bool
CompiledCode::swap(MacroAssembler& masm)
{
    MOZ_ASSERT(bytes.empty());
    if (!masm.swapBuffer(bytes))
        return false;

    callSites.swap(masm.callSites());
    callSiteTargets.swap(masm.callSiteTargets());
    trapSites.swap(masm.trapSites());
    callFarJumps.swap(masm.callFarJumps());
    trapFarJumps.swap(masm.trapFarJumps());
    memoryAccesses.swap(masm.memoryAccesses());
    symbolicAccesses.swap(masm.symbolicAccesses());
    codeLabels.swap(masm.codeLabels());
    return true;
}

// ****************************************************************************
// ModuleGenerator

static const unsigned GENERATOR_LIFO_DEFAULT_CHUNK_SIZE = 4 * 1024;
static const unsigned COMPILATION_LIFO_DEFAULT_CHUNK_SIZE = 64 * 1024;
static const uint32_t BAD_CODE_RANGE = UINT32_MAX;

ModuleGenerator::ModuleGenerator(const CompileArgs& args, ModuleEnvironment* env,
                                 Atomic<bool>* cancelled, UniqueChars* error)
  : compileArgs_(&args),
    error_(error),
    cancelled_(cancelled),
    env_(env),
    linkDataTier_(nullptr),
    metadataTier_(nullptr),
    taskState_(mutexid::WasmCompileTaskState),
    lifo_(GENERATOR_LIFO_DEFAULT_CHUNK_SIZE),
    masmAlloc_(&lifo_),
    masm_(MacroAssembler::WasmToken(), masmAlloc_),
    trapCodeOffsets_(),
    debugTrapCodeOffset_(),
    lastPatchedCallSite_(0),
    startOfUnpatchedCallsites_(0),
    parallel_(false),
    outstanding_(0),
    currentTask_(nullptr),
    batchedBytecode_(0),
    startedFuncDefs_(false),
    finishedFuncDefs_(false)
{
    MOZ_ASSERT(IsCompilingWasm());
    std::fill(trapCodeOffsets_.begin(), trapCodeOffsets_.end(), 0);
}

ModuleGenerator::~ModuleGenerator()
{
    MOZ_ASSERT_IF(finishedFuncDefs_, !batchedBytecode_);
    MOZ_ASSERT_IF(finishedFuncDefs_, !currentTask_);

    if (parallel_) {
        if (outstanding_) {
            // Remove any pending compilation tasks from the worklist.
            {
                AutoLockHelperThreadState lock;
                CompileTaskPtrFifo& worklist = HelperThreadState().wasmWorklist(lock, mode());
                auto pred = [this](CompileTask* task) { return &task->state == &taskState_; };
                size_t removed = worklist.eraseIf(pred);
                MOZ_ASSERT(outstanding_ >= removed);
                outstanding_ -= removed;
            }

            // Wait until all active compilation tasks have finished.
            {
                auto taskState = taskState_.lock();
                while (true) {
                    MOZ_ASSERT(outstanding_ >= taskState->finished.length());
                    outstanding_ -= taskState->finished.length();
                    taskState->finished.clear();

                    MOZ_ASSERT(outstanding_ >= taskState->numFailed);
                    outstanding_ -= taskState->numFailed;
                    taskState->numFailed = 0;

                    if (!outstanding_)
                        break;

                    taskState.wait(taskState->failedOrFinished);
                }
            }
        }
    } else {
        MOZ_ASSERT(!outstanding_);
    }

    // Propagate error state.
    if (error_ && !*error_)
        *error_ = Move(taskState_.lock()->errorMessage);
}

bool
ModuleGenerator::allocateGlobalBytes(uint32_t bytes, uint32_t align, uint32_t* globalDataOffset)
{
    MOZ_ASSERT(!startedFuncDefs_);

    CheckedInt<uint32_t> newGlobalDataLength(metadata_->globalDataLength);

    newGlobalDataLength += ComputeByteAlignment(newGlobalDataLength.value(), align);
    if (!newGlobalDataLength.isValid())
        return false;

    *globalDataOffset = newGlobalDataLength.value();
    newGlobalDataLength += bytes;

    if (!newGlobalDataLength.isValid())
        return false;

    metadata_->globalDataLength = newGlobalDataLength.value();
    return true;
}

bool
ModuleGenerator::init(size_t codeSectionSize, Metadata* maybeAsmJSMetadata)
{
    // Perform fallible metadata, linkdata, assumption allocations.

    if (maybeAsmJSMetadata) {
        MOZ_ASSERT(isAsmJS());
        metadataTier_ = &maybeAsmJSMetadata->metadata(tier());
        metadata_ = maybeAsmJSMetadata;
    } else {
        MOZ_ASSERT(!isAsmJS());
        auto metadataTier = js::MakeUnique<MetadataTier>(tier());
        if (!metadataTier)
            return false;
        metadataTier_ = metadataTier.get();
        metadata_ = js_new<Metadata>(Move(metadataTier));
        if (!metadata_)
            return false;
    }

    if (compileArgs_->scriptedCaller.filename) {
        metadata_->filename = DuplicateString(compileArgs_->scriptedCaller.filename.get());
        if (!metadata_->filename)
            return false;
    }

    if (!linkData_.initTier1(tier(), *metadata_))
        return false;

    linkDataTier_ = &linkData_.linkData(tier());

    if (!assumptions_.clone(compileArgs_->assumptions))
        return false;

    // The funcToCodeRange_ maps function indices to code-range indices and all
    // elements will be initialized by the time module generation is finished.

    if (!funcToCodeRange_.appendN(BAD_CODE_RANGE, env_->funcSigs.length()))
        return false;

    // Pre-reserve space for large Vectors to avoid the significant cost of the
    // final reallocs. In particular, the MacroAssembler can be enormous, so be
    // extra conservative. Note, podResizeToFit calls at the end will trim off
    // unneeded capacity.

    if (!masm_.reserve(size_t(1.2 * EstimateCompiledCodeSize(tier(), codeSectionSize))))
        return false;

    if (!metadataTier_->codeRanges.reserve(2 * env_->numFuncDefs()))
        return false;

    const size_t CallSitesPerByteCode = 10;
    if (!metadataTier_->callSites.reserve(codeSectionSize / CallSitesPerByteCode))
        return false;

    const size_t MemoryAccessesPerByteCode = 10;
    if (!metadataTier_->memoryAccesses.reserve(codeSectionSize / MemoryAccessesPerByteCode))
        return false;

    // Allocate space in TlsData for declarations that need it.

    MOZ_ASSERT(metadata_->globalDataLength == 0);

    for (size_t i = 0; i < env_->funcImportGlobalDataOffsets.length(); i++) {
        uint32_t globalDataOffset;
        if (!allocateGlobalBytes(sizeof(FuncImportTls), sizeof(void*), &globalDataOffset))
            return false;

        env_->funcImportGlobalDataOffsets[i] = globalDataOffset;

        Sig copy;
        if (!copy.clone(*env_->funcSigs[i]))
            return false;
        if (!metadataTier_->funcImports.emplaceBack(Move(copy), globalDataOffset))
            return false;
    }

    for (TableDesc& table : env_->tables) {
        if (!allocateGlobalBytes(sizeof(TableTls), sizeof(void*), &table.globalDataOffset))
            return false;
    }

    if (!isAsmJS()) {
        for (SigWithId& sig : env_->sigs) {
            if (SigIdDesc::isGlobal(sig)) {
                uint32_t globalDataOffset;
                if (!allocateGlobalBytes(sizeof(void*), sizeof(void*), &globalDataOffset))
                    return false;

                sig.id = SigIdDesc::global(sig, globalDataOffset);

                Sig copy;
                if (!copy.clone(sig))
                    return false;

                if (!metadata_->sigIds.emplaceBack(Move(copy), sig.id))
                    return false;
            } else {
                sig.id = SigIdDesc::immediate(sig);
            }
        }
    }

    for (GlobalDesc& global : env_->globals) {
        if (global.isConstant())
            continue;

        uint32_t width = SizeOf(global.type());

        uint32_t globalDataOffset;
        if (!allocateGlobalBytes(width, width, &globalDataOffset))
            return false;

        global.setOffset(globalDataOffset);
    }

    // Accumulate all exported functions, whether by explicit export or
    // implicitly by being an element of an external (imported or exported)
    // table or by being the start function. The FuncExportVector stored in
    // Metadata needs to be sorted (to allow O(log(n)) lookup at runtime) and
    // deduplicated, so use an intermediate vector to sort and de-duplicate.

    Uint32Vector exportedFuncs;

    for (const Export& exp : env_->exports) {
        if (exp.kind() == DefinitionKind::Function) {
            if (!exportedFuncs.append(exp.funcIndex()))
                return false;
        }
    }

    for (ElemSegment& elems : env_->elemSegments) {
        if (env_->tables[elems.tableIndex].external) {
            if (!exportedFuncs.appendAll(elems.elemFuncIndices))
                return false;
        }
    }

    if (env_->startFuncIndex && !exportedFuncs.append(*env_->startFuncIndex))
        return false;

    std::sort(exportedFuncs.begin(), exportedFuncs.end());
    auto* newEnd = std::unique(exportedFuncs.begin(), exportedFuncs.end());
    exportedFuncs.erase(newEnd, exportedFuncs.end());

    if (!metadataTier_->funcExports.reserve(exportedFuncs.length()))
        return false;

    for (uint32_t funcIndex : exportedFuncs) {
        Sig sig;
        if (!sig.clone(*env_->funcSigs[funcIndex]))
            return false;
        metadataTier_->funcExports.infallibleEmplaceBack(Move(sig), funcIndex);
    }

    return true;
}

bool
ModuleGenerator::funcIsCompiled(uint32_t funcIndex) const
{
    return funcToCodeRange_[funcIndex] != BAD_CODE_RANGE;
}

const CodeRange&
ModuleGenerator::funcCodeRange(uint32_t funcIndex) const
{
    MOZ_ASSERT(funcIsCompiled(funcIndex));
    const CodeRange& cr = metadataTier_->codeRanges[funcToCodeRange_[funcIndex]];
    MOZ_ASSERT(cr.isFunction());
    return cr;
}

static bool
InRange(uint32_t caller, uint32_t callee)
{
    // We assume JumpImmediateRange is defined conservatively enough that the
    // slight difference between 'caller' (which is really the return address
    // offset) and the actual base of the relative displacement computation
    // isn't significant.
    uint32_t range = Min(JitOptions.jumpThreshold, JumpImmediateRange);
    if (caller < callee)
        return callee - caller < range;
    return caller - callee < range;
}

typedef HashMap<uint32_t, uint32_t, DefaultHasher<uint32_t>, SystemAllocPolicy> OffsetMap;
typedef EnumeratedArray<Trap, Trap::Limit, Maybe<uint32_t>> TrapOffsetArray;

bool
ModuleGenerator::linkCallSites()
{
    masm_.haltingAlign(CodeAlignment);

    // Create far jumps for calls that have relative offsets that may otherwise
    // go out of range. Far jumps are created for two cases: direct calls
    // between function definitions and calls to trap exits by trap out-of-line
    // paths. Far jump code is shared when possible to reduce bloat. This method
    // is called both between function bodies (at a frequency determined by the
    // ISA's jump range) and once at the very end of a module's codegen after
    // all possible calls/traps have been emitted.

    OffsetMap existingCallFarJumps;
    if (!existingCallFarJumps.init())
        return false;

    TrapOffsetArray existingTrapFarJumps;

    for (; lastPatchedCallSite_ < metadataTier_->callSites.length(); lastPatchedCallSite_++) {
        const CallSite& callSite = metadataTier_->callSites[lastPatchedCallSite_];
        const CallSiteTarget& target = callSiteTargets_[lastPatchedCallSite_];
        uint32_t callerOffset = callSite.returnAddressOffset();
        switch (callSite.kind()) {
          case CallSiteDesc::Dynamic:
          case CallSiteDesc::Symbolic:
            break;
          case CallSiteDesc::Func: {
            if (funcIsCompiled(target.funcIndex())) {
                uint32_t calleeOffset = funcCodeRange(target.funcIndex()).funcNormalEntry();
                if (InRange(callerOffset, calleeOffset)) {
                    masm_.patchCall(callerOffset, calleeOffset);
                    break;
                }
            }

            OffsetMap::AddPtr p = existingCallFarJumps.lookupForAdd(target.funcIndex());
            if (!p) {
                Offsets offsets;
                offsets.begin = masm_.currentOffset();
                if (!callFarJumps_.emplaceBack(target.funcIndex(), masm_.farJumpWithPatch()))
                    return false;
                offsets.end = masm_.currentOffset();
                if (masm_.oom())
                    return false;
                if (!metadataTier_->codeRanges.emplaceBack(CodeRange::FarJumpIsland, offsets))
                    return false;
                if (!existingCallFarJumps.add(p, target.funcIndex(), offsets.begin))
                    return false;
            }

            masm_.patchCall(callerOffset, p->value());
            break;
          }
          case CallSiteDesc::TrapExit: {
            if (!existingTrapFarJumps[target.trap()]) {
                // See MacroAssembler::wasmEmitTrapOutOfLineCode for why we must
                // reload the TLS register on this path.
                Offsets offsets;
                offsets.begin = masm_.currentOffset();
                masm_.loadPtr(Address(FramePointer, offsetof(Frame, tls)), WasmTlsReg);
                if (!trapFarJumps_.emplaceBack(target.trap(), masm_.farJumpWithPatch()))
                    return false;
                offsets.end = masm_.currentOffset();
                if (masm_.oom())
                    return false;
                if (!metadataTier_->codeRanges.emplaceBack(CodeRange::FarJumpIsland, offsets))
                    return false;
                existingTrapFarJumps[target.trap()] = Some(offsets.begin);
            }

            masm_.patchCall(callerOffset, *existingTrapFarJumps[target.trap()]);
            break;
          }
          case CallSiteDesc::Breakpoint:
          case CallSiteDesc::EnterFrame:
          case CallSiteDesc::LeaveFrame: {
            Uint32Vector& jumps = metadataTier_->debugTrapFarJumpOffsets;
            if (jumps.empty() || !InRange(jumps.back(), callerOffset)) {
                // See BaseCompiler::insertBreakablePoint for why we must
                // reload the TLS register on this path.
                Offsets offsets;
                offsets.begin = masm_.currentOffset();
                masm_.loadPtr(Address(FramePointer, offsetof(Frame, tls)), WasmTlsReg);
                CodeOffset jumpOffset = masm_.farJumpWithPatch();
                offsets.end = masm_.currentOffset();
                if (masm_.oom())
                    return false;
                if (!metadataTier_->codeRanges.emplaceBack(CodeRange::FarJumpIsland, offsets))
                    return false;
                if (!debugTrapFarJumps_.emplaceBack(jumpOffset))
                    return false;
                if (!jumps.emplaceBack(offsets.begin))
                    return false;
            }
            break;
          }
        }
    }

    masm_.flushBuffer();
    return !masm_.oom();
}

void
ModuleGenerator::noteCodeRange(uint32_t codeRangeIndex, const CodeRange& codeRange)
{
    switch (codeRange.kind()) {
      case CodeRange::Function:
        MOZ_ASSERT(funcToCodeRange_[codeRange.funcIndex()] == BAD_CODE_RANGE);
        funcToCodeRange_[codeRange.funcIndex()] = codeRangeIndex;
        break;
      case CodeRange::Entry:
        metadataTier_->lookupFuncExport(codeRange.funcIndex()).initEntryOffset(codeRange.begin());
        break;
      case CodeRange::ImportJitExit:
        metadataTier_->funcImports[codeRange.funcIndex()].initJitExitOffset(codeRange.begin());
        break;
      case CodeRange::ImportInterpExit:
        metadataTier_->funcImports[codeRange.funcIndex()].initInterpExitOffset(codeRange.begin());
        break;
      case CodeRange::TrapExit:
        MOZ_ASSERT(!trapCodeOffsets_[codeRange.trap()]);
        trapCodeOffsets_[codeRange.trap()] = codeRange.begin();
        break;
      case CodeRange::DebugTrap:
        MOZ_ASSERT(!debugTrapCodeOffset_);
        debugTrapCodeOffset_ = codeRange.begin();
        break;
      case CodeRange::OutOfBoundsExit:
        MOZ_ASSERT(!linkDataTier_->outOfBoundsOffset);
        linkDataTier_->outOfBoundsOffset = codeRange.begin();
        break;
      case CodeRange::UnalignedExit:
        MOZ_ASSERT(!linkDataTier_->unalignedAccessOffset);
        linkDataTier_->unalignedAccessOffset = codeRange.begin();
        break;
      case CodeRange::Interrupt:
        MOZ_ASSERT(!linkDataTier_->interruptOffset);
        linkDataTier_->interruptOffset = codeRange.begin();
        break;
      case CodeRange::Throw:
        // Jumped to by other stubs, so nothing to do.
        break;
      case CodeRange::FarJumpIsland:
      case CodeRange::BuiltinThunk:
        MOZ_CRASH("Unexpected CodeRange kind");
    }
}

template <class Vec, class Op>
static bool
AppendForEach(Vec* dstVec, const Vec& srcVec, Op op)
{
    if (!dstVec->growByUninitialized(srcVec.length()))
        return false;

    typedef typename Vec::ElementType T;

    const T* src = srcVec.begin();

    T* dstBegin = dstVec->begin();
    T* dstEnd = dstVec->end();
    T* dstStart = dstEnd - srcVec.length();

    for (T* dst = dstStart; dst != dstEnd; dst++, src++) {
        new(dst) T(*src);
        op(dst - dstBegin, dst);
    }

    return true;
}

bool
ModuleGenerator::linkCompiledCode(const CompiledCode& code)
{
    // All code offsets in 'code' must be incremented by their position in the
    // overall module when the code was appended.

    masm_.haltingAlign(CodeAlignment);
    const size_t offsetInModule = masm_.size();
    if (!masm_.appendRawCode(code.bytes.begin(), code.bytes.length()))
        return false;

    auto codeRangeOp = [=](uint32_t codeRangeIndex, CodeRange* codeRange) {
        codeRange->offsetBy(offsetInModule);
        noteCodeRange(codeRangeIndex, *codeRange);
    };
    if (!AppendForEach(&metadataTier_->codeRanges, code.codeRanges, codeRangeOp))
        return false;

    auto callSiteOp = [=](uint32_t, CallSite* cs) { cs->offsetBy(offsetInModule); };
    if (!AppendForEach(&metadataTier_->callSites, code.callSites, callSiteOp))
        return false;

    if (!callSiteTargets_.appendAll(code.callSiteTargets))
        return false;

    MOZ_ASSERT(code.trapSites.empty());

    auto trapFarJumpOp = [=](uint32_t, TrapFarJump* tfj) { tfj->offsetBy(offsetInModule); };
    if (!AppendForEach(&trapFarJumps_, code.trapFarJumps, trapFarJumpOp))
        return false;

    auto callFarJumpOp = [=](uint32_t, CallFarJump* cfj) { cfj->offsetBy(offsetInModule); };
    if (!AppendForEach(&callFarJumps_, code.callFarJumps, callFarJumpOp))
        return false;

    auto memoryOp = [=](uint32_t, MemoryAccess* ma) { ma->offsetBy(offsetInModule); };
    if (!AppendForEach(&metadataTier_->memoryAccesses, code.memoryAccesses, memoryOp))
        return false;

    for (const SymbolicAccess& access : code.symbolicAccesses) {
        uint32_t patchAt = offsetInModule + access.patchAt.offset();
        if (!linkDataTier_->symbolicLinks[access.target].append(patchAt))
            return false;
    }

    for (const CodeLabel& codeLabel : code.codeLabels) {
        LinkDataTier::InternalLink link;
        link.patchAtOffset = offsetInModule + codeLabel.patchAt().offset();
        link.targetOffset = offsetInModule + codeLabel.target().offset();
        if (!linkDataTier_->internalLinks.append(link))
            return false;
    }

    return true;
}

bool
ModuleGenerator::startFuncDefs()
{
    MOZ_ASSERT(!startedFuncDefs_);
    MOZ_ASSERT(!finishedFuncDefs_);

    GlobalHelperThreadState& threads = HelperThreadState();
    MOZ_ASSERT(threads.threadCount > 1);

    uint32_t numTasks;
    if (CanUseExtraThreads() && threads.cpuCount > 1) {
        parallel_ = true;
        numTasks = 2 * threads.maxWasmCompilationThreads();
    } else {
        numTasks = 1;
    }

    if (!tasks_.initCapacity(numTasks))
        return false;
    for (size_t i = 0; i < numTasks; i++)
        tasks_.infallibleEmplaceBack(*env_, taskState_, COMPILATION_LIFO_DEFAULT_CHUNK_SIZE);

    if (!freeTasks_.reserve(numTasks))
        return false;
    for (size_t i = 0; i < numTasks; i++)
        freeTasks_.infallibleAppend(&tasks_[i]);

    // Fill in function stubs for each import so that imported functions can be
    // used in all the places that normal function definitions can (table
    // elements, export calls, etc).

    CompiledCode& importCode = tasks_[0].output;
    MOZ_ASSERT(importCode.empty());

    if (!GenerateImportFunctions(*env_, metadataTier_->funcImports, &importCode))
        return false;

    if (!linkCompiledCode(importCode))
        return false;

    importCode.clear();

    startedFuncDefs_ = true;
    MOZ_ASSERT(!finishedFuncDefs_);
    return true;
}

static bool
ExecuteCompileTask(CompileTask* task, UniqueChars* error)
{
    MOZ_ASSERT(task->lifo.isEmpty());
    MOZ_ASSERT(task->output.empty());

    switch (task->env.tier()) {
      case Tier::Ion:
        if (!IonCompileFunctions(task->env, task->lifo, task->inputs, &task->output, error))
            return false;
        break;
      case Tier::Baseline:
        if (!BaselineCompileFunctions(task->env, task->lifo, task->inputs, &task->output, error))
            return false;
        break;
    }

    MOZ_ASSERT(task->lifo.isEmpty());
    MOZ_ASSERT(task->inputs.length() == task->output.codeRanges.length());
    task->inputs.clear();
    return true;
}

void
wasm::ExecuteCompileTaskFromHelperThread(CompileTask* task)
{
    TraceLoggerThread* logger = TraceLoggerForCurrentThread();
    AutoTraceLog logCompile(logger, TraceLogger_WasmCompilation);

    UniqueChars error;
    bool ok = ExecuteCompileTask(task, &error);

    auto taskState = task->state.lock();

    if (!ok || !taskState->finished.append(task)) {
        taskState->numFailed++;
        if (!taskState->errorMessage)
            taskState->errorMessage = Move(error);
    }

    taskState->failedOrFinished.notify_one();
}

bool
ModuleGenerator::finishTask(CompileTask* task)
{
    masm_.haltingAlign(CodeAlignment);

    // Before merging in the new function's code, if calls in a prior code range
    // might go out of range, insert far jumps to extend the range.
    if (!InRange(startOfUnpatchedCallsites_, masm_.size() + task->output.bytes.length())) {
        startOfUnpatchedCallsites_ = masm_.size();
        if (!linkCallSites())
            return false;
    }

    if (!linkCompiledCode(task->output))
        return false;

    task->output.clear();

    MOZ_ASSERT(task->inputs.empty());
    MOZ_ASSERT(task->output.empty());
    MOZ_ASSERT(task->lifo.isEmpty());
    freeTasks_.infallibleAppend(task);
    return true;
}

bool
ModuleGenerator::launchBatchCompile()
{
    MOZ_ASSERT(currentTask_);

    if (cancelled_ && *cancelled_)
        return false;

    if (parallel_) {
        if (!StartOffThreadWasmCompile(currentTask_, mode()))
            return false;
        outstanding_++;
    } else {
        if (!ExecuteCompileTask(currentTask_, error_))
            return false;
        if (!finishTask(currentTask_))
            return false;
    }

    currentTask_ = nullptr;
    batchedBytecode_ = 0;
    return true;
}

bool
ModuleGenerator::finishOutstandingTask()
{
    MOZ_ASSERT(parallel_);

    CompileTask* task = nullptr;
    {
        auto taskState = taskState_.lock();
        while (true) {
            MOZ_ASSERT(outstanding_ > 0);

            if (taskState->numFailed > 0)
                return false;

            if (!taskState->finished.empty()) {
                outstanding_--;
                task = taskState->finished.popCopy();
                break;
            }

            taskState.wait(taskState->failedOrFinished);
        }
    }

    // Call outside of the compilation lock.
    return finishTask(task);
}

bool
ModuleGenerator::compileFuncDef(uint32_t funcIndex, uint32_t lineOrBytecode,
                                const uint8_t* begin, const uint8_t* end,
                                Uint32Vector&& lineNums)
{
    MOZ_ASSERT(startedFuncDefs_);
    MOZ_ASSERT(!finishedFuncDefs_);
    MOZ_ASSERT_IF(mode() == CompileMode::Tier1, funcIndex < env_->numFuncs());

    if (!currentTask_) {
        if (freeTasks_.empty() && !finishOutstandingTask())
            return false;
        currentTask_ = freeTasks_.popCopy();
    }

    uint32_t funcBytecodeLength = end - begin;

    FuncCompileInputVector& inputs = currentTask_->inputs;
    if (!inputs.emplaceBack(funcIndex, lineOrBytecode, begin, end, Move(lineNums)))
        return false;

    uint32_t threshold;
    switch (tier()) {
      case Tier::Baseline: threshold = JitOptions.wasmBatchBaselineThreshold; break;
      case Tier::Ion:      threshold = JitOptions.wasmBatchIonThreshold;      break;
      default:             MOZ_CRASH("Invalid tier value");                   break;
    }

    batchedBytecode_ += funcBytecodeLength;
    MOZ_ASSERT(batchedBytecode_ <= MaxModuleBytes);
    return batchedBytecode_ <= threshold || launchBatchCompile();
}

bool
ModuleGenerator::finishFuncDefs()
{
    MOZ_ASSERT(startedFuncDefs_);
    MOZ_ASSERT(!finishedFuncDefs_);

    if (currentTask_ && !launchBatchCompile())
        return false;

    while (outstanding_ > 0) {
        if (!finishOutstandingTask())
            return false;
    }

    finishedFuncDefs_ = true;
    return true;
}

bool
ModuleGenerator::finishLinking()
{
    // All functions and traps CodeRanges should have been processed.

#ifdef DEBUG
    for (uint32_t codeRangeIndex : funcToCodeRange_)
        MOZ_ASSERT(codeRangeIndex != BAD_CODE_RANGE);
#endif

    // Now that all functions and stubs are generated and their CodeRanges
    // known, patch all calls (which can emit far jumps) and far jumps.

    if (!linkCallSites())
        return false;

    for (CallFarJump far : callFarJumps_)
        masm_.patchFarJump(far.jump, funcCodeRange(far.funcIndex).funcNormalEntry());

    for (TrapFarJump far : trapFarJumps_)
        masm_.patchFarJump(far.jump, trapCodeOffsets_[far.trap]);

    for (CodeOffset farJump : debugTrapFarJumps_)
        masm_.patchFarJump(farJump, debugTrapCodeOffset_);

    // None of the linking or far-jump operations should emit masm metadata.

    MOZ_ASSERT(masm_.callSites().empty());
    MOZ_ASSERT(masm_.callSiteTargets().empty());
    MOZ_ASSERT(masm_.trapSites().empty());
    MOZ_ASSERT(masm_.trapFarJumps().empty());
    MOZ_ASSERT(masm_.callFarJumps().empty());
    MOZ_ASSERT(masm_.memoryAccesses().empty());
    MOZ_ASSERT(masm_.symbolicAccesses().empty());
    MOZ_ASSERT(masm_.codeLabels().empty());

    masm_.finish();
    return !masm_.oom();
}

bool
ModuleGenerator::finishMetadata(const ShareableBytes& bytecode)
{
#ifdef DEBUG
    // Assert CodeRanges are sorted.
    uint32_t lastEnd = 0;
    for (const CodeRange& codeRange : metadataTier_->codeRanges) {
        MOZ_ASSERT(codeRange.begin() >= lastEnd);
        lastEnd = codeRange.end();
    }

    // Assert debugTrapFarJumpOffsets are sorted.
    uint32_t lastOffset = 0;
    for (uint32_t debugTrapFarJumpOffset : metadataTier_->debugTrapFarJumpOffsets) {
        MOZ_ASSERT(debugTrapFarJumpOffset >= lastOffset);
        lastOffset = debugTrapFarJumpOffset;
    }
#endif

    // Copy over data from the ModuleEnvironment.

    metadata_->memoryUsage = env_->memoryUsage;
    metadata_->minMemoryLength = env_->minMemoryLength;
    metadata_->maxMemoryLength = env_->maxMemoryLength;
    metadata_->startFuncIndex = env_->startFuncIndex;
    metadata_->tables = Move(env_->tables);
    metadata_->globals = Move(env_->globals);
    metadata_->funcNames = Move(env_->funcNames);
    metadata_->customSections = Move(env_->customSections);

    // Inflate the global bytes up to page size so that the total bytes are a
    // page size (as required by the allocator functions).

    metadata_->globalDataLength = AlignBytes(metadata_->globalDataLength, gc::SystemPageSize());

    // These Vectors can get large and the excess capacity can be significant,
    // so realloc them down to size.

    metadataTier_->memoryAccesses.podResizeToFit();
    metadataTier_->codeRanges.podResizeToFit();
    metadataTier_->callSites.podResizeToFit();
    metadataTier_->debugTrapFarJumpOffsets.podResizeToFit();
    metadataTier_->debugFuncToCodeRange.podResizeToFit();

    // Complete function exports and element segments with code range indices,
    // now that every function has a code range.

    for (FuncExport& fe : metadataTier_->funcExports)
        fe.initCodeRangeIndex(funcToCodeRange_[fe.funcIndex()]);

    for (ElemSegment& elems : env_->elemSegments) {
        Uint32Vector& codeRangeIndices = elems.elemCodeRangeIndices(tier());
        MOZ_ASSERT(codeRangeIndices.empty());
        if (!codeRangeIndices.reserve(elems.elemFuncIndices.length()))
            return false;
        for (uint32_t funcIndex : elems.elemFuncIndices)
            codeRangeIndices.infallibleAppend(funcToCodeRange_[funcIndex]);
    }

    // Copy over additional debug information.

    if (env_->debugEnabled()) {
        metadata_->debugEnabled = true;

        const size_t numSigs = env_->funcSigs.length();
        if (!metadata_->debugFuncArgTypes.resize(numSigs))
            return false;
        if (!metadata_->debugFuncReturnTypes.resize(numSigs))
            return false;
        for (size_t i = 0; i < numSigs; i++) {
            if (!metadata_->debugFuncArgTypes[i].appendAll(env_->funcSigs[i]->args()))
                return false;
            metadata_->debugFuncReturnTypes[i] = env_->funcSigs[i]->ret();
        }
        metadataTier_->debugFuncToCodeRange = Move(funcToCodeRange_);

        static_assert(sizeof(ModuleHash) <= sizeof(mozilla::SHA1Sum::Hash),
                      "The ModuleHash size shall not exceed the SHA1 hash size.");
        mozilla::SHA1Sum::Hash hash;
        mozilla::SHA1Sum sha1Sum;
        sha1Sum.update(bytecode.begin(), bytecode.length());
        sha1Sum.finish(hash);
        memcpy(metadata_->debugHash, hash, sizeof(ModuleHash));
    }

    return true;
}

UniqueCodeSegment
ModuleGenerator::finishCodeSegment(const ShareableBytes& bytecode)
{
    MOZ_ASSERT(finishedFuncDefs_);

    // Now that all imports/exports are known, we can generate a special
    // CompiledCode containing stubs.

    CompiledCode& stubCode = tasks_[0].output;
    MOZ_ASSERT(stubCode.empty());

    if (!GenerateStubs(*env_, metadataTier_->funcImports, metadataTier_->funcExports, &stubCode))
        return nullptr;

    if (!linkCompiledCode(stubCode))
        return nullptr;

    // Now that all code is linked in masm_, patch calls and far jumps and
    // finish the metadata. Linking can emit tiny far-jump stubs, so there is an
    // ordering dependency here.

    if (!finishLinking())
        return nullptr;

    if (!finishMetadata(bytecode))
        return nullptr;

    return CodeSegment::create(tier(), masm_, bytecode, *linkDataTier_, *metadata_);
}

UniqueJumpTable
ModuleGenerator::createJumpTable(const CodeSegment& codeSegment)
{
    MOZ_ASSERT(mode() == CompileMode::Tier1);
    MOZ_ASSERT(!isAsmJS());

    uint32_t tableSize = env_->numFuncs();
    UniqueJumpTable jumpTable(js_pod_calloc<void*>(tableSize));
    if (!jumpTable)
        return nullptr;

    uint8_t* codeBase = codeSegment.base();
    for (const CodeRange& codeRange : metadataTier_->codeRanges) {
        if (codeRange.isFunction())
            jumpTable[codeRange.funcIndex()] = codeBase + codeRange.funcTierEntry();
    }

    return jumpTable;
}

SharedModule
ModuleGenerator::finishModule(const ShareableBytes& bytecode)
{
    MOZ_ASSERT(mode() == CompileMode::Once || mode() == CompileMode::Tier1);

    UniqueCodeSegment codeSegment = finishCodeSegment(bytecode);
    if (!codeSegment)
        return nullptr;

    UniqueJumpTable maybeJumpTable;
    if (mode() == CompileMode::Tier1) {
        maybeJumpTable = createJumpTable(*codeSegment);
        if (!maybeJumpTable)
            return nullptr;
    }

    UniqueConstBytes maybeDebuggingBytes;
    if (env_->debugEnabled()) {
        MOZ_ASSERT(mode() == CompileMode::Once);
        Bytes bytes;
        if (!bytes.resize(masm_.bytesNeeded()))
            return nullptr;
        masm_.executableCopy(bytes.begin(), /* flushICache = */ false);
        maybeDebuggingBytes = js::MakeUnique<Bytes>(Move(bytes));
        if (!maybeDebuggingBytes)
            return nullptr;
    }

    SharedCode code = js_new<Code>(Move(codeSegment), *metadata_, Move(maybeJumpTable));
    if (!code)
        return nullptr;

    SharedModule module(js_new<Module>(Move(assumptions_),
                                       *code,
                                       Move(maybeDebuggingBytes),
                                       Move(linkData_),
                                       Move(env_->imports),
                                       Move(env_->exports),
                                       Move(env_->dataSegments),
                                       Move(env_->elemSegments),
                                       bytecode));
    if (!module)
        return nullptr;

    if (mode() == CompileMode::Tier1)
        module->startTier2(*compileArgs_);

    return module;
}

bool
ModuleGenerator::finishTier2(Module& module)
{
    MOZ_ASSERT(mode() == CompileMode::Tier2);
    MOZ_ASSERT(tier() == Tier::Ion);
    MOZ_ASSERT(!env_->debugEnabled());

    if (cancelled_ && *cancelled_)
        return false;

    UniqueCodeSegment codeSegment = finishCodeSegment(module.bytecode());
    if (!codeSegment)
        return false;

    module.finishTier2(linkData_.takeLinkData(tier()),
                       metadata_->takeMetadata(tier()),
                       Move(codeSegment),
                       env_);
    return true;
}
