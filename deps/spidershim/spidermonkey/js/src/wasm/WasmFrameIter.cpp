/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 *
 * Copyright 2014 Mozilla Foundation
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

#include "wasm/WasmFrameIter.h"

#include "wasm/WasmInstance.h"

#include "jit/MacroAssembler-inl.h"

using namespace js;
using namespace js::jit;
using namespace js::wasm;

using mozilla::DebugOnly;
using mozilla::Swap;

/*****************************************************************************/
// WasmFrameIter implementation

WasmFrameIter::WasmFrameIter(JitActivation* activation, wasm::Frame* fp)
  : activation_(activation),
    code_(nullptr),
    callsite_(nullptr),
    codeRange_(nullptr),
    fp_(fp ? fp : activation->wasmExitFP()),
    unwind_(Unwind::False)
{
    MOZ_ASSERT(fp_);

    // Normally, execution exits wasm code via an exit stub which sets exitFP
    // to the exit stub's frame. Thus, in this case, we want to start iteration
    // at the caller of the exit frame, whose Code, CodeRange and CallSite are
    // indicated by the returnAddress of the exit stub's frame.

    if (!activation->isWasmInterrupted()) {
        popFrame();
        MOZ_ASSERT(!done());
        return;
    }

    // When asynchronously interrupted, exitFP is set to the interrupted frame
    // itself and so we do not want to skip it. Instead, we can recover the
    // Code and CodeRange from the JitActivation, which are set when control
    // flow was interrupted. There is no CallSite (b/c the interrupt was
    // async), but this is fine because CallSite is only used for line number
    // for which we can use the beginning of the function from the CodeRange
    // instead.

    code_ = &fp_->tls->instance->code();
    MOZ_ASSERT(code_ == LookupCode(activation->wasmUnwindPC()));

    codeRange_ = code_->lookupRange(activation->wasmUnwindPC());
    MOZ_ASSERT(codeRange_->kind() == CodeRange::Function);

    MOZ_ASSERT(!done());
}

bool
WasmFrameIter::done() const
{
    MOZ_ASSERT(!!fp_ == !!code_);
    MOZ_ASSERT(!!fp_ == !!codeRange_);
    return !fp_;
}

void
WasmFrameIter::operator++()
{
    MOZ_ASSERT(!done());

    // When the iterator is set to unwind, each time the iterator pops a frame,
    // the JitActivation is updated so that the just-popped frame is no longer
    // visible. This is necessary since Debugger::onLeaveFrame is called before
    // popping each frame and, once onLeaveFrame is called for a given frame,
    // that frame must not be visible to subsequent stack iteration (or it
    // could be added as a "new" frame just as it becomes garbage).  When the
    // frame is "interrupted", then exitFP is included in the callstack
    // (otherwise, it is skipped, as explained above). So to unwind the
    // innermost frame, we just clear the interrupt state.

    if (unwind_ == Unwind::True) {
        if (activation_->isWasmInterrupted())
            activation_->finishWasmInterrupt();
        activation_->setWasmExitFP(fp_);
    }

    popFrame();
}

void
WasmFrameIter::popFrame()
{
    Frame* prevFP = fp_;
    fp_ = prevFP->callerFP;
    MOZ_ASSERT(!(uintptr_t(fp_) & JitActivation::ExitFpWasmBit));

    if (!fp_) {
        code_ = nullptr;
        codeRange_ = nullptr;
        callsite_ = nullptr;

        if (unwind_ == Unwind::True) {
            // TODO with bug 1319203, there may be other JIT frames above.
            activation_->setWasmExitFP(nullptr);
            unwoundAddressOfReturnAddress_ = &prevFP->returnAddress;
        }

        MOZ_ASSERT(done());
        return;
    }

    void* returnAddress = prevFP->returnAddress;

    code_ = &fp_->tls->instance->code();
    MOZ_ASSERT(code_ == LookupCode(returnAddress));

    codeRange_ = code_->lookupRange(returnAddress);
    MOZ_ASSERT(codeRange_->kind() == CodeRange::Function);

    callsite_ = code_->lookupCallSite(returnAddress);
    MOZ_ASSERT(callsite_);

    MOZ_ASSERT(!done());
}

const char*
WasmFrameIter::filename() const
{
    MOZ_ASSERT(!done());
    return code_->metadata().filename.get();
}

const char16_t*
WasmFrameIter::displayURL() const
{
    MOZ_ASSERT(!done());
    return code_->metadata().displayURL();
}

bool
WasmFrameIter::mutedErrors() const
{
    MOZ_ASSERT(!done());
    return code_->metadata().mutedErrors();
}

JSAtom*
WasmFrameIter::functionDisplayAtom() const
{
    MOZ_ASSERT(!done());

    JSContext* cx = activation_->cx();
    JSAtom* atom = instance()->getFuncAtom(cx, codeRange_->funcIndex());
    if (!atom) {
        cx->clearPendingException();
        return cx->names().empty;
    }

    return atom;
}

unsigned
WasmFrameIter::lineOrBytecode() const
{
    MOZ_ASSERT(!done());
    MOZ_ASSERT_IF(!callsite_, activation_->isWasmInterrupted());
    return callsite_ ? callsite_->lineOrBytecode() : codeRange_->funcLineOrBytecode();
}

Instance*
WasmFrameIter::instance() const
{
    MOZ_ASSERT(!done());
    return fp_->tls->instance;
}

void**
WasmFrameIter::unwoundAddressOfReturnAddress() const
{
    MOZ_ASSERT(done());
    MOZ_ASSERT(unwind_ == Unwind::True);
    MOZ_ASSERT(unwoundAddressOfReturnAddress_);
    return unwoundAddressOfReturnAddress_;
}

