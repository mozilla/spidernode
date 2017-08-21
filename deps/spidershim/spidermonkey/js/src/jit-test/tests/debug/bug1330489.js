// |jit-test| test-also-no-wasm-baseline; error: TestComplete

load(libdir + "asserts.js");

if (!wasmDebuggingIsSupported())
    throw "TestComplete";

var g = newGlobal();
g.parent = this;
g.eval("Debugger(parent).onExceptionUnwind = function () {};");

let module = new WebAssembly.Module(wasmTextToBinary(`
    (module
        (import $imp "a" "b" (result i32))
        (memory 1 1)
        (table 2 2 anyfunc)
        (elem (i32.const 0) $imp $def)
        (func $def (result i32) (i32.load (i32.const 0)))
        (type $v2i (func (result i32)))
        (func $call (param i32) (result i32) (call_indirect $v2i (get_local 0)))
        (export "call" $call)
    )
`));

let instance = new WebAssembly.Instance(module, {
    a: { b: function () { throw "test"; } }
});

try {
    instance.exports.call(0);
    assertEq(false, true);
} catch (e) {
    assertEq(e, "test");
}

throw "TestComplete";
