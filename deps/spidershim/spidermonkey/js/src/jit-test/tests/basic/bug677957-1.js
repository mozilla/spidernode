// |jit-test| need-for-each

function test() {
    for each (var i in []) {}
}
for each (new test().p in [0]) {}
