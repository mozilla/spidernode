Understanding V8 APIs
===
A good understanding of the semantics of the V8 API is required to work on SpiderShim.  Here is a quick guide on one way of approaching the V8 APIs in order to understand them:

* Learn the basic terminology.  Start reading the [Getting Started Guide](https://developers.google.com/v8/get_started).
* Study the [Embedder's Guide](https://developers.google.com/v8/embed).  This document goes through a lot of the different parts of the API.  Feel free to focus only on what you want to work on, and its what it refers to if you're not familiar with something.
* The source is your guide!  The V8 API is mostly undocumented, especially when it comes to corner cases, what happens in error conditions, etc.  There's the [Unofficial V8 API Reference Guide](http://v8.paulfryzel.com/docs/master/) that is the doxygen docs of v8.h, but it's rare to find useful documentation there.  Prepare to do some grepping and reading the V8 source code.  :-)  Thankfully V8 is well written software and relatively easy to read, although sometimes you need to delve into quite a bit of the engine code to find your answers.  You can always write small test programs to test the behavior of something if you prefer the trial and error based discovery of semantics!

Sometimes even after understanding a V8 API, it's hard to figure out how to emulate it completely on top of the SpiderMonkey APIs.  In those cases, please check how Node uses that API, and if the part of API you're struggling with is not used, please feel free to add a `// TODO` comment and file and issue for it and move on.  Otherwise the right thing to do depends on the expected complexity of emulating the behavior on top of the SpiderMonkey APIs.