bool
WasmFrameIter::debugEnabled() const
{
    MOZ_ASSERT(!done());

    // Only non-imported functions can have debug frames.
    //
    // Metadata::debugEnabled is only set if debugging is actually enabled (both
    // requested, and available via baseline compilation), and Tier::Debug code
    // will be available.
    return code_->metadata().debugEnabled &&
           codeRange_->funcIndex() >= code_->metadata(Tier::Debug).funcImports.length();
}

DebugFrame*
WasmFrameIter::debugFrame() const
{
    MOZ_ASSERT(!done());
    MOZ_ASSERT(debugEnabled());
    return reinterpret_cast<DebugFrame*>((uint8_t*)fp_ - DebugFrame::offsetOfFrame());
}

const CallSite*
WasmFrameIter::debugTrapCallsite() const
{
    MOZ_ASSERT(!done());
    MOZ_ASSERT(callsite_);
    MOZ_ASSERT(debugEnabled());
    MOZ_ASSERT(callsite_->kind() == CallSite::EnterFrame ||
               callsite_->kind() == CallSite::LeaveFrame ||
               callsite_->kind() == CallSite::Breakpoint);
    return callsite_;
}

/*****************************************************************************/
// Prologue/epilogue code generation

// These constants reflect statically-determined offsets in the
// prologue/epilogue. The offsets are dynamically asserted during code
// generation.
#if defined(JS_CODEGEN_X64)
static const unsigned PushedRetAddr = 0;
static const unsigned PushedTLS = 2;
static const unsigned PushedFP = 3;
static const unsigned SetFP = 6;
static const unsigned PoppedFP = 2;
static const unsigned PoppedTLSReg = 0;
#elif defined(JS_CODEGEN_X86)
static const unsigned PushedRetAddr = 0;
static const unsigned PushedTLS = 1;
static const unsigned PushedFP = 2;
static const unsigned SetFP = 4;
static const unsigned PoppedFP = 1;
static const unsigned PoppedTLSReg = 0;
#elif defined(JS_CODEGEN_ARM)
static const unsigned BeforePushRetAddr = 0;
static const unsigned PushedRetAddr = 4;
static const unsigned PushedTLS = 8;
static const unsigned PushedFP = 12;
static const unsigned SetFP = 16;
static const unsigned PoppedFP = 4;
static const unsigned PoppedTLSReg = 0;
#elif defined(JS_CODEGEN_ARM64)
static const unsigned BeforePushRetAddr = 0;
static const unsigned PushedRetAddr = 0;
static const unsigned PushedTLS = 1;
static const unsigned PushedFP = 1;
static const unsigned SetFP = 0;
static const unsigned PoppedFP = 0;
static const unsigned PoppedTLSReg = 0;
#elif defined(JS_CODEGEN_MIPS32) || defined(JS_CODEGEN_MIPS64)
static const unsigned BeforePushRetAddr = 0;
static const unsigned PushedRetAddr = 8;
static const unsigned PushedTLS = 16;
static const unsigned PushedFP = 24;
static const unsigned SetFP = 28;
static const unsigned PoppedFP = 16;
static const unsigned PoppedTLSReg = 8;
#elif defined(JS_CODEGEN_NONE)
// Synthetic values to satisfy asserts and avoid compiler warnings.
static const unsigned PushedRetAddr = 0;
static const unsigned PushedTLS = 1;
static const unsigned PushedFP = 2;
static const unsigned SetFP = 3;
static const unsigned PoppedFP = 4;
static const unsigned PoppedTLSReg = 5;
#else
# error "Unknown architecture!"
#endif

static void
PushRetAddr(MacroAssembler& masm, unsigned entry)
{
#if defined(JS_CODEGEN_ARM)
    MOZ_ASSERT(masm.currentOffset() - entry == BeforePushRetAddr);
    masm.push(lr);
#elif defined(JS_CODEGEN_MIPS32) || defined(JS_CODEGEN_MIPS64)
    MOZ_ASSERT(masm.currentOffset() - entry == BeforePushRetAddr);
    masm.push(ra);
#else
    // The x86/x64 call instruction pushes the return address.
#endif
}

static void
LoadActivation(MacroAssembler& masm, const Register& dest)
{
    // WasmCall pushes a JitActivation.
    masm.loadPtr(Address(WasmTlsReg, offsetof(wasm::TlsData, addressOfContext)), dest);
    masm.loadPtr(Address(dest, 0), dest);
    masm.loadPtr(Address(dest, JSContext::offsetOfActivation()), dest);
}

void
wasm::SetExitFP(MacroAssembler& masm, ExitReason reason, Register scratch)
{
    MOZ_ASSERT(!reason.isNone());

    LoadActivation(masm, scratch);

    masm.store32(Imm32(reason.encode()),
                 Address(scratch, JitActivation::offsetOfEncodedWasmExitReason()));

    masm.orPtr(Imm32(JitActivation::ExitFpWasmBit), FramePointer);
    masm.storePtr(FramePointer, Address(scratch, JitActivation::offsetOfPackedExitFP()));
    masm.andPtr(Imm32(int32_t(~JitActivation::ExitFpWasmBit)), FramePointer);
}

void
wasm::ClearExitFP(MacroAssembler& masm, Register scratch)
{
    LoadActivation(masm, scratch);
    masm.storePtr(ImmWord(0x0), Address(scratch, JitActivation::offsetOfPackedExitFP()));
    masm.store32(Imm32(0x0), Address(scratch, JitActivation::offsetOfEncodedWasmExitReason()));
}

