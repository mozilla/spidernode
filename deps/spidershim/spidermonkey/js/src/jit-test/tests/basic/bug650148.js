// |jit-test| need-for-each

summary=/(?!AB+D)AB/.exec("AB") + '';
try {
  var s = "throw 42";
} catch (e) {}
test();
function test() {
  [ {0xBe: /l/|| 'Error' ? s++ : summary } ]
}
function foo(code) {
    return Function(code)();
}
foo("for each (y in this);");
