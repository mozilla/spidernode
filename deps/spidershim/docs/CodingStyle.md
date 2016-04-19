Coding Style
===
This is a document explaining the current style and design assumptions.

### v8.h
* Keep it self-contained.  Including SpiderMonkey headers is not OK.  Use the "pimpl idiom" if you need to pull in SpiderMonkey objects as members.
* Our v8.h is based on node-chakracore's v8.h, which is a bit behind V8's.

### Naming things
* Classes and functions are CamelCased.
* Variables are camelCased.
* Put V8 APIs in namespace `v8`.  Other things go in `v8::internal`.  Don't pollute the global namespace.

### Whitespace
* Don't focus on whitespace!  This is the 21st century.  We'll probably reformat the code in `src` using `clang-format`.

### Dependencies
* SpiderShim tries to be a V8 abstraction layer on top of SpiderMonkey.  Do not depend on stuff outside of the `spidershim` directory.