static void
GenerateCallablePrologue(MacroAssembler& masm, unsigned framePushed, ExitReason reason,
                         uint32_t* entry, uint32_t* tierEntry, CompileMode mode, uint32_t funcIndex)
{
    // ProfilingFrameIterator needs to know the offsets of several key
    // instructions from entry. To save space, we make these offsets static
    // constants and assert that they match the actual codegen below. On ARM,
    // this requires AutoForbidPools to prevent a constant pool from being
    // randomly inserted between two instructions.
    {
#if defined(JS_CODEGEN_ARM)
        AutoForbidPools afp(&masm, /* number of instructions in scope = */ 7);
#endif
        *entry = masm.currentOffset();

        PushRetAddr(masm, *entry);
        MOZ_ASSERT_IF(!masm.oom(), PushedRetAddr == masm.currentOffset() - *entry);
        masm.push(WasmTlsReg);
        MOZ_ASSERT_IF(!masm.oom(), PushedTLS == masm.currentOffset() - *entry);
        masm.push(FramePointer);
        MOZ_ASSERT_IF(!masm.oom(), PushedFP == masm.currentOffset() - *entry);
        masm.moveStackPtrTo(FramePointer);
        MOZ_ASSERT_IF(!masm.oom(), SetFP == masm.currentOffset() - *entry);
    }

    // Tiering works as follows.  The Code owns a jumpTable, which has one
    // pointer-sized element for each function up to the largest funcIndex in
    // the module.  Each table element is an address into the Tier-1 or the
    // Tier-2 function at that index; the elements are updated when Tier-2 code
    // becomes available.  The Tier-1 function will unconditionally jump to this
    // address.  The table elements are written racily but without tearing when
    // Tier-2 compilation is finished.
    //
    // The address in the table is either to the instruction following the jump
    // in Tier-1 code, or into the function prologue after the standard setup in
    // Tier-2 code.  Effectively, Tier-1 code performs standard frame setup on
    // behalf of whatever code it jumps to, and the target code allocates its
    // own frame in whatever way it wants.

    if (reason.isNone()) {
        if (mode == CompileMode::Tier1) {
            Register scratch = ABINonArgReg0;
            masm.loadPtr(Address(WasmTlsReg, offsetof(TlsData, jumpTable)), scratch);
            masm.jump(Address(scratch, funcIndex*sizeof(uintptr_t)));
        }
        if (tierEntry)
            *tierEntry = masm.currentOffset();
    }

    // If this frame will be exiting compiled code to C++, record the fp and
    // reason in the JitActivation so the frame iterators can unwind.
    if (!reason.isNone())
        SetExitFP(masm, reason, ABINonArgReturnVolatileReg);

    if (framePushed)
        masm.subFromStackPtr(Imm32(framePushed));
}

static void
GenerateCallableEpilogue(MacroAssembler& masm, unsigned framePushed, ExitReason reason,
                         uint32_t* ret)
{
    if (framePushed)
        masm.addToStackPtr(Imm32(framePushed));

    if (!reason.isNone())
        ClearExitFP(masm, ABINonArgReturnVolatileReg);

    // Forbid pools for the same reason as described in GenerateCallablePrologue.
#if defined(JS_CODEGEN_ARM)
    AutoForbidPools afp(&masm, /* number of instructions in scope = */ 7);
#endif

    // There is an important ordering constraint here: fp must be repointed to
    // the caller's frame before any field of the frame currently pointed to by
    // fp is popped: asynchronous signal handlers (which use stack space
    // starting at sp) could otherwise clobber these fields while they are still
    // accessible via fp (fp fields are read during frame iteration which is
    // *also* done asynchronously).

    masm.pop(FramePointer);
    DebugOnly<uint32_t> poppedFP = masm.currentOffset();

    masm.pop(WasmTlsReg);
    DebugOnly<uint32_t> poppedTlsReg = masm.currentOffset();

#if defined(JS_CODEGEN_MIPS32) || defined(JS_CODEGEN_MIPS64)
    masm.pop(ra);
    *ret = masm.currentOffset();
    masm.branch(ra);
#else
    *ret = masm.currentOffset();
    masm.ret();
#endif

    MOZ_ASSERT_IF(!masm.oom(), PoppedFP == *ret - poppedFP);
    MOZ_ASSERT_IF(!masm.oom(), PoppedTLSReg == *ret - poppedTlsReg);
}

void
wasm::GenerateFunctionPrologue(MacroAssembler& masm, unsigned framePushed, const SigIdDesc& sigId,
                               FuncOffsets* offsets, CompileMode mode, uint32_t funcIndex)
{
    // Flush pending pools so they do not get dumped between the 'begin' and
    // 'normalEntry' offsets since the difference must be less than UINT8_MAX
    // to be stored in CodeRange::funcBeginToNormalEntry_.
    masm.flushBuffer();
    masm.haltingAlign(CodeAlignment);

    // Generate table entry:
    offsets->begin = masm.currentOffset();
    BytecodeOffset trapOffset(0);  // ignored by masm.wasmEmitTrapOutOfLineCode
    TrapDesc trap(trapOffset, Trap::IndirectCallBadSig, masm.framePushed());
    switch (sigId.kind()) {
      case SigIdDesc::Kind::Global: {
        Register scratch = WasmTableCallScratchReg;
        masm.loadWasmGlobalPtr(sigId.globalDataOffset(), scratch);
        masm.branchPtr(Assembler::Condition::NotEqual, WasmTableCallSigReg, scratch, trap);
        break;
      }
      case SigIdDesc::Kind::Immediate: {
        masm.branch32(Assembler::Condition::NotEqual, WasmTableCallSigReg, Imm32(sigId.immediate()), trap);
        break;
      }
      case SigIdDesc::Kind::None:
        break;
    }

    // The table entry might have generated a small constant pool in case of
    // immediate comparison.
    masm.flushBuffer();

    // Generate normal entry:
    masm.nopAlign(CodeAlignment);
    GenerateCallablePrologue(masm, framePushed, ExitReason::None(), &offsets->normalEntry,
                             &offsets->tierEntry, mode, funcIndex);

    masm.setFramePushed(framePushed);
}

