Adding a test
===
For now, since we can't link Node yet, adding a test and running it in CI requires some manual hackery.  Here are the steps:

1. Add a new .cc file to `deps/spidershim/test`.
2. Add the test to `deps/spidershim/tests.gyp`.
2. Add the test name to `deps/spidershim/scripts/run-tests.sh`.
