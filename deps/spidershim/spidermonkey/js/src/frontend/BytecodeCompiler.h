/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_BytecodeCompiler_h
#define frontend_BytecodeCompiler_h

#include "mozilla/Maybe.h"

#include "NamespaceImports.h"

#include "vm/Scope.h"
#include "vm/String.h"

class JSLinearString;

namespace js {

class LazyScript;
class LifoAlloc;
class ModuleObject;
class ScriptSourceObject;
struct SourceCompressionTask;

namespace frontend {

JSScript*
CompileGlobalScript(ExclusiveContext* cx, LifoAlloc& alloc, ScopeKind scopeKind,
                    const ReadOnlyCompileOptions& options,
                    SourceBufferHolder& srcBuf,
                    SourceCompressionTask* extraSct = nullptr,
                    ScriptSourceObject** sourceObjectOut = nullptr);

JSScript*
CompileEvalScript(ExclusiveContext* cx, LifoAlloc& alloc,
                  HandleObject scopeChain, HandleScope enclosingScope,
                  const ReadOnlyCompileOptions& options,
                  SourceBufferHolder& srcBuf,
                  SourceCompressionTask* extraSct = nullptr,
                  ScriptSourceObject** sourceObjectOut = nullptr);

ModuleObject*
CompileModule(JSContext* cx, const ReadOnlyCompileOptions& options,
              SourceBufferHolder& srcBuf);

ModuleObject*
CompileModule(ExclusiveContext* cx, const ReadOnlyCompileOptions& options,
              SourceBufferHolder& srcBuf, LifoAlloc& alloc,
              ScriptSourceObject** sourceObjectOut = nullptr);

MOZ_MUST_USE bool
CompileLazyFunction(JSContext* cx, Handle<LazyScript*> lazy, const char16_t* chars, size_t length);

//
// Compile a single function. The source in srcBuf must match the ECMA-262
// FunctionExpression production.
//
// If nonzero, parameterListEnd is the offset within srcBuf where the parameter
// list is expected to end. During parsing, if we find that it ends anywhere
// else, it's a SyntaxError. This is used to implement the Function constructor;
// it's how we detect that these weird cases are SyntaxErrors:
//
//     Function("/*", "*/x) {")
//     Function("x){ if (3", "return x;}")
//
MOZ_MUST_USE bool
CompileStandaloneFunction(JSContext* cx, MutableHandleFunction fun,
                          const ReadOnlyCompileOptions& options,
                          JS::SourceBufferHolder& srcBuf,
                          mozilla::Maybe<uint32_t> parameterListEnd,
                          HandleScope enclosingScope = nullptr);

MOZ_MUST_USE bool
CompileStandaloneGenerator(JSContext* cx, MutableHandleFunction fun,
                           const ReadOnlyCompileOptions& options,
                           JS::SourceBufferHolder& srcBuf,
                           mozilla::Maybe<uint32_t> parameterListEnd);

MOZ_MUST_USE bool
CompileStandaloneAsyncFunction(JSContext* cx, MutableHandleFunction fun,
                               const ReadOnlyCompileOptions& options,
                               JS::SourceBufferHolder& srcBuf,
                               mozilla::Maybe<uint32_t> parameterListEnd);

MOZ_MUST_USE bool
CompileAsyncFunctionBody(JSContext* cx, MutableHandleFunction fun,
                         const ReadOnlyCompileOptions& options,
                         Handle<PropertyNameVector> formals, JS::SourceBufferHolder& srcBuf);

ScriptSourceObject*
CreateScriptSourceObject(ExclusiveContext* cx, const ReadOnlyCompileOptions& options,
                         mozilla::Maybe<uint32_t> parameterListEnd = mozilla::Nothing());

/*
 * True if str consists of an IdentifierStart character, followed by one or
 * more IdentifierPart characters, i.e. it matches the IdentifierName production
 * in the language spec.
 *
 * This returns true even if str is a keyword like "if".
 *
 * Defined in TokenStream.cpp.
 */
bool
IsIdentifier(JSLinearString* str);

/*
 * As above, but taking chars + length.
 */
bool
IsIdentifier(const char16_t* chars, size_t length);

/* True if str is a keyword. Defined in TokenStream.cpp. */
bool
IsKeyword(JSLinearString* str);

/* Trace all GC things reachable from parser. Defined in Parser.cpp. */
void
TraceParser(JSTracer* trc, JS::AutoGCRooter* parser);

} /* namespace frontend */
} /* namespace js */

#endif /* frontend_BytecodeCompiler_h */