void
wasm::GenerateFunctionEpilogue(MacroAssembler& masm, unsigned framePushed, FuncOffsets* offsets)
{
    MOZ_ASSERT(masm.framePushed() == framePushed);
    GenerateCallableEpilogue(masm, framePushed, ExitReason::None(), &offsets->ret);
    masm.setFramePushed(0);
}

void
wasm::GenerateExitPrologue(MacroAssembler& masm, unsigned framePushed, ExitReason reason,
                           CallableOffsets* offsets)
{
    masm.haltingAlign(CodeAlignment);
    GenerateCallablePrologue(masm, framePushed, reason, &offsets->begin, nullptr,
                             CompileMode::Once, 0);
    masm.setFramePushed(framePushed);
}

void
wasm::GenerateExitEpilogue(MacroAssembler& masm, unsigned framePushed, ExitReason reason,
                           CallableOffsets* offsets)
{
    // Inverse of GenerateExitPrologue:
    MOZ_ASSERT(masm.framePushed() == framePushed);
    GenerateCallableEpilogue(masm, framePushed, reason, &offsets->ret);
    masm.setFramePushed(0);
}

static void
AssertNoWasmExitFPInJitExit(MacroAssembler& masm)
{
    // As a general stack invariant, if Activation::packedExitFP is tagged as
    // wasm, it must point to a valid wasm::Frame. The JIT exit stub calls into
    // JIT code and thus does not really exit, thus, when entering/leaving the
    // JIT exit stub from/to normal wasm code, packedExitFP is not tagged wasm.
#ifdef DEBUG
    Register scratch = ABINonArgReturnReg0;
    LoadActivation(masm, scratch);

    Label ok;
    masm.branchTestPtr(Assembler::Zero,
                       Address(scratch, JitActivation::offsetOfPackedExitFP()),
                       Imm32(uintptr_t(JitActivation::ExitFpWasmBit)),
                       &ok);
    masm.breakpoint();
    masm.bind(&ok);
#endif
}

void
wasm::GenerateJitExitPrologue(MacroAssembler& masm, unsigned framePushed, CallableOffsets* offsets)
{
    masm.haltingAlign(CodeAlignment);
    GenerateCallablePrologue(masm, framePushed, ExitReason::None(),
                             &offsets->begin, nullptr, CompileMode::Once, 0);
    AssertNoWasmExitFPInJitExit(masm);
    masm.setFramePushed(framePushed);
}

void
wasm::GenerateJitExitEpilogue(MacroAssembler& masm, unsigned framePushed, CallableOffsets* offsets)
{
    // Inverse of GenerateJitExitPrologue:
    MOZ_ASSERT(masm.framePushed() == framePushed);
    AssertNoWasmExitFPInJitExit(masm);
    GenerateCallableEpilogue(masm, framePushed, ExitReason::None(), &offsets->ret);
    masm.setFramePushed(0);
}

/*****************************************************************************/
// ProfilingFrameIterator

ProfilingFrameIterator::ProfilingFrameIterator()
  : activation_(nullptr),
    code_(nullptr),
    codeRange_(nullptr),
    callerFP_(nullptr),
    callerPC_(nullptr),
    stackAddress_(nullptr),
    exitReason_(ExitReason::Fixed::None)
{
    MOZ_ASSERT(done());
}

ProfilingFrameIterator::ProfilingFrameIterator(const JitActivation& activation)
  : activation_(&activation),
    code_(nullptr),
    codeRange_(nullptr),
    callerFP_(nullptr),
    callerPC_(nullptr),
    stackAddress_(nullptr),
    exitReason_(activation.wasmExitReason())
{
    initFromExitFP(activation.wasmExitFP());
}

ProfilingFrameIterator::ProfilingFrameIterator(const JitActivation& activation, const Frame* fp)
  : activation_(&activation),
    code_(nullptr),
    codeRange_(nullptr),
    callerFP_(nullptr),
    callerPC_(nullptr),
    stackAddress_(nullptr),
    exitReason_(ExitReason::Fixed::ImportJit)
{
    MOZ_ASSERT(fp);
    initFromExitFP(fp);
}

static inline void
AssertMatchesCallSite(const JitActivation& activation, void* callerPC, Frame* callerFP)
{
#ifdef DEBUG
    const Code* code = LookupCode(callerPC);
    MOZ_ASSERT(code);

    const CodeRange* callerCodeRange = code->lookupRange(callerPC);
    MOZ_ASSERT(callerCodeRange);

    if (callerCodeRange->kind() == CodeRange::InterpEntry) {
        MOZ_ASSERT(callerFP == nullptr);
        return;
    }

    const CallSite* callsite = code->lookupCallSite(callerPC);
    MOZ_ASSERT(callsite);
#endif
}

