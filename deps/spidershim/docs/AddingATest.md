Adding a test
===
For now, since we can't link Node yet, adding a test and running it in CI requires some manual hackery.  Here are the steps:

1. Add a new .cc file to `deps/spidershim/test`.
2. Add the test to `deps/spidershim/tests.gyp`.
3. Edit `.travis.yml` and add your test binary name to the list of tests to be fun there.
