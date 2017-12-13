// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm --allow-natives-syntax

load("test/mjsunit/wasm/wasm-constants.js");
load("test/mjsunit/wasm/wasm-module-builder.js");

let kReturnValue = 19;

let buffer = (function CreateBuffer() {
  let builder = new WasmModuleBuilder();
  builder.addMemory(1, 1, true);
  builder.addFunction('main', kSig_i_v)
      .addBody([kExprI32Const, kReturnValue])
      .exportFunc();

  return builder.toBuffer();
})();

%DisallowCodegenFromStrings(true);

async function SyncTestOk() {
  print('sync module compile (ok)...');
  %DisallowCodegenFromStrings(false);
  let module = new WebAssembly.Module(buffer);
  assertInstanceof(module, WebAssembly.Module);
}

async function SyncTestFail() {
  print('sync module compile (fail)...');
  %DisallowCodegenFromStrings(true);
  try {
    let module = new WebAssembly.Module(buffer);
    assertUnreachable();
  } catch (e) {
    print("  " + e);
    assertInstanceof(e, WebAssembly.CompileError);
  }
}

async function AsyncTestOk() {
  print('async module compile (ok)...');
  %DisallowCodegenFromStrings(false);
  let promise = WebAssembly.compile(buffer);
  assertPromiseResult(
    promise, module => assertInstanceof(module, WebAssembly.Module));
}

async function AsyncTestFail() {
  print('async module compile (fail)...');
  %DisallowCodegenFromStrings(true);
  try {
    let m = await WebAssembly.compile(buffer);
    assertUnreachable();
  } catch (e) {
    print("  " + e);
    assertInstanceof(e, WebAssembly.CompileError);
  }
}

async function StreamingTestOk() {
  print('streaming module compile (ok)...');
  // TODO(titzer): compileStreaming must be supplied by embedder.
  // (and it takes a response, not a buffer)
  %DisallowCodegenFromStrings(false);
  if ("Function" != typeof WebAssembly.compileStreaming) {
    print("  no embedder for streaming compilation");
    return;
  }
  let promise = WebAssembly.compileStreaming(buffer);
  assertPromiseResult(
    promise, module => assertInstanceof(module, WebAssembly.Module));
}

async function StreamingTestFail() {
  print('streaming module compile (fail)...');
  %DisallowCodegenFromStrings(true);
  // TODO(titzer): compileStreaming must be supplied by embedder.
  // (and it takes a response, not a buffer)
  if ("Function" != typeof WebAssembly.compileStreaming) {
    print("  no embedder for streaming compilation");
    return;
  }
  try {
    let m = await WebAssembly.compileStreaming(buffer);
    assertUnreachable();
  } catch (e) {
    print("  " + e);
    assertInstanceof(e, WebAssembly.CompileError);
  }
}

async function RunAll() {
  await SyncTestOk();
  await SyncTestFail();
  await AsyncTestOk();
  await AsyncTestFail();
  await StreamingTestOk();
  await StreamingTestFail();
}

assertPromiseResult(RunAll());