void
ProfilingFrameIterator::initFromExitFP(const Frame* fp)
{
    MOZ_ASSERT(fp);
    stackAddress_ = (void*)fp;

    void* pc = fp->returnAddress;

    code_ = LookupCode(pc);
    MOZ_ASSERT(code_);

    codeRange_ = code_->lookupRange(pc);
    MOZ_ASSERT(codeRange_);

    // Since we don't have the pc for fp, start unwinding at the caller of fp.
    // This means that the innermost frame is skipped. This is fine because:
    //  - for import exit calls, the innermost frame is a thunk, so the first
    //    frame that shows up is the function calling the import;
    //  - for Math and other builtin calls as well as interrupts, we note the absence
    //    of an exit reason and inject a fake "builtin" frame; and
    //  - for async interrupts, we just accept that we'll lose the innermost frame.
    switch (codeRange_->kind()) {
      case CodeRange::InterpEntry:
        callerPC_ = nullptr;
        callerFP_ = nullptr;
        break;
      case CodeRange::Function:
        fp = fp->callerFP;
        callerPC_ = fp->returnAddress;
        callerFP_ = fp->callerFP;
        AssertMatchesCallSite(*activation_, callerPC_, callerFP_);
        break;
      case CodeRange::ImportJitExit:
      case CodeRange::ImportInterpExit:
      case CodeRange::BuiltinThunk:
      case CodeRange::TrapExit:
      case CodeRange::DebugTrap:
      case CodeRange::OutOfBoundsExit:
      case CodeRange::UnalignedExit:
      case CodeRange::Throw:
      case CodeRange::Interrupt:
      case CodeRange::FarJumpIsland:
        MOZ_CRASH("Unexpected CodeRange kind");
    }

    MOZ_ASSERT(!done());
}

