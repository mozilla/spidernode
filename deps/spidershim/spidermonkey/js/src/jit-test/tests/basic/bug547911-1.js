// |jit-test| need-for-each

a = b = c = d = 0;
this.__defineGetter__("e", function () { throw StopIteration; })
try {
    for each(f in this) {}
} catch (exc) {
    assertEq(exc, StopIteration);
}
