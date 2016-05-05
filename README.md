SpiderNode: Node.js on SpiderMonkey
===
This project is a port of Node.js on top of SpiderMonkey, the JavaScript engine in Firefox. We're still in the very early stages of the port, and a lot of work remains to be done before Node works.

[![Build Status](https://travis-ci.org/mozilla/spidernode.svg?branch=master)](https://travis-ci.org/mozilla/spidernode)

### Goals
Right now we're focused on using this project in the [Positron](https://github.com/mozilla/positron) project.  This means that we will need to finish SpiderShim to the extent necessary for Node.js to work.  In the future, we may look into finishing implementing the features of the V8 API that Node.js does not use, in order to provide a V8 API shim layer out of the box in SpiderMonkey.  The SpiderShim code is being developed with that long term goal in mind.

### How it works
To enable building and running Node.js with SpiderMonkey, a V8 API shim (SpiderShim) is created on top of the SpiderMonkey API.  This is based on Microsoft's [node-chakracore](https://github.com/nodejs/node-chakracore), but it doesn't share much code with it besides the build system integration.

### Current status
This is a _work in progress_, and Node cannot be successfully built yet because of the missing V8 APIs causing linker errors when building the Node.js binary.  So far enough of the V8 API has been implemented to enable running a minimal JavaScript program on top of SpiderShim.  More specifically [these tests](https://github.com/mozilla/spidernode/blob/master/deps/spidershim/test) currently pass.  Nothing else will work out of the box yet!

The build system integration can be improved.

We're actively working on this, so if you're interested in the status of this project, please check here again soon.

In order to find what members of a class need to be implemented, run the following command in a built tree (replacing Isolate with the name of the class you are interested in):

```bash
./deps/spidershim/scripts/find_methods_to_implement.sh Isolate
```

To find everything that is yet to be implemented, run:

```bash
./deps/spidershim/scripts/find_all_unimplemented_methods.sh
```

### How to build
Before building please make sure you have the prerequisites for building Node.js as documented [here](https://github.com/nodejs/node/blob/master/BUILDING.md).

Building on any OS other than Linux or OS X has not been tested.

Build Command:
```bash
./configure --engine=spidermonkey
make
```

Note that right now the build will fail as stated above when linking Node.  Building the SpiderShim test requires invoking the build system twice.  See [.travis.yml](https://github.com/mozilla/spidernode/blob/master/.travis.yml) for example.  To run the tests, do:

```bash
./deps/spidershim/scripts/run-tests.sh
```

### Repository structure
The repository is based on [node-chakracore](https://github.com/nodejs/node-chakracore).  The interesting bits can be found in the [deps/spidershim](https://github.com/mozilla/spidernode/tree/master/deps/spidershim) directory.