bool
js::wasm::StartUnwinding(const JitActivation& activation, const RegisterState& registers,
                         UnwindState* unwindState, bool* unwoundCaller)
{
    // Shorthands.
    uint8_t* const pc = (uint8_t*) registers.pc;
    void** const sp = (void**) registers.sp;

    // The frame pointer might be in the process of tagging/untagging; make
    // sure it's untagged.
    Frame* const fp = (Frame*) (intptr_t(registers.fp) & ~JitActivation::ExitFpWasmBit);

    // Get the CodeRange describing pc and the base address to which the
    // CodeRange is relative. If the pc is not in a wasm module or a builtin
    // thunk, then execution must be entering from or leaving to the C++ caller
    // that pushed the JitActivation.
    const CodeRange* codeRange;
    uint8_t* codeBase;
    const Code* code = nullptr;

    const CodeSegment* codeSegment = LookupCodeSegment(pc);
    if (codeSegment) {
        code = codeSegment->code();
        codeRange = code->lookupRange(pc);
        codeBase = codeSegment->base();
    } else if (!LookupBuiltinThunk(pc, &codeRange, &codeBase)) {
        return false;
    }

    // When the pc is inside the prologue/epilogue, the innermost call's Frame
    // is not complete and thus fp points to the second-to-innermost call's
    // Frame. Since fp can only tell you about its caller, naively unwinding
    // while pc is in the prologue/epilogue would skip the second-to-innermost
    // call. To avoid this problem, we use the static structure of the code in
    // the prologue and epilogue to do the Right Thing.
    uint32_t offsetInCode = pc - codeBase;
    MOZ_ASSERT(offsetInCode >= codeRange->begin());
    MOZ_ASSERT(offsetInCode < codeRange->end());

    // Compute the offset of the pc from the (normal) entry of the code range.
    // The stack state of the pc for the entire table-entry is equivalent to
    // that of the first pc of the normal-entry. Thus, we can simplify the below
    // case analysis by redirecting all pc-in-table-entry cases to the
    // pc-at-normal-entry case.
    uint32_t offsetFromEntry;
    if (codeRange->isFunction()) {
        if (offsetInCode < codeRange->funcNormalEntry())
            offsetFromEntry = 0;
        else
            offsetFromEntry = offsetInCode - codeRange->funcNormalEntry();
    } else {
        offsetFromEntry = offsetInCode - codeRange->begin();
    }

    // Most cases end up unwinding to the caller state; not unwinding is the
    // exception here.
    *unwoundCaller = true;

    Frame* fixedFP = nullptr;
    void* fixedPC = nullptr;
    switch (codeRange->kind()) {
      case CodeRange::Function:
      case CodeRange::FarJumpIsland:
      case CodeRange::ImportJitExit:
      case CodeRange::ImportInterpExit:
      case CodeRange::BuiltinThunk:
      case CodeRange::TrapExit:
      case CodeRange::DebugTrap:
#if defined(JS_CODEGEN_MIPS32) || defined(JS_CODEGEN_MIPS64)
        if ((offsetFromEntry >= BeforePushRetAddr && offsetFromEntry < PushedFP) || codeRange->isThunk()) {
            // See BUG 1407986.
            // On MIPS push is emulated by two instructions: adjusting the sp
            // and storing the value to sp.
            // Execution might be interrupted in between the two operation so we
            // have to relay on register state instead of state saved on stack
            // until the wasm::Frame is completely built.
            // On entry the return address is in ra (registers.lr) and
            // fp holds the caller's fp.
            fixedPC = (uint8_t*) registers.lr;
            fixedFP = fp;
            AssertMatchesCallSite(activation, fixedPC, fixedFP);
        } else
#elif defined(JS_CODEGEN_ARM) || defined(JS_CODEGEN_ARM64)
        if (offsetFromEntry == BeforePushRetAddr || codeRange->isThunk()) {
            // The return address is still in lr and fp holds the caller's fp.
            fixedPC = (uint8_t*) registers.lr;
            fixedFP = fp;
            AssertMatchesCallSite(activation, fixedPC, fixedFP);
        } else
#endif
        if (offsetFromEntry == PushedRetAddr || codeRange->isThunk()) {
            // The return address has been pushed on the stack but fp still
            // points to the caller's fp.
            fixedPC = sp[0];
            fixedFP = fp;
            AssertMatchesCallSite(activation, fixedPC, fixedFP);
        } else if (offsetFromEntry >= PushedTLS && offsetFromEntry < PushedFP) {
            // The return address and caller's TLS have been pushed on the
            // stack; fp is still the caller's fp.
            fixedPC = sp[1];
            fixedFP = fp;
            AssertMatchesCallSite(activation, fixedPC, fixedFP);
        } else if (offsetFromEntry == PushedFP) {
            // The full Frame has been pushed; fp is still the caller's fp.
            MOZ_ASSERT(fp == reinterpret_cast<Frame*>(sp)->callerFP);
            fixedPC = reinterpret_cast<Frame*>(sp)->returnAddress;
            fixedFP = fp;
            AssertMatchesCallSite(activation, fixedPC, fixedFP);
        } else if (offsetInCode >= codeRange->ret() - PoppedFP &&
                   offsetInCode < codeRange->ret() - PoppedTLSReg)
        {
            // The fixedFP field of the Frame has been popped into fp.
            fixedPC = sp[1];
            fixedFP = fp;
            AssertMatchesCallSite(activation, fixedPC, fixedFP);
#if defined(JS_CODEGEN_MIPS32) || defined(JS_CODEGEN_MIPS64)
        } else if (offsetInCode >= codeRange->ret() - PoppedTLSReg &&
                   offsetInCode < codeRange->ret())
        {
            // The fixedFP field of the Frame has been popped into fp, but the
            // exit reason hasn't been popped yet.
            fixedPC = sp[0];
            fixedFP = fp;
            AssertMatchesCallSite(activation, fixedPC, fixedFP);
        } else if (offsetInCode == codeRange->ret()) {
            // Both the TLS, fixedFP and ra have been popped and fp now
            // points to the caller's frame.
            fixedPC = (uint8_t*) registers.lr;
            fixedFP = fp;
            AssertMatchesCallSite(activation, fixedPC, fixedFP);
#else
        } else if (offsetInCode == codeRange->ret()) {
            // Both the TLS and fixedFP fields have been popped and fp now
            // points to the caller's frame.
            fixedPC = sp[0];
            fixedFP = fp;
            AssertMatchesCallSite(activation, fixedPC, fixedFP);
#endif
        } else {
            if (codeRange->kind() == CodeRange::ImportJitExit) {
                // The jit exit contains a range where the value of FP can't be
                // trusted. Technically, we could recover fp from sp, but since
                // the range is so short, for now just drop the stack.
                if (offsetInCode >= codeRange->jitExitUntrustedFPStart() &&
                    offsetInCode < codeRange->jitExitUntrustedFPEnd())
                {
                    return false;
                }
            }
            // Not in the prologue/epilogue.
            fixedPC = pc;
            fixedFP = fp;
            *unwoundCaller = false;
            AssertMatchesCallSite(activation, fp->returnAddress, fp->callerFP);
            break;
        }
        break;
      case CodeRange::OutOfBoundsExit:
      case CodeRange::UnalignedExit:
        // These code stubs execute after the prologue/epilogue have completed
        // so pc/fp contains the right values here.
        fixedPC = pc;
        fixedFP = fp;
        *unwoundCaller = false;
        AssertMatchesCallSite(activation, fp->returnAddress, fp->callerFP);
        break;
      case CodeRange::InterpEntry:
        // The entry trampoline is the final frame in an wasm JitActivation. The
        // entry trampoline also doesn't GeneratePrologue/Epilogue so we can't
        // use the general unwinding logic above.
        break;
      case CodeRange::Throw:
        // The throw stub executes a small number of instructions before popping
        // the entire activation. To simplify testing, we simply pretend throw
        // stubs have already popped the entire stack.
        return false;
      case CodeRange::Interrupt:
        // When the PC is in the async interrupt stub, the fp may be garbage and
        // so we cannot blindly unwind it. Since the percent of time spent in
        // the interrupt stub is extremely small, just ignore the stack.
        return false;
    }

    unwindState->code = code;
    unwindState->codeRange = codeRange;
    unwindState->fp = fixedFP;
    unwindState->pc = fixedPC;
    return true;
}

ProfilingFrameIterator::ProfilingFrameIterator(const JitActivation& activation,
                                               const RegisterState& state)
  : activation_(&activation),
    code_(nullptr),
    codeRange_(nullptr),
    callerFP_(nullptr),
    callerPC_(nullptr),
    stackAddress_(nullptr),
    exitReason_(ExitReason::Fixed::None)
{
    // Let wasmExitFP take precedence to StartUnwinding when it is set since
    // during the body of an exit stub, the register state may not be valid
    // causing StartUnwinding() to abandon unwinding this activation.
    if (activation.hasWasmExitFP()) {
        exitReason_ = activation.wasmExitReason();
        initFromExitFP(activation.wasmExitFP());
        return;
    }

    bool unwoundCaller;
    UnwindState unwindState;
    if (!StartUnwinding(*activation_, state, &unwindState, &unwoundCaller)) {
        MOZ_ASSERT(done());
        return;
    }

    if (unwoundCaller) {
        callerFP_ = unwindState.fp;
        callerPC_ = unwindState.pc;
    } else {
        callerFP_ = unwindState.fp->callerFP;
        callerPC_ = unwindState.fp->returnAddress;
    }

    code_ = unwindState.code;
    codeRange_ = unwindState.codeRange;
    stackAddress_ = state.sp;
    MOZ_ASSERT(!done());
}

void
ProfilingFrameIterator::operator++()
{
    if (!exitReason_.isNone()) {
        MOZ_ASSERT(codeRange_);
        exitReason_ = ExitReason::None();
        MOZ_ASSERT(!done());
        return;
    }

    if (!callerPC_) {
        MOZ_ASSERT(!callerFP_);
        codeRange_ = nullptr;
        MOZ_ASSERT(done());
        return;
    }

    if (!callerFP_) {
        codeRange_ = code_->lookupRange(callerPC_);
        MOZ_ASSERT(codeRange_->kind() == CodeRange::InterpEntry);
        callerPC_ = nullptr;
        MOZ_ASSERT(!done());
        return;
    }

    code_ = &callerFP_->tls->instance->code();
    MOZ_ASSERT(code_ == LookupCode(callerPC_));

    codeRange_ = code_->lookupRange(callerPC_);
    MOZ_ASSERT(codeRange_);

    switch (codeRange_->kind()) {
      case CodeRange::Function:
      case CodeRange::ImportJitExit:
      case CodeRange::ImportInterpExit:
      case CodeRange::BuiltinThunk:
      case CodeRange::TrapExit:
      case CodeRange::DebugTrap:
      case CodeRange::OutOfBoundsExit:
      case CodeRange::UnalignedExit:
      case CodeRange::FarJumpIsland:
        stackAddress_ = callerFP_;
        callerPC_ = callerFP_->returnAddress;
        AssertMatchesCallSite(*activation_, callerPC_, callerFP_->callerFP);
        callerFP_ = callerFP_->callerFP;
        break;
      case CodeRange::InterpEntry:
        MOZ_CRASH("should have had null caller fp");
      case CodeRange::Interrupt:
      case CodeRange::Throw:
        MOZ_CRASH("code range doesn't have frame");
    }

    MOZ_ASSERT(!done());
}

static const char*
ThunkedNativeToDescription(SymbolicAddress func)
{
    MOZ_ASSERT(NeedsBuiltinThunk(func));
    switch (func) {
      case SymbolicAddress::HandleExecutionInterrupt:
      case SymbolicAddress::HandleDebugTrap:
      case SymbolicAddress::HandleThrow:
      case SymbolicAddress::ReportTrap:
      case SymbolicAddress::ReportOutOfBounds:
      case SymbolicAddress::ReportUnalignedAccess:
      case SymbolicAddress::CallImport_Void:
      case SymbolicAddress::CallImport_I32:
      case SymbolicAddress::CallImport_I64:
      case SymbolicAddress::CallImport_F64:
      case SymbolicAddress::CoerceInPlace_ToInt32:
      case SymbolicAddress::CoerceInPlace_ToNumber:
        MOZ_ASSERT(!NeedsBuiltinThunk(func), "not in sync with NeedsBuiltinThunk");
        break;
      case SymbolicAddress::ToInt32:
        return "call to asm.js native ToInt32 coercion (in wasm)";
      case SymbolicAddress::DivI64:
        return "call to native i64.div_s (in wasm)";
      case SymbolicAddress::UDivI64:
        return "call to native i64.div_u (in wasm)";
      case SymbolicAddress::ModI64:
        return "call to native i64.rem_s (in wasm)";
      case SymbolicAddress::UModI64:
        return "call to native i64.rem_u (in wasm)";
      case SymbolicAddress::TruncateDoubleToUint64:
        return "call to native i64.trunc_u/f64 (in wasm)";
      case SymbolicAddress::TruncateDoubleToInt64:
        return "call to native i64.trunc_s/f64 (in wasm)";
      case SymbolicAddress::Uint64ToDouble:
        return "call to native f64.convert_u/i64 (in wasm)";
      case SymbolicAddress::Uint64ToFloat32:
        return "call to native f32.convert_u/i64 (in wasm)";
      case SymbolicAddress::Int64ToDouble:
        return "call to native f64.convert_s/i64 (in wasm)";
      case SymbolicAddress::Int64ToFloat32:
        return "call to native f32.convert_s/i64 (in wasm)";
#if defined(JS_CODEGEN_ARM)
      case SymbolicAddress::aeabi_idivmod:
        return "call to native i32.div_s (in wasm)";
      case SymbolicAddress::aeabi_uidivmod:
        return "call to native i32.div_u (in wasm)";
      case SymbolicAddress::AtomicCmpXchg:
        return "call to native atomic compare exchange (in wasm)";
      case SymbolicAddress::AtomicXchg:
        return "call to native atomic exchange (in wasm)";
      case SymbolicAddress::AtomicFetchAdd:
        return "call to native atomic fetch add (in wasm)";
      case SymbolicAddress::AtomicFetchSub:
        return "call to native atomic fetch sub (in wasm)";
      case SymbolicAddress::AtomicFetchAnd:
        return "call to native atomic fetch and (in wasm)";
      case SymbolicAddress::AtomicFetchOr:
        return "call to native atomic fetch or (in wasm)";
      case SymbolicAddress::AtomicFetchXor:
        return "call to native atomic fetch xor (in wasm)";
#endif
      case SymbolicAddress::ModD:
        return "call to asm.js native f64 % (mod)";
      case SymbolicAddress::SinD:
        return "call to asm.js native f64 Math.sin";
      case SymbolicAddress::CosD:
        return "call to asm.js native f64 Math.cos";
      case SymbolicAddress::TanD:
        return "call to asm.js native f64 Math.tan";
      case SymbolicAddress::ASinD:
        return "call to asm.js native f64 Math.asin";
      case SymbolicAddress::ACosD:
        return "call to asm.js native f64 Math.acos";
      case SymbolicAddress::ATanD:
        return "call to asm.js native f64 Math.atan";
      case SymbolicAddress::CeilD:
        return "call to native f64.ceil (in wasm)";
      case SymbolicAddress::CeilF:
        return "call to native f32.ceil (in wasm)";
      case SymbolicAddress::FloorD:
        return "call to native f64.floor (in wasm)";
      case SymbolicAddress::FloorF:
        return "call to native f32.floor (in wasm)";
      case SymbolicAddress::TruncD:
        return "call to native f64.trunc (in wasm)";
      case SymbolicAddress::TruncF:
        return "call to native f32.trunc (in wasm)";
      case SymbolicAddress::NearbyIntD:
        return "call to native f64.nearest (in wasm)";
      case SymbolicAddress::NearbyIntF:
        return "call to native f32.nearest (in wasm)";
      case SymbolicAddress::ExpD:
        return "call to asm.js native f64 Math.exp";
      case SymbolicAddress::LogD:
        return "call to asm.js native f64 Math.log";
      case SymbolicAddress::PowD:
        return "call to asm.js native f64 Math.pow";
      case SymbolicAddress::ATan2D:
        return "call to asm.js native f64 Math.atan2";
      case SymbolicAddress::GrowMemory:
        return "call to native grow_memory (in wasm)";
      case SymbolicAddress::CurrentMemory:
        return "call to native current_memory (in wasm)";
      case SymbolicAddress::WaitI32:
        return "call to native i32.wait (in wasm)";
      case SymbolicAddress::WaitI64:
        return "call to native i64.wait (in wasm)";
      case SymbolicAddress::Wake:
        return "call to native wake (in wasm)";
      case SymbolicAddress::Limit:
        break;
    }
    return "?";
}

const char*
ProfilingFrameIterator::label() const
{
    MOZ_ASSERT(!done());

    // Use the same string for both time inside and under so that the two
    // entries will be coalesced by the profiler.
    static const char* importJitDescription = "fast FFI trampoline (in wasm)";
    static const char* importInterpDescription = "slow FFI trampoline (in wasm)";
    static const char* builtinNativeDescription = "fast FFI trampoline to native (in wasm)";
    static const char* trapDescription = "trap handling (in wasm)";
    static const char* debugTrapDescription = "debug trap handling (in wasm)";

    if (!exitReason_.isFixed())
        return ThunkedNativeToDescription(exitReason_.symbolic());

    switch (exitReason_.fixed()) {
      case ExitReason::Fixed::None:
        break;
      case ExitReason::Fixed::ImportJit:
        return importJitDescription;
      case ExitReason::Fixed::ImportInterp:
        return importInterpDescription;
      case ExitReason::Fixed::BuiltinNative:
        return builtinNativeDescription;
      case ExitReason::Fixed::Trap:
        return trapDescription;
      case ExitReason::Fixed::DebugTrap:
        return debugTrapDescription;
    }

    switch (codeRange_->kind()) {
      case CodeRange::Function:          return code_->profilingLabel(codeRange_->funcIndex());
      case CodeRange::InterpEntry:       return "slow entry trampoline (in wasm)";
      case CodeRange::ImportJitExit:     return importJitDescription;
      case CodeRange::BuiltinThunk:      return builtinNativeDescription;
      case CodeRange::ImportInterpExit:  return importInterpDescription;
      case CodeRange::TrapExit:          return trapDescription;
      case CodeRange::DebugTrap:         return debugTrapDescription;
      case CodeRange::OutOfBoundsExit:   return "out-of-bounds stub (in wasm)";
      case CodeRange::UnalignedExit:     return "unaligned trap stub (in wasm)";
      case CodeRange::FarJumpIsland:     return "interstitial (in wasm)";
      case CodeRange::Throw:             MOZ_FALLTHROUGH;
      case CodeRange::Interrupt:         MOZ_CRASH("does not have a frame");
    }

    MOZ_CRASH("bad code range kind");
}

Instance*
wasm::LookupFaultingInstance(const CodeSegment& codeSegment, void* pc, void* fp)
{
    // Assume bug-caused faults can be raised at any PC and apply the logic of
    // ProfilingFrameIterator to reject any pc outside the (post-prologue,
    // pre-epilogue) body of a wasm function. This is exhaustively tested by the
    // simulators which call this function at every load/store before even
    // knowing whether there is a fault.

    const CodeRange* codeRange = codeSegment.code()->lookupRange(pc);
    if (!codeRange || !codeRange->isFunction())
        return nullptr;

    size_t offsetInModule = ((uint8_t*)pc) - codeSegment.base();
    if (offsetInModule < codeRange->funcNormalEntry() + SetFP)
        return nullptr;
    if (offsetInModule >= codeRange->ret() - PoppedFP)
        return nullptr;

    Instance* instance = reinterpret_cast<Frame*>(fp)->tls->instance;
    MOZ_RELEASE_ASSERT(&instance->code() == codeSegment.code());
    return instance;
}

bool
wasm::InCompiledCode(void* pc)
{
    if (LookupCodeSegment(pc))
        return true;

    const CodeRange* codeRange;
    uint8_t* codeBase;
    return LookupBuiltinThunk(pc, &codeRange, &codeBase);
}
