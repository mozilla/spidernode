# Node.js 8 ChangeLog

<table>
<tr>
<th>Current</th>
</tr>
<tr>
<td>
<a href="#8.1.2">8.1.2</a><br/>
<a href="#8.1.1">8.1.1</a><br/>
<a href="#8.1.0">8.1.0</a><br/>
<a href="#8.0.0">8.0.0</a><br/>
</td>
</tr>
</table>

* Other Versions
  * [7.x](CHANGELOG_V7.md)
  * [6.x](CHANGELOG_V6.md)
  * [5.x](CHANGELOG_V5.md)
  * [4.x](CHANGELOG_V4.md)
  * [0.12.x](CHANGELOG_V012.md)
  * [0.10.x](CHANGELOG_V010.md)
  * [io.js](CHANGELOG_IOJS.md)
  * [Archive](CHANGELOG_ARCHIVE.md)

<a id="8.1.2"></a>
## 2017-06-15, Version 8.1.2 (Current), @rvagg

### Notable changes

Release to fix broken `process.release` properties
Ref: https://github.com/nodejs/node/issues/13667

<a id="8.1.1"></a>
## 2017-06-13, Version 8.1.1 (Current), @addaleax

### Notable changes

* **Child processes**
  * `stdout` and `stderr` are now available on the error output of a
    failed call to the `util.promisify()`ed version of
    `child_process.exec`.
    [[`d66d4fc94c`](https://github.com/nodejs/node/commit/d66d4fc94c)]
    [#13388](https://github.com/nodejs/node/pull/13388)

* **HTTP**
  * A regression that broke certain scenarios in which HTTP is used together
    with the `cluster` module has been fixed.
    [[`fff8a56d6f`](https://github.com/nodejs/node/commit/fff8a56d6f)]
    [#13578](https://github.com/nodejs/node/pull/13578)

* **HTTPS**
  * The `rejectUnauthorized` option now works properly for unix sockets.
    [[`c4cbd99d37`](https://github.com/nodejs/node/commit/c4cbd99d37)]
    [#13505](https://github.com/nodejs/node/pull/13505)

* **Readline**
  * A change that broke `npm init` and other code which uses `readline`
    multiple times on the same input stream is reverted.
    [[`0df6c0b5f0`](https://github.com/nodejs/node/commit/0df6c0b5f0)]
    [#13560](https://github.com/nodejs/node/pull/13560)

### Commits

* [[`61c73085ba`](https://github.com/nodejs/node/commit/61c73085ba)] - **async_hooks**: minor refactor to callback invocation (Anna Henningsen) [#13419](https://github.com/nodejs/node/pull/13419)
* [[`bf61d97742`](https://github.com/nodejs/node/commit/bf61d97742)] - **async_hooks**: make sure `.{en|dis}able() === this` (Anna Henningsen) [#13418](https://github.com/nodejs/node/pull/13418)
* [[`32c87ac6f3`](https://github.com/nodejs/node/commit/32c87ac6f3)] - **benchmark**: fix some RegExp nits (Vse Mozhet Byt) [#13551](https://github.com/nodejs/node/pull/13551)
* [[`b967b4cbc5`](https://github.com/nodejs/node/commit/b967b4cbc5)] - **build**: merge test suite groups (Refael Ackermann) [#13378](https://github.com/nodejs/node/pull/13378)
* [[`00d2f7c818`](https://github.com/nodejs/node/commit/00d2f7c818)] - **build,windows**: check for VS version and arch (Refael Ackermann) [#13485](https://github.com/nodejs/node/pull/13485)
* [[`d66d4fc94c`](https://github.com/nodejs/node/commit/d66d4fc94c)] - **child_process**: promisify includes stdio in error (Gil Tayar) [#13388](https://github.com/nodejs/node/pull/13388)
* [[`0ca4bd1e18`](https://github.com/nodejs/node/commit/0ca4bd1e18)] - **child_process**: reduce nextTick() usage (Brian White) [#13459](https://github.com/nodejs/node/pull/13459)
* [[`d1fa59fbb7`](https://github.com/nodejs/node/commit/d1fa59fbb7)] - **child_process**: simplify send() result handling (Brian White) [#13459](https://github.com/nodejs/node/pull/13459)
* [[`d51b1c2e6f`](https://github.com/nodejs/node/commit/d51b1c2e6f)] - **cluster, dns, repl, tls, util**: fix RegExp nits (Vse Mozhet Byt) [#13536](https://github.com/nodejs/node/pull/13536)
* [[`68c0518e48`](https://github.com/nodejs/node/commit/68c0518e48)] - **doc**: fix links and typos in fs.md (Vse Mozhet Byt) [#13573](https://github.com/nodejs/node/pull/13573)
* [[`70432f2111`](https://github.com/nodejs/node/commit/70432f2111)] - **doc**: fix incorrect fs.utimes() link (Justin Beckwith) [#13608](https://github.com/nodejs/node/pull/13608)
* [[`26d76307d5`](https://github.com/nodejs/node/commit/26d76307d5)] - **doc**: fs constants for Node \< v6.3.0 in fs.md (Anshul Guleria) [#12690](https://github.com/nodejs/node/pull/12690)
* [[`52f5e3f804`](https://github.com/nodejs/node/commit/52f5e3f804)] - **doc**: use HTTPS URL for suggested upstream remote (Nikolai Vavilov) [#13602](https://github.com/nodejs/node/pull/13602)
* [[`2c1133d5fe`](https://github.com/nodejs/node/commit/2c1133d5fe)] - **doc**: add readline.emitKeypressEvents note (Samuel Reed) [#9447](https://github.com/nodejs/node/pull/9447)
* [[`53ec50d971`](https://github.com/nodejs/node/commit/53ec50d971)] - **doc**: fix napi_create_*_error signatures in n-api (Jamen Marzonie) [#13544](https://github.com/nodejs/node/pull/13544)
* [[`98d7f25181`](https://github.com/nodejs/node/commit/98d7f25181)] - **doc**: fix out of date sections in n-api doc (Michael Dawson) [#13508](https://github.com/nodejs/node/pull/13508)
* [[`85cac4ed53`](https://github.com/nodejs/node/commit/85cac4ed53)] - **doc**: update new CTC members (Refael Ackermann) [#13534](https://github.com/nodejs/node/pull/13534)
* [[`8c5407d321`](https://github.com/nodejs/node/commit/8c5407d321)] - **doc**: corrects reference to tlsClientError (Tarun) [#13533](https://github.com/nodejs/node/pull/13533)
* [[`3d12e1b455`](https://github.com/nodejs/node/commit/3d12e1b455)] - **doc**: emphasize Collaborators in GOVERNANCE.md (Rich Trott) [#13423](https://github.com/nodejs/node/pull/13423)
* [[`a9be8fff58`](https://github.com/nodejs/node/commit/a9be8fff58)] - **doc**: minimal documentation for Emeritus status (Rich Trott) [#13421](https://github.com/nodejs/node/pull/13421)
* [[`2778256680`](https://github.com/nodejs/node/commit/2778256680)] - **doc**: remove note highlighting in GOVERNANCE doc (Rich Trott) [#13420](https://github.com/nodejs/node/pull/13420)
* [[`2cb6f2b281`](https://github.com/nodejs/node/commit/2cb6f2b281)] - **http**: fix timeout reset after keep-alive timeout (Alexey Orlenko) [#13549](https://github.com/nodejs/node/pull/13549)
* [[`fff8a56d6f`](https://github.com/nodejs/node/commit/fff8a56d6f)] - **http**: handle cases where socket.server is null (Luigi Pinca) [#13578](https://github.com/nodejs/node/pull/13578)
* [[`c4cbd99d37`](https://github.com/nodejs/node/commit/c4cbd99d37)] - **https**: support rejectUnauthorized for unix sockets (cjihrig) [#13505](https://github.com/nodejs/node/pull/13505)
* [[`6a696d15ff`](https://github.com/nodejs/node/commit/6a696d15ff)] - **inspector**: fix crash on exception (Nikolai Vavilov) [#13455](https://github.com/nodejs/node/pull/13455)
* [[`50e1f931a9`](https://github.com/nodejs/node/commit/50e1f931a9)] - **profiler**: declare missing `printErr` (Fedor Indutny) [#13590](https://github.com/nodejs/node/pull/13590)
* [[`0df6c0b5f0`](https://github.com/nodejs/node/commit/0df6c0b5f0)] - ***Revert*** "**readline**: clean up event listener in onNewListener" (Anna Henningsen) [#13560](https://github.com/nodejs/node/pull/13560)
* [[`a5f415fe83`](https://github.com/nodejs/node/commit/a5f415fe83)] - **src**: merge `fn_name` in NODE_SET_PROTOTYPE_METHOD (XadillaX) [#13547](https://github.com/nodejs/node/pull/13547)
* [[`4a96ed4896`](https://github.com/nodejs/node/commit/4a96ed4896)] - **src**: check whether inspector is doing io (Sam Roberts) [#13504](https://github.com/nodejs/node/pull/13504)
* [[`f134c9d147`](https://github.com/nodejs/node/commit/f134c9d147)] - **src**: correct indentation for X509ToObject (Daniel Bevenius) [#13543](https://github.com/nodejs/node/pull/13543)
* [[`dd158b096f`](https://github.com/nodejs/node/commit/dd158b096f)] - **src**: make IsConstructCall checks consistent (Daniel Bevenius) [#13473](https://github.com/nodejs/node/pull/13473)
* [[`bf065344cf`](https://github.com/nodejs/node/commit/bf065344cf)] - **stream**: ensure that instanceof fast-path is hit. (Benedikt Meurer) [#13403](https://github.com/nodejs/node/pull/13403)
* [[`e713482147`](https://github.com/nodejs/node/commit/e713482147)] - **test**: fix typo in test-cli-node-options.js (Vse Mozhet Byt) [#13558](https://github.com/nodejs/node/pull/13558)
* [[`4c5457fae5`](https://github.com/nodejs/node/commit/4c5457fae5)] - **test**: fix flaky test-http-client-get-url (Sebastian Plesciuc) [#13516](https://github.com/nodejs/node/pull/13516)
* [[`812e0b0fbf`](https://github.com/nodejs/node/commit/812e0b0fbf)] - **test**: refactor async-hooks test-callback-error (Rich Trott) [#13554](https://github.com/nodejs/node/pull/13554)
* [[`2ea529b797`](https://github.com/nodejs/node/commit/2ea529b797)] - **test**: add regression test for 13557 (Anna Henningsen) [#13560](https://github.com/nodejs/node/pull/13560)
* [[`4d27930faf`](https://github.com/nodejs/node/commit/4d27930faf)] - **test**: fix flaky test-tls-socket-close (Rich Trott) [#13529](https://github.com/nodejs/node/pull/13529)
* [[`3da56ac9fb`](https://github.com/nodejs/node/commit/3da56ac9fb)] - **test**: harden test-dgram-bind-shared-ports (Refael Ackermann) [#13100](https://github.com/nodejs/node/pull/13100)
* [[`f686f73465`](https://github.com/nodejs/node/commit/f686f73465)] - **test**: add coverage for AsyncResource constructor (Gergely Nemeth) [#13327](https://github.com/nodejs/node/pull/13327)
* [[`12036a1d73`](https://github.com/nodejs/node/commit/12036a1d73)] - **test**: exercise once() with varying arguments (cjihrig) [#13524](https://github.com/nodejs/node/pull/13524)
* [[`1f88cbd620`](https://github.com/nodejs/node/commit/1f88cbd620)] - **test**: refactor test-http-server-keep-alive-timeout (realwakka) [#13448](https://github.com/nodejs/node/pull/13448)
* [[`bdbeb33dcb`](https://github.com/nodejs/node/commit/bdbeb33dcb)] - **test**: add hijackStdout and hijackStderr (XadillaX) [#13439](https://github.com/nodejs/node/pull/13439)
* [[`1c7f9171c0`](https://github.com/nodejs/node/commit/1c7f9171c0)] - **test**: add coverage for napi_property_descriptor (Michael Dawson) [#13510](https://github.com/nodejs/node/pull/13510)
* [[`c8db0475e0`](https://github.com/nodejs/node/commit/c8db0475e0)] - **test**: refactor test-fs-read-* (Rich Trott) [#13501](https://github.com/nodejs/node/pull/13501)
* [[`ad07c46b00`](https://github.com/nodejs/node/commit/ad07c46b00)] - **test**: refactor domain tests (Rich Trott) [#13480](https://github.com/nodejs/node/pull/13480)
* [[`fe5ea3feb0`](https://github.com/nodejs/node/commit/fe5ea3feb0)] - **test**: check callback not invoked on lookup error (Rich Trott) [#13456](https://github.com/nodejs/node/pull/13456)
* [[`216cb3f6e9`](https://github.com/nodejs/node/commit/216cb3f6e9)] - **test,benchmark**: stabilize child-process (Refael Ackermann) [#13457](https://github.com/nodejs/node/pull/13457)
* [[`a0f8faa3a4`](https://github.com/nodejs/node/commit/a0f8faa3a4)] - **v8**: fix debug builds on Windows (Bartosz Sosnowski) [#13634](https://github.com/nodejs/node/pull/13634)
* [[`38a1cfb5e6`](https://github.com/nodejs/node/commit/38a1cfb5e6)] - **v8**: add a js class for Serializer/Dserializer (Rajaram Gaunker) [#13541](https://github.com/nodejs/node/pull/13541)

<a id="8.1.0"></a>
## 2017-06-07, Version 8.1.0 (Current), @jasnell

### Notable Changes

* **Async Hooks**
  * When one `Promise` leads to the creation of a new `Promise`, the parent
    `Promise` will be identified as the trigger
    [[`135f4e6643`](https://github.com/nodejs/node/commit/135f4e6643)]
    [#13367](https://github.com/nodejs/node/pull/13367).
* **Dependencies**
  * libuv has been updated to 1.12.0
    [[`968596ec77`](https://github.com/nodejs/node/commit/968596ec77)]
    [#13306](https://github.com/nodejs/node/pull/13306).
  * npm has been updated to 5.0.3
    [[`ffa7debd7a`](https://github.com/nodejs/node/commit/ffa7debd7a)]
    [#13487](https://github.com/nodejs/node/pull/13487).
* **File system**
  * The `fs.exists()` function now works correctly with `util.promisify()`
    [[`6e0eccd7a1`](https://github.com/nodejs/node/commit/6e0eccd7a1)]
    [#13316](https://github.com/nodejs/node/pull/13316).
  * fs.Stats times are now also available as numbers
    [[`c756efb25a`](https://github.com/nodejs/node/commit/c756efb25a)]
    [#13173](https://github.com/nodejs/node/pull/13173).
* **Inspector**
  * It is now possible to bind to a random port using `--inspect=0`
    [[`cc6ec2fb27`](https://github.com/nodejs/node/commit/cc6ec2fb27)]
    [#5025](https://github.com/nodejs/node/pull/5025).
* **Zlib**
  * A regression in the Zlib module that made it impossible to properly
    subclasses `zlib.Deflate` and other Zlib classes has been fixed.
    [[`6aeb555cc4`](https://github.com/nodejs/node/commit/6aeb555cc4)]
    [#13374](https://github.com/nodejs/node/pull/13374).

### Commits

* [[`b8e90ddf53`](https://github.com/nodejs/node/commit/b8e90ddf53)] - **assert**: fix deepEqual similar sets and maps bug (Joseph Gentle) [#13426](https://github.com/nodejs/node/pull/13426)
* [[`47c9de9842`](https://github.com/nodejs/node/commit/47c9de9842)] - **assert**: fix deepEqual RangeError: Maximum call stack size exceeded (rmdm) [#13318](https://github.com/nodejs/node/pull/13318)
* [[`135f4e6643`](https://github.com/nodejs/node/commit/135f4e6643)] - **(SEMVER-MINOR)** **async_hooks**: use parent promise as triggerId (JiaLi.Passion) [#13367](https://github.com/nodejs/node/pull/13367)
* [[`9db02dcc85`](https://github.com/nodejs/node/commit/9db02dcc85)] - **async_hooks,http**: fix socket reuse with Agent (Anna Henningsen) [#13348](https://github.com/nodejs/node/pull/13348)
* [[`6917df2a80`](https://github.com/nodejs/node/commit/6917df2a80)] - **async_wrap**: run destroy in uv_timer_t (Trevor Norris) [#13369](https://github.com/nodejs/node/pull/13369)
* [[`65f22e481b`](https://github.com/nodejs/node/commit/65f22e481b)] - **build**: use existing variable to reduce complexity (Bryce Baril) [#2883](https://github.com/nodejs/node/pull/2883)
* [[`291669e7d8`](https://github.com/nodejs/node/commit/291669e7d8)] - **build**: streamline JS test suites in Makefile (Rich Trott) [#13340](https://github.com/nodejs/node/pull/13340)
* [[`dcadeb4fef`](https://github.com/nodejs/node/commit/dcadeb4fef)] - **build**: fix typo (Nikolai Vavilov) [#13396](https://github.com/nodejs/node/pull/13396)
* [[`50b5f8bac0`](https://github.com/nodejs/node/commit/50b5f8bac0)] - **crypto**: clear err stack after ECDH::BufferToPoint (Ryan Kelly) [#13275](https://github.com/nodejs/node/pull/13275)
* [[`968596ec77`](https://github.com/nodejs/node/commit/968596ec77)] - **deps**: upgrade libuv to 1.12.0 (cjihrig) [#13306](https://github.com/nodejs/node/pull/13306)
* [[`ffa7debd7a`](https://github.com/nodejs/node/commit/ffa7debd7a)] - **deps**: upgrade npm to 5.0.3 (Kat Marchán) [#13487](https://github.com/nodejs/node/pull/13487)
* [[`035a81b2e6`](https://github.com/nodejs/node/commit/035a81b2e6)] - **deps**: update openssl asm and asm_obsolete files (Daniel Bevenius) [#13233](https://github.com/nodejs/node/pull/13233)
* [[`6f57554650`](https://github.com/nodejs/node/commit/6f57554650)] - **deps**: update openssl config files (Daniel Bevenius) [#13233](https://github.com/nodejs/node/pull/13233)
* [[`1b8b82d076`](https://github.com/nodejs/node/commit/1b8b82d076)] - **deps**: add -no_rand_screen to openssl s_client (Shigeki Ohtsu) [nodejs/io.js#1836](https://github.com/nodejs/io.js/pull/1836)
* [[`783294add1`](https://github.com/nodejs/node/commit/783294add1)] - **deps**: fix asm build error of openssl in x86_win32 (Shigeki Ohtsu) [iojs/io.js#1389](https://github.com/iojs/io.js/pull/1389)
* [[`db7419bead`](https://github.com/nodejs/node/commit/db7419bead)] - **deps**: fix openssl assembly error on ia32 win32 (Fedor Indutny) [iojs/io.js#1389](https://github.com/iojs/io.js/pull/1389)
* [[`dd93fa677a`](https://github.com/nodejs/node/commit/dd93fa677a)] - **deps**: copy all openssl header files to include dir (Daniel Bevenius) [#13233](https://github.com/nodejs/node/pull/13233)
* [[`d9191f6e18`](https://github.com/nodejs/node/commit/d9191f6e18)] - **deps**: upgrade openssl sources to 1.0.2l (Daniel Bevenius) [#13233](https://github.com/nodejs/node/pull/13233)
* [[`d985ca7d4a`](https://github.com/nodejs/node/commit/d985ca7d4a)] - **deps**: float patch on npm to fix citgm (Myles Borins) [#13305](https://github.com/nodejs/node/pull/13305)
* [[`92de432780`](https://github.com/nodejs/node/commit/92de432780)] - **dns**: use faster IP address type check on results (Brian White) [#13261](https://github.com/nodejs/node/pull/13261)
* [[`007a033820`](https://github.com/nodejs/node/commit/007a033820)] - **dns**: improve callback performance (Brian White) [#13261](https://github.com/nodejs/node/pull/13261)
* [[`26e5b6411f`](https://github.com/nodejs/node/commit/26e5b6411f)] - **doc**: update linux supported versions (cjihrig) [#13306](https://github.com/nodejs/node/pull/13306)
* [[`a117bcc0a7`](https://github.com/nodejs/node/commit/a117bcc0a7)] - **doc**: Add URL argument with http/https request (Vladimir Trifonov) [#13405](https://github.com/nodejs/node/pull/13405)
* [[`e6e42c1f75`](https://github.com/nodejs/node/commit/e6e42c1f75)] - **doc**: fix typo "ndapi" in n-api.md (Jamen Marz) [#13484](https://github.com/nodejs/node/pull/13484)
* [[`e991cd79f3`](https://github.com/nodejs/node/commit/e991cd79f3)] - **doc**: add ref to option to enable n-api (Michael Dawson) [#13406](https://github.com/nodejs/node/pull/13406)
* [[`414da1b7a1`](https://github.com/nodejs/node/commit/414da1b7a1)] - **doc**: fix nits in code examples of async_hooks.md (Vse Mozhet Byt) [#13400](https://github.com/nodejs/node/pull/13400)
* [[`159294d7d5`](https://github.com/nodejs/node/commit/159294d7d5)] - **doc**: use prefer-rest-params eslint rule in docs (Vse Mozhet Byt) [#13389](https://github.com/nodejs/node/pull/13389)
* [[`641979b213`](https://github.com/nodejs/node/commit/641979b213)] - **doc**: resume a stream after pipe() and unpipe() (Matteo Collina) [#13329](https://github.com/nodejs/node/pull/13329)
* [[`6c56bbdf13`](https://github.com/nodejs/node/commit/6c56bbdf13)] - **doc**: add missing backticks to doc/api/tls.md (Paul Bininda) [#13394](https://github.com/nodejs/node/pull/13394)
* [[`837ecc01eb`](https://github.com/nodejs/node/commit/837ecc01eb)] - **doc**: update who to cc for async_hooks (Anna Henningsen) [#13332](https://github.com/nodejs/node/pull/13332)
* [[`52c0c47856`](https://github.com/nodejs/node/commit/52c0c47856)] - **doc**: suggest xcode-select --install (Gibson Fahnestock) [#13264](https://github.com/nodejs/node/pull/13264)
* [[`11e428dd99`](https://github.com/nodejs/node/commit/11e428dd99)] - **doc**: add require modules in url.md (Daijiro Wachi) [#13365](https://github.com/nodejs/node/pull/13365)
* [[`2d25e09b0f`](https://github.com/nodejs/node/commit/2d25e09b0f)] - **doc**: add object-curly-spacing to doc/.eslintrc (Vse Mozhet Byt) [#13354](https://github.com/nodejs/node/pull/13354)
* [[`6cd5312b22`](https://github.com/nodejs/node/commit/6cd5312b22)] - **doc**: unify spaces in object literals (Vse Mozhet Byt) [#13354](https://github.com/nodejs/node/pull/13354)
* [[`4e687605ee`](https://github.com/nodejs/node/commit/4e687605ee)] - **doc**: use destructuring in code examples (Vse Mozhet Byt) [#13349](https://github.com/nodejs/node/pull/13349)
* [[`1b192f936a`](https://github.com/nodejs/node/commit/1b192f936a)] - **doc**: fix code examples in zlib.md (Vse Mozhet Byt) [#13342](https://github.com/nodejs/node/pull/13342)
* [[`a872399ddb`](https://github.com/nodejs/node/commit/a872399ddb)] - **doc**: update who to cc for n-api (Michael Dawson) [#13335](https://github.com/nodejs/node/pull/13335)
* [[`90417e8ced`](https://github.com/nodejs/node/commit/90417e8ced)] - **doc**: add missing make command to UPGRADING.md (Daniel Bevenius) [#13233](https://github.com/nodejs/node/pull/13233)
* [[`3c55d1aea4`](https://github.com/nodejs/node/commit/3c55d1aea4)] - **doc**: refine spaces in example from vm.md (Vse Mozhet Byt) [#13334](https://github.com/nodejs/node/pull/13334)
* [[`1729574cd7`](https://github.com/nodejs/node/commit/1729574cd7)] - **doc**: fix link in CHANGELOG_V8 (James, please) [#13313](https://github.com/nodejs/node/pull/13313)
* [[`16605cc3e4`](https://github.com/nodejs/node/commit/16605cc3e4)] - **doc**: add async_hooks, n-api to _toc.md and all.md (Vse Mozhet Byt) [#13379](https://github.com/nodejs/node/pull/13379)
* [[`eb6e9a0c9a`](https://github.com/nodejs/node/commit/eb6e9a0c9a)] - **doc**: remove 'you' from writing-tests.md (Michael Dawson) [#13319](https://github.com/nodejs/node/pull/13319)
* [[`e4f37568e2`](https://github.com/nodejs/node/commit/e4f37568e2)] - **doc**: fix date for 8.0.0 changelog (Myles Borins) [#13360](https://github.com/nodejs/node/pull/13360)
* [[`41f0af524d`](https://github.com/nodejs/node/commit/41f0af524d)] - **doc**: async-hooks documentation (Thorsten Lorenz) [#13287](https://github.com/nodejs/node/pull/13287)
* [[`b8b0bfb1a7`](https://github.com/nodejs/node/commit/b8b0bfb1a7)] - **doc**: add tniessen to collaborators (Tobias Nießen) [#13371](https://github.com/nodejs/node/pull/13371)
* [[`561c14ba12`](https://github.com/nodejs/node/commit/561c14ba12)] - **doc**: modernize and fix code examples in util.md (Vse Mozhet Byt) [#13298](https://github.com/nodejs/node/pull/13298)
* [[`c2d7b41ac7`](https://github.com/nodejs/node/commit/c2d7b41ac7)] - **doc**: fix code examples in url.md (Vse Mozhet Byt) [#13288](https://github.com/nodejs/node/pull/13288)
* [[`243643e5e4`](https://github.com/nodejs/node/commit/243643e5e4)] - **doc**: fix typo in n-api.md (JongChan Choi) [#13323](https://github.com/nodejs/node/pull/13323)
* [[`bee1421501`](https://github.com/nodejs/node/commit/bee1421501)] - **doc**: fix doc styles (Daijiro Wachi) [#13270](https://github.com/nodejs/node/pull/13270)
* [[`44c8ea32df`](https://github.com/nodejs/node/commit/44c8ea32df)] - **doc,stream**: clarify 'data', pipe() and 'readable' (Matteo Collina) [#13432](https://github.com/nodejs/node/pull/13432)
* [[`8f2b82a2b4`](https://github.com/nodejs/node/commit/8f2b82a2b4)] - **errors,tty**: migrate to use internal/errors.js (Gautam Mittal) [#13240](https://github.com/nodejs/node/pull/13240)
* [[`a666238ffe`](https://github.com/nodejs/node/commit/a666238ffe)] - **events**: fix potential permanent deopt (Brian White) [#13384](https://github.com/nodejs/node/pull/13384)
* [[`c756efb25a`](https://github.com/nodejs/node/commit/c756efb25a)] - **(SEMVER-MINOR)** **fs**: expose Stats times as Numbers (Refael Ackermann) [#13173](https://github.com/nodejs/node/pull/13173)
* [[`5644dd76a5`](https://github.com/nodejs/node/commit/5644dd76a5)] - **fs**: replace a bind() with a top-level function (Matteo Collina) [#13474](https://github.com/nodejs/node/pull/13474)
* [[`6e0eccd7a1`](https://github.com/nodejs/node/commit/6e0eccd7a1)] - **(SEMVER-MINOR)** **fs**: promisify exists correctly (Dan Fabulich) [#13316](https://github.com/nodejs/node/pull/13316)
* [[`0caa09da60`](https://github.com/nodejs/node/commit/0caa09da60)] - **gitignore**: add libuv book and GitHub template (cjihrig) [#13306](https://github.com/nodejs/node/pull/13306)
* [[`8efaa554f2`](https://github.com/nodejs/node/commit/8efaa554f2)] - **(SEMVER-MINOR)** **http**: overridable keep-alive behavior of `Agent` (Fedor Indutny) [#13005](https://github.com/nodejs/node/pull/13005)
* [[`afe91ec957`](https://github.com/nodejs/node/commit/afe91ec957)] - **http**: assert parser.consume argument's type (Gireesh Punathil) [#12288](https://github.com/nodejs/node/pull/12288)
* [[`b3c9bff254`](https://github.com/nodejs/node/commit/b3c9bff254)] - **http**: describe parse err in debug output (Sam Roberts) [#13206](https://github.com/nodejs/node/pull/13206)
* [[`c7ebf6ea70`](https://github.com/nodejs/node/commit/c7ebf6ea70)] - **http**: suppress data event if req aborted (Yihong Wang) [#13260](https://github.com/nodejs/node/pull/13260)
* [[`9be8b6373e`](https://github.com/nodejs/node/commit/9be8b6373e)] - **(SEMVER-MINOR)** **inspector**: allow --inspect=host:port from js (Sam Roberts) [#13228](https://github.com/nodejs/node/pull/13228)
* [[`376ac5fc3e`](https://github.com/nodejs/node/commit/376ac5fc3e)] - **inspector**: Allows reentry when paused (Eugene Ostroukhov) [#13350](https://github.com/nodejs/node/pull/13350)
* [[`7f0aa3f4bd`](https://github.com/nodejs/node/commit/7f0aa3f4bd)] - **inspector**: refactor to rename and comment methods (Sam Roberts) [#13321](https://github.com/nodejs/node/pull/13321)
* [[`cc6ec2fb27`](https://github.com/nodejs/node/commit/cc6ec2fb27)] - **(SEMVER-MINOR)** **inspector**: bind to random port with --inspect=0 (Ben Noordhuis) [#5025](https://github.com/nodejs/node/pull/5025)
* [[`4b2c756bfc`](https://github.com/nodejs/node/commit/4b2c756bfc)] - **(SEMVER-MINOR)** **lib**: return this from net.Socket.end() (Sam Roberts) [#13481](https://github.com/nodejs/node/pull/13481)
* [[`b3fb909d06`](https://github.com/nodejs/node/commit/b3fb909d06)] - **lib**: "iff" changed to "if and only if" (Jacob Jones) [#13496](https://github.com/nodejs/node/pull/13496)
* [[`a95f080160`](https://github.com/nodejs/node/commit/a95f080160)] - **n-api**: enable napi_wrap() to work with any object (Jason Ginchereau) [#13250](https://github.com/nodejs/node/pull/13250)
* [[`41eaa4b6a6`](https://github.com/nodejs/node/commit/41eaa4b6a6)] - **net**: fix permanent deopt (Brian White) [#13384](https://github.com/nodejs/node/pull/13384)
* [[`b5409abf9a`](https://github.com/nodejs/node/commit/b5409abf9a)] - **openssl**: fix keypress requirement in apps on win32 (Shigeki Ohtsu) [iojs/io.js#1389](https://github.com/iojs/io.js/pull/1389)
* [[`103de0e69a`](https://github.com/nodejs/node/commit/103de0e69a)] - **process**: fix permanent deopt (Brian White) [#13384](https://github.com/nodejs/node/pull/13384)
* [[`81ddeb98f6`](https://github.com/nodejs/node/commit/81ddeb98f6)] - **readline**: clean up event listener in onNewListener (Gibson Fahnestock) [#13266](https://github.com/nodejs/node/pull/13266)
* [[`791b5b5cbe`](https://github.com/nodejs/node/commit/791b5b5cbe)] - **src**: remove `'` print modifier (Refael Ackermann) [#13447](https://github.com/nodejs/node/pull/13447)
* [[`640101b780`](https://github.com/nodejs/node/commit/640101b780)] - **src**: remove process._inspectorEnbale (cjihrig) [#13460](https://github.com/nodejs/node/pull/13460)
* [[`8620aad573`](https://github.com/nodejs/node/commit/8620aad573)] - **src**: added newline in help message (Josh Ferge) [#13315](https://github.com/nodejs/node/pull/13315)
* [[`71a3d2c87e`](https://github.com/nodejs/node/commit/71a3d2c87e)] - **test**: refactor test-dgram-oob-buffer (Rich Trott) [#13443](https://github.com/nodejs/node/pull/13443)
* [[`54ae7d8931`](https://github.com/nodejs/node/commit/54ae7d8931)] - **test**: pass env vars through to test-benchmark-http (Gibson Fahnestock) [#13390](https://github.com/nodejs/node/pull/13390)
* [[`757ae521b5`](https://github.com/nodejs/node/commit/757ae521b5)] - **test**: validate full error messages (aniketshukla) [#13453](https://github.com/nodejs/node/pull/13453)
* [[`68e06e6945`](https://github.com/nodejs/node/commit/68e06e6945)] - **test**: increase coverage of async_hooks (David Cai) [#13336](https://github.com/nodejs/node/pull/13336)
* [[`7be1a1cd47`](https://github.com/nodejs/node/commit/7be1a1cd47)] - **test**: fix build warning in addons-napi/test_object (Jason Ginchereau) [#13412](https://github.com/nodejs/node/pull/13412)
* [[`fb73070068`](https://github.com/nodejs/node/commit/fb73070068)] - **test**: consolidate n-api test addons - part2 (Michael Dawson) [#13380](https://github.com/nodejs/node/pull/13380)
* [[`339d220eed`](https://github.com/nodejs/node/commit/339d220eed)] - **test**: rearrange inspector headers into convention (Sam Roberts) [#13428](https://github.com/nodejs/node/pull/13428)
* [[`8c7f9da489`](https://github.com/nodejs/node/commit/8c7f9da489)] - **test**: improve async hooks test error messages (Anna Henningsen) [#13243](https://github.com/nodejs/node/pull/13243)
* [[`818c935add`](https://github.com/nodejs/node/commit/818c935add)] - **test**: test async-hook triggerId properties (Dávid Szakállas) [#13328](https://github.com/nodejs/node/pull/13328)
* [[`29f19b6d39`](https://github.com/nodejs/node/commit/29f19b6d39)] - **test**: add documentation for common.mustNotCall() (Rich Trott) [#13359](https://github.com/nodejs/node/pull/13359)
* [[`c208f9d51f`](https://github.com/nodejs/node/commit/c208f9d51f)] - **test**: check destroy hooks are called before exit (Anna Henningsen) [#13369](https://github.com/nodejs/node/pull/13369)
* [[`406c2cd8e4`](https://github.com/nodejs/node/commit/406c2cd8e4)] - **test**: make test-fs-watchfile reliable (Rich Trott) [#13385](https://github.com/nodejs/node/pull/13385)
* [[`93e91a4f3f`](https://github.com/nodejs/node/commit/93e91a4f3f)] - **test**: check inspector support in test/inspector (Daniel Bevenius) [#13324](https://github.com/nodejs/node/pull/13324)
* [[`d1b39d92d6`](https://github.com/nodejs/node/commit/d1b39d92d6)] - **test**: add known_test request with Unicode in the URL (David D Lowe) [#13297](https://github.com/nodejs/node/pull/13297)
* [[`dccd1d2d31`](https://github.com/nodejs/node/commit/dccd1d2d31)] - **test**: improve dns internet test case (Brian White) [#13261](https://github.com/nodejs/node/pull/13261)
* [[`e20f3577d0`](https://github.com/nodejs/node/commit/e20f3577d0)] - **test**: improve test-https-server-keep-alive-timeout (Rich Trott) [#13312](https://github.com/nodejs/node/pull/13312)
* [[`2a29c07d9e`](https://github.com/nodejs/node/commit/2a29c07d9e)] - **test**: mark inspector-port-zero-cluster as flaky (Refael Ackermann)
* [[`b16dd98387`](https://github.com/nodejs/node/commit/b16dd98387)] - **test**: consolidate n-api test addons (Michael Dawson) [#13317](https://github.com/nodejs/node/pull/13317)
* [[`830049f784`](https://github.com/nodejs/node/commit/830049f784)] - **test**: refactor test-net-server-bind (Rich Trott) [#13273](https://github.com/nodejs/node/pull/13273)
* [[`9df8e2a3e9`](https://github.com/nodejs/node/commit/9df8e2a3e9)] - **test**: use mustCall() in test-readline-interface (Rich Trott) [#13259](https://github.com/nodejs/node/pull/13259)
* [[`25a05e5db1`](https://github.com/nodejs/node/commit/25a05e5db1)] - **test**: fix flaky test-fs-watchfile on macOS (Rich Trott) [#13252](https://github.com/nodejs/node/pull/13252)
* [[`ec357bf88f`](https://github.com/nodejs/node/commit/ec357bf88f)] - **test**: use mustNotCall() in test-stream2-objects (Rich Trott) [#13249](https://github.com/nodejs/node/pull/13249)
* [[`5369359d52`](https://github.com/nodejs/node/commit/5369359d52)] - **test**: Make N-API weak-ref GC tests asynchronous (Jason Ginchereau) [#13121](https://github.com/nodejs/node/pull/13121)
* [[`7cc6fd8403`](https://github.com/nodejs/node/commit/7cc6fd8403)] - **test**: improve n-api coverage for typed arrays (Michael Dawson) [#13244](https://github.com/nodejs/node/pull/13244)
* [[`a2d49545a7`](https://github.com/nodejs/node/commit/a2d49545a7)] - **test**: support candidate V8 versions (Michaël Zasso) [#13282](https://github.com/nodejs/node/pull/13282)
* [[`f0ad3bb695`](https://github.com/nodejs/node/commit/f0ad3bb695)] - **test**: hasCrypto https-server-keep-alive-timeout (Daniel Bevenius) [#13253](https://github.com/nodejs/node/pull/13253)
* [[`658560ee5b`](https://github.com/nodejs/node/commit/658560ee5b)] - **test,fs**: test fs.watch for `filename` (Refael Ackermann) [#13411](https://github.com/nodejs/node/pull/13411)
* [[`2e3b758006`](https://github.com/nodejs/node/commit/2e3b758006)] - **test,module**: make message check MUI dependent (Refael Ackermann) [#13393](https://github.com/nodejs/node/pull/13393)
* [[`01278bdd64`](https://github.com/nodejs/node/commit/01278bdd64)] - **tools**: fix order of ESLint rules (Michaël Zasso) [#13363](https://github.com/nodejs/node/pull/13363)
* [[`48cad9cde6`](https://github.com/nodejs/node/commit/48cad9cde6)] - **tools**: fix node args passing in test runner (Brian White) [#13384](https://github.com/nodejs/node/pull/13384)
* [[`bccda4f2b8`](https://github.com/nodejs/node/commit/bccda4f2b8)] - **tools**: be explicit about including key-id (Myles Borins) [#13309](https://github.com/nodejs/node/pull/13309)
* [[`61eb085c6a`](https://github.com/nodejs/node/commit/61eb085c6a)] - **tools, test**: update test-npm-package paths (Gibson Fahnestock) [#13441](https://github.com/nodejs/node/pull/13441)
* [[`ba817d3312`](https://github.com/nodejs/node/commit/ba817d3312)] - **url**: update IDNA handling (Timothy Gu) [#13362](https://github.com/nodejs/node/pull/13362)
* [[`d4d138c6e9`](https://github.com/nodejs/node/commit/d4d138c6e9)] - **url**: do not pass WHATWG host to http.request (Tobias Nießen) [#13409](https://github.com/nodejs/node/pull/13409)
* [[`315c3aaf43`](https://github.com/nodejs/node/commit/315c3aaf43)] - **url**: more precise URLSearchParams constructor (Timothy Gu) [#13026](https://github.com/nodejs/node/pull/13026)
* [[`1bcda5efda`](https://github.com/nodejs/node/commit/1bcda5efda)] - **util**: refactor format method.Performance improved. (Jesus Seijas) [#12407](https://github.com/nodejs/node/pull/12407)
* [[`f47ce01dfb`](https://github.com/nodejs/node/commit/f47ce01dfb)] - **win, doc**: document per-drive current working dir (Bartosz Sosnowski) [#13330](https://github.com/nodejs/node/pull/13330)
* [[`6aeb555cc4`](https://github.com/nodejs/node/commit/6aeb555cc4)] - **zlib**: revert back to Functions (James M Snell) [#13374](https://github.com/nodejs/node/pull/13374)
* [[`cc3174a937`](https://github.com/nodejs/node/commit/cc3174a937)] - **(SEMVER-MINOR)** **zlib**: expose amount of data read for engines (Alexander O'Mara) [#13088](https://github.com/nodejs/node/pull/13088)
* [[`bb77d6c1cc`](https://github.com/nodejs/node/commit/bb77d6c1cc)] - **(SEMVER-MINOR)** **zlib**: option for engine in convenience methods (Alexander O'Mara) [#13089](https://github.com/nodejs/node/pull/13089)


<a id="8.0.0"></a>
## 2017-05-30, Version 8.0.0 (Current), @jasnell

Node.js 8.0.0 is a major new release that includes a significant number of
`semver-major` and `semver-minor` changes. Notable changes are listed below.

The Node.js 8.x release branch is scheduled to become the *next* actively
maintained Long Term Support (LTS) release line in October, 2017 under the
LTS codename `'Carbon'`. Note that the
[LTS lifespan](https://github.com/nodejs/lts) for 8.x will end on December 31st,
2019.

### Notable Changes

* **Async Hooks**
  * The `async_hooks` module has landed in core
    [[`4a7233c178`](https://github.com/nodejs/node/commit/4a7233c178)]
    [#12892](https://github.com/nodejs/node/pull/12892).

* **Buffer**
  * Using the `--pending-deprecation` flag will cause Node.js to emit a
    deprecation warning when using `new Buffer(num)` or `Buffer(num)`.
    [[`d2d32ea5a2`](https://github.com/nodejs/node/commit/d2d32ea5a2)]
    [#11968](https://github.com/nodejs/node/pull/11968).
  * `new Buffer(num)` and `Buffer(num)` will zero-fill new `Buffer` instances
    [[`7eb1b4658e`](https://github.com/nodejs/node/commit/7eb1b4658e)]
    [#12141](https://github.com/nodejs/node/pull/12141).
  * Many `Buffer` methods now accept `Uint8Array` as input
    [[`beca3244e2`](https://github.com/nodejs/node/commit/beca3244e2)]
    [#10236](https://github.com/nodejs/node/pull/10236).

* **Child Process**
  * Argument and kill signal validations have been improved
    [[`97a77288ce`](https://github.com/nodejs/node/commit/97a77288ce)]
    [#12348](https://github.com/nodejs/node/pull/12348),
    [[`d75fdd96aa`](https://github.com/nodejs/node/commit/d75fdd96aa)]
    [#10423](https://github.com/nodejs/node/pull/10423).
  * Child Process methods accept `Uint8Array` as input
    [[`627ecee9ed`](https://github.com/nodejs/node/commit/627ecee9ed)]
    [#10653](https://github.com/nodejs/node/pull/10653).

* **Console**
  * Error events emitted when using `console` methods are now supressed.
    [[`f18e08d820`](https://github.com/nodejs/node/commit/f18e08d820)]
    [#9744](https://github.com/nodejs/node/pull/9744).

* **Dependencies**
  * The npm client has been updated to 5.0.0
    [[`3c3b36af0f`](https://github.com/nodejs/node/commit/3c3b36af0f)]
    [#12936](https://github.com/nodejs/node/pull/12936).
  * V8 has been updated to 5.8 with forward ABI stability to 6.0
    [[`60d1aac8d2`](https://github.com/nodejs/node/commit/60d1aac8d2)]
    [#12784](https://github.com/nodejs/node/pull/12784).

* **Domains**
  * Native `Promise` instances are now `Domain` aware
    [[`84dabe8373`](https://github.com/nodejs/node/commit/84dabe8373)]
    [#12489](https://github.com/nodejs/node/pull/12489).

* **Errors**
  * We have started assigning static error codes to errors generated by Node.js.
    This has been done through multiple commits and is still a work in
    progress.

* **File System**
  * The utility class `fs.SyncWriteStream` has been deprecated
    [[`7a55e34ef4`](https://github.com/nodejs/node/commit/7a55e34ef4)]
    [#10467](https://github.com/nodejs/node/pull/10467).
  * The deprecated `fs.read()` string interface has been removed
    [[`3c2a9361ff`](https://github.com/nodejs/node/commit/3c2a9361ff)]
    [#9683](https://github.com/nodejs/node/pull/9683).

* **HTTP**
  * Improved support for userland implemented Agents
    [[`90403dd1d0`](https://github.com/nodejs/node/commit/90403dd1d0)]
    [#11567](https://github.com/nodejs/node/pull/11567).
  * Outgoing Cookie headers are concatenated into a single string
    [[`d3480776c7`](https://github.com/nodejs/node/commit/d3480776c7)]
    [#11259](https://github.com/nodejs/node/pull/11259).
  * The `httpResponse.writeHeader()` method has been deprecated
    [[`fb71ba4921`](https://github.com/nodejs/node/commit/fb71ba4921)]
    [#11355](https://github.com/nodejs/node/pull/11355).
  * New methods for accessing HTTP headers have been added to `OutgoingMessage`
    [[`3e6f1032a4`](https://github.com/nodejs/node/commit/3e6f1032a4)]
    [#10805](https://github.com/nodejs/node/pull/10805).

* **Lib**
  * All deprecation messages have been assigned static identifiers
    [[`5de3cf099c`](https://github.com/nodejs/node/commit/5de3cf099c)]
    [#10116](https://github.com/nodejs/node/pull/10116).
  * The legacy `linkedlist` module has been removed
    [[`84a23391f6`](https://github.com/nodejs/node/commit/84a23391f6)]
    [#12113](https://github.com/nodejs/node/pull/12113).

* **N-API**
  * Experimental support for the new N-API API has been added
    [[`56e881d0b0`](https://github.com/nodejs/node/commit/56e881d0b0)]
    [#11975](https://github.com/nodejs/node/pull/11975).

* **Process**
  * Process warning output can be redirected to a file using the
    `--redirect-warnings` command-line argument
    [[`03e89b3ff2`](https://github.com/nodejs/node/commit/03e89b3ff2)]
    [#10116](https://github.com/nodejs/node/pull/10116).
  * Process warnings may now include additional detail
    [[`dd20e68b0f`](https://github.com/nodejs/node/commit/dd20e68b0f)]
    [#12725](https://github.com/nodejs/node/pull/12725).

* **REPL**
  * REPL magic mode has been deprecated
    [[`3f27f02da0`](https://github.com/nodejs/node/commit/3f27f02da0)]
    [#11599](https://github.com/nodejs/node/pull/11599).

* **Src**
  * `NODE_MODULE_VERSION` has been updated to 57
    [[`ec7cbaf266`](https://github.com/nodejs/node/commit/ec7cbaf266)]
    [#12995](https://github.com/nodejs/node/pull/12995).
  * Add `--pending-deprecation` command-line argument and
    `NODE_PENDING_DEPRECATION` environment variable
    [[`a16b570f8c`](https://github.com/nodejs/node/commit/a16b570f8c)]
    [#11968](https://github.com/nodejs/node/pull/11968).
  * The `--debug` command-line argument has been deprecated. Note that
    using `--debug` will enable the *new* Inspector-based debug protocol
    as the legacy Debugger protocol previously used by Node.js has been
    removed. [[`010f864426`](https://github.com/nodejs/node/commit/010f864426)]
    [#12949](https://github.com/nodejs/node/pull/12949).
  * Throw when the `-c` and `-e` command-line arguments are used at the same
    time [[`a5f91ab230`](https://github.com/nodejs/node/commit/a5f91ab230)]
    [#11689](https://github.com/nodejs/node/pull/11689).
  * Throw when the `--use-bundled-ca` and `--use-openssl-ca` command-line
    arguments are used at the same time.
    [[`8a7db9d4b5`](https://github.com/nodejs/node/commit/8a7db9d4b5)]
    [#12087](https://github.com/nodejs/node/pull/12087).

* **Stream**
  * `Stream` now supports `destroy()` and `_destroy()` APIs
    [[`b6e1d22fa6`](https://github.com/nodejs/node/commit/b6e1d22fa6)]
    [#12925](https://github.com/nodejs/node/pull/12925).
  * `Stream` now supports the `_final()` API
    [[`07c7f198db`](https://github.com/nodejs/node/commit/07c7f198db)]
    [#12828](https://github.com/nodejs/node/pull/12828).

* **TLS**
  * The `rejectUnauthorized` option now defaults to `true`
    [[`348cc80a3c`](https://github.com/nodejs/node/commit/348cc80a3c)]
    [#5923](https://github.com/nodejs/node/pull/5923).
  * The `tls.createSecurePair()` API now emits a runtime deprecation
    [[`a2ae08999b`](https://github.com/nodejs/node/commit/a2ae08999b)]
    [#11349](https://github.com/nodejs/node/pull/11349).
  * A runtime deprecation will now be emitted when `dhparam` is less than
    2048 bits [[`d523eb9c40`](https://github.com/nodejs/node/commit/d523eb9c40)]
    [#11447](https://github.com/nodejs/node/pull/11447).

* **URL**
  * The WHATWG URL implementation is now a fully-supported Node.js API
    [[`d080ead0f9`](https://github.com/nodejs/node/commit/d080ead0f9)]
    [#12710](https://github.com/nodejs/node/pull/12710).

* **Util**
  * `Symbol` keys are now displayed by default when using `util.inspect()`
    [[`5bfd13b81e`](https://github.com/nodejs/node/commit/5bfd13b81e)]
    [#9726](https://github.com/nodejs/node/pull/9726).
  * `toJSON` errors will be thrown when formatting `%j`
    [[`455e6f1dd8`](https://github.com/nodejs/node/commit/455e6f1dd8)]
    [#11708](https://github.com/nodejs/node/pull/11708).
  * Convert `inspect.styles` and `inspect.colors` to prototype-less objects
    [[`aab0d202f8`](https://github.com/nodejs/node/commit/aab0d202f8)]
    [#11624](https://github.com/nodejs/node/pull/11624).
  * The new `util.promisify()` API has been added
    [[`99da8e8e02`](https://github.com/nodejs/node/commit/99da8e8e02)]
    [#12442](https://github.com/nodejs/node/pull/12442).

* **Zlib**
  * Support `Uint8Array` in Zlib convenience methods
    [[`91383e47fd`](https://github.com/nodejs/node/commit/91383e47fd)]
    [#12001](https://github.com/nodejs/node/pull/12001).
  * Zlib errors now use `RangeError` and `TypeError` consistently
    [[`b514bd231e`](https://github.com/nodejs/node/commit/b514bd231e)]
    [#11391](https://github.com/nodejs/node/pull/11391).

### Commits

#### Semver-Major Commits

* [[`e48d58b8b2`](https://github.com/nodejs/node/commit/e48d58b8b2)] - **(SEMVER-MAJOR)** **assert**: fix AssertionError, assign error code (James M Snell) [#12651](https://github.com/nodejs/node/pull/12651)
* [[`758b8b6e5d`](https://github.com/nodejs/node/commit/758b8b6e5d)] - **(SEMVER-MAJOR)** **assert**: improve assert.fail() API (Rich Trott) [#12293](https://github.com/nodejs/node/pull/12293)
* [[`6481c93aef`](https://github.com/nodejs/node/commit/6481c93aef)] - **(SEMVER-MAJOR)** **assert**: add support for Map and Set in deepEqual (Joseph Gentle) [#12142](https://github.com/nodejs/node/pull/12142)
* [[`efec14a7d1`](https://github.com/nodejs/node/commit/efec14a7d1)] - **(SEMVER-MAJOR)** **assert**: enforce type check in deepStrictEqual (Joyee Cheung) [#10282](https://github.com/nodejs/node/pull/10282)
* [[`562cf5a81c`](https://github.com/nodejs/node/commit/562cf5a81c)] - **(SEMVER-MAJOR)** **assert**: fix premature deep strict comparison (Joyee Cheung) [#11128](https://github.com/nodejs/node/pull/11128)
* [[`0af41834f1`](https://github.com/nodejs/node/commit/0af41834f1)] - **(SEMVER-MAJOR)** **assert**: fix misformatted error message (Rich Trott) [#11254](https://github.com/nodejs/node/pull/11254)
* [[`190dc69c89`](https://github.com/nodejs/node/commit/190dc69c89)] - **(SEMVER-MAJOR)** **benchmark**: add parameter for module benchmark (Brian White) [#10789](https://github.com/nodejs/node/pull/10789)
* [[`b888bfe81d`](https://github.com/nodejs/node/commit/b888bfe81d)] - **(SEMVER-MAJOR)** **benchmark**: allow zero when parsing http req/s (Brian White) [#10558](https://github.com/nodejs/node/pull/10558)
* [[`f53a6fb48b`](https://github.com/nodejs/node/commit/f53a6fb48b)] - **(SEMVER-MAJOR)** **benchmark**: add http header setting scenarios (Brian White) [#10558](https://github.com/nodejs/node/pull/10558)
* [[`d2d32ea5a2`](https://github.com/nodejs/node/commit/d2d32ea5a2)] - **(SEMVER-MAJOR)** **buffer**: add pending deprecation warning (James M Snell) [#11968](https://github.com/nodejs/node/pull/11968)
* [[`7eb1b4658e`](https://github.com/nodejs/node/commit/7eb1b4658e)] - **(SEMVER-MAJOR)** **buffer**: zero fill Buffer(num) by default (James M Snell) [#12141](https://github.com/nodejs/node/pull/12141)
* [[`682573c11d`](https://github.com/nodejs/node/commit/682573c11d)] - **(SEMVER-MAJOR)** **buffer**: remove error for malformatted hex string (Rich Trott) [#12012](https://github.com/nodejs/node/pull/12012)
* [[`9a0829d728`](https://github.com/nodejs/node/commit/9a0829d728)] - **(SEMVER-MAJOR)** **buffer**: stricter argument checking in toString (Nikolai Vavilov) [#11120](https://github.com/nodejs/node/pull/11120)
* [[`beca3244e2`](https://github.com/nodejs/node/commit/beca3244e2)] - **(SEMVER-MAJOR)** **buffer**: allow Uint8Array input to methods (Anna Henningsen) [#10236](https://github.com/nodejs/node/pull/10236)
* [[`3d353c749c`](https://github.com/nodejs/node/commit/3d353c749c)] - **(SEMVER-MAJOR)** **buffer**: consistent error for length \> kMaxLength (Joyee Cheung) [#10152](https://github.com/nodejs/node/pull/10152)
* [[`bf5c309b5e`](https://github.com/nodejs/node/commit/bf5c309b5e)] - **(SEMVER-MAJOR)** **build**: fix V8 build on FreeBSD (Michaël Zasso) [#12784](https://github.com/nodejs/node/pull/12784)
* [[`a1028d5e3e`](https://github.com/nodejs/node/commit/a1028d5e3e)] - **(SEMVER-MAJOR)** **build**: remove cares headers from tarball (Gibson Fahnestock) [#10283](https://github.com/nodejs/node/pull/10283)
* [[`d08836003c`](https://github.com/nodejs/node/commit/d08836003c)] - **(SEMVER-MAJOR)** **build**: build an x64 build by default on Windows (Nikolai Vavilov) [#11537](https://github.com/nodejs/node/pull/11537)
* [[`92ed1ab450`](https://github.com/nodejs/node/commit/92ed1ab450)] - **(SEMVER-MAJOR)** **build**: change nosign flag to sign and flips logic (Joe Doyle) [#10156](https://github.com/nodejs/node/pull/10156)
* [[`97a77288ce`](https://github.com/nodejs/node/commit/97a77288ce)] - **(SEMVER-MAJOR)** **child_process**: improve ChildProcess validation (cjihrig) [#12348](https://github.com/nodejs/node/pull/12348)
* [[`a9111f9738`](https://github.com/nodejs/node/commit/a9111f9738)] - **(SEMVER-MAJOR)** **child_process**: minor cleanup of internals (cjihrig) [#12348](https://github.com/nodejs/node/pull/12348)
* [[`d75fdd96aa`](https://github.com/nodejs/node/commit/d75fdd96aa)] - **(SEMVER-MAJOR)** **child_process**: improve killSignal validations (Sakthipriyan Vairamani (thefourtheye)) [#10423](https://github.com/nodejs/node/pull/10423)
* [[`4cafa60c99`](https://github.com/nodejs/node/commit/4cafa60c99)] - **(SEMVER-MAJOR)** **child_process**: align fork/spawn stdio error msg (Sam Roberts) [#11044](https://github.com/nodejs/node/pull/11044)
* [[`3268863ebc`](https://github.com/nodejs/node/commit/3268863ebc)] - **(SEMVER-MAJOR)** **child_process**: add string shortcut for fork stdio (Javis Sullivan) [#10866](https://github.com/nodejs/node/pull/10866)
* [[`8f3ff98f0e`](https://github.com/nodejs/node/commit/8f3ff98f0e)] - **(SEMVER-MAJOR)** **child_process**: allow Infinity as maxBuffer value (cjihrig) [#10769](https://github.com/nodejs/node/pull/10769)
* [[`627ecee9ed`](https://github.com/nodejs/node/commit/627ecee9ed)] - **(SEMVER-MAJOR)** **child_process**: support Uint8Array input to methods (Anna Henningsen) [#10653](https://github.com/nodejs/node/pull/10653)
* [[`fc7b0dda85`](https://github.com/nodejs/node/commit/fc7b0dda85)] - **(SEMVER-MAJOR)** **child_process**: improve input validation (cjihrig) [#8312](https://github.com/nodejs/node/pull/8312)
* [[`49d1c366d8`](https://github.com/nodejs/node/commit/49d1c366d8)] - **(SEMVER-MAJOR)** **child_process**: remove extra newline in errors (cjihrig) [#9343](https://github.com/nodejs/node/pull/9343)
* [[`f18e08d820`](https://github.com/nodejs/node/commit/f18e08d820)] - **(SEMVER-MAJOR)** **console**: do not emit error events (Anna Henningsen) [#9744](https://github.com/nodejs/node/pull/9744)
* [[`a8f460f12d`](https://github.com/nodejs/node/commit/a8f460f12d)] - **(SEMVER-MAJOR)** **crypto**: support all ArrayBufferView types (Timothy Gu) [#12223](https://github.com/nodejs/node/pull/12223)
* [[`0db49fef41`](https://github.com/nodejs/node/commit/0db49fef41)] - **(SEMVER-MAJOR)** **crypto**: support Uint8Array prime in createDH (Anna Henningsen) [#11983](https://github.com/nodejs/node/pull/11983)
* [[`443691a5ae`](https://github.com/nodejs/node/commit/443691a5ae)] - **(SEMVER-MAJOR)** **crypto**: fix default encoding of LazyTransform (Lukas Möller) [#8611](https://github.com/nodejs/node/pull/8611)
* [[`9f74184e98`](https://github.com/nodejs/node/commit/9f74184e98)] - **(SEMVER-MAJOR)** **crypto**: upgrade pbkdf2 without digest to an error (James M Snell) [#11305](https://github.com/nodejs/node/pull/11305)
* [[`e90f38270c`](https://github.com/nodejs/node/commit/e90f38270c)] - **(SEMVER-MAJOR)** **crypto**: throw error in CipherBase::SetAutoPadding (Kirill Fomichev) [#9405](https://github.com/nodejs/node/pull/9405)
* [[`1ef401ce92`](https://github.com/nodejs/node/commit/1ef401ce92)] - **(SEMVER-MAJOR)** **crypto**: use check macros in CipherBase::SetAuthTag (Kirill Fomichev) [#9395](https://github.com/nodejs/node/pull/9395)
* [[`7599b0ef9d`](https://github.com/nodejs/node/commit/7599b0ef9d)] - **(SEMVER-MAJOR)** **debug**: activate inspector with _debugProcess (Eugene Ostroukhov) [#11431](https://github.com/nodejs/node/pull/11431)
* [[`549e81bfa1`](https://github.com/nodejs/node/commit/549e81bfa1)] - **(SEMVER-MAJOR)** **debugger**: remove obsolete _debug_agent.js (Rich Trott) [#12582](https://github.com/nodejs/node/pull/12582)
* [[`3c3b36af0f`](https://github.com/nodejs/node/commit/3c3b36af0f)] - **(SEMVER-MAJOR)** **deps**: upgrade npm beta to 5.0.0-beta.56 (Kat Marchán) [#12936](https://github.com/nodejs/node/pull/12936)
* [[`6690415696`](https://github.com/nodejs/node/commit/6690415696)] - **(SEMVER-MAJOR)** **deps**: cherry-pick a927f81c7 from V8 upstream (Anna Henningsen) [#11752](https://github.com/nodejs/node/pull/11752)
* [[`60d1aac8d2`](https://github.com/nodejs/node/commit/60d1aac8d2)] - **(SEMVER-MAJOR)** **deps**: update V8 to 5.8.283.38 (Michaël Zasso) [#12784](https://github.com/nodejs/node/pull/12784)
* [[`b7608ac707`](https://github.com/nodejs/node/commit/b7608ac707)] - **(SEMVER-MAJOR)** **deps**: cherry-pick node-inspect#43 (Ali Ijaz Sheikh) [#11441](https://github.com/nodejs/node/pull/11441)
* [[`9c9e2d7f4a`](https://github.com/nodejs/node/commit/9c9e2d7f4a)] - **(SEMVER-MAJOR)** **deps**: backport 3297130 from upstream V8 (Michaël Zasso) [#11752](https://github.com/nodejs/node/pull/11752)
* [[`07088e6fc1`](https://github.com/nodejs/node/commit/07088e6fc1)] - **(SEMVER-MAJOR)** **deps**: backport 39642fa from upstream V8 (Michaël Zasso) [#11752](https://github.com/nodejs/node/pull/11752)
* [[`8394b05e20`](https://github.com/nodejs/node/commit/8394b05e20)] - **(SEMVER-MAJOR)** **deps**: cherry-pick c5c570f from upstream V8 (Michaël Zasso) [#11752](https://github.com/nodejs/node/pull/11752)
* [[`fcc58bf0da`](https://github.com/nodejs/node/commit/fcc58bf0da)] - **(SEMVER-MAJOR)** **deps**: cherry-pick a927f81c7 from V8 upstream (Anna Henningsen) [#11752](https://github.com/nodejs/node/pull/11752)
* [[`83bf2975ec`](https://github.com/nodejs/node/commit/83bf2975ec)] - **(SEMVER-MAJOR)** **deps**: cherry-pick V8 ValueSerializer changes (Anna Henningsen) [#11752](https://github.com/nodejs/node/pull/11752)
* [[`c459d8ea5d`](https://github.com/nodejs/node/commit/c459d8ea5d)] - **(SEMVER-MAJOR)** **deps**: update V8 to 5.7.492.69 (Michaël Zasso) [#11752](https://github.com/nodejs/node/pull/11752)
* [[`7c0c7baff3`](https://github.com/nodejs/node/commit/7c0c7baff3)] - **(SEMVER-MAJOR)** **deps**: fix gyp configuration for v8-inspector (Michaël Zasso) [#10992](https://github.com/nodejs/node/pull/10992)
* [[`00a2aa0af5`](https://github.com/nodejs/node/commit/00a2aa0af5)] - **(SEMVER-MAJOR)** **deps**: fix gyp build configuration for Windows (Michaël Zasso) [#10992](https://github.com/nodejs/node/pull/10992)
* [[`b30ec59855`](https://github.com/nodejs/node/commit/b30ec59855)] - **(SEMVER-MAJOR)** **deps**: switch to v8_inspector in V8 (Ali Ijaz Sheikh) [#10992](https://github.com/nodejs/node/pull/10992)
* [[`7a77daf243`](https://github.com/nodejs/node/commit/7a77daf243)] - **(SEMVER-MAJOR)** **deps**: update V8 to 5.6.326.55 (Michaël Zasso) [#10992](https://github.com/nodejs/node/pull/10992)
* [[`c9e5178f3c`](https://github.com/nodejs/node/commit/c9e5178f3c)] - **(SEMVER-MAJOR)** **deps**: hide zlib internal symbols (Sam Roberts) [#11082](https://github.com/nodejs/node/pull/11082)
* [[`2739185b79`](https://github.com/nodejs/node/commit/2739185b79)] - **(SEMVER-MAJOR)** **deps**: update V8 to 5.5.372.40 (Michaël Zasso) [#9618](https://github.com/nodejs/node/pull/9618)
* [[`f2e3a670af`](https://github.com/nodejs/node/commit/f2e3a670af)] - **(SEMVER-MAJOR)** **dgram**: convert to using internal/errors (Michael Dawson) [#12926](https://github.com/nodejs/node/pull/12926)
* [[`2dc1053b0a`](https://github.com/nodejs/node/commit/2dc1053b0a)] - **(SEMVER-MAJOR)** **dgram**: support Uint8Array input to send() (Anna Henningsen) [#11985](https://github.com/nodejs/node/pull/11985)
* [[`32679c73c1`](https://github.com/nodejs/node/commit/32679c73c1)] - **(SEMVER-MAJOR)** **dgram**: improve signature of Socket.prototype.send (Christopher Hiller) [#10473](https://github.com/nodejs/node/pull/10473)
* [[`5587ff1ccd`](https://github.com/nodejs/node/commit/5587ff1ccd)] - **(SEMVER-MAJOR)** **dns**: handle implicit bind DNS errors (cjihrig) [#11036](https://github.com/nodejs/node/pull/11036)
* [[`eb535c5154`](https://github.com/nodejs/node/commit/eb535c5154)] - **(SEMVER-MAJOR)** **doc**: deprecate vm.runInDebugContext (Josh Gavant) [#12243](https://github.com/nodejs/node/pull/12243)
* [[`75c471a026`](https://github.com/nodejs/node/commit/75c471a026)] - **(SEMVER-MAJOR)** **doc**: drop PPC BE from supported platforms (Michael Dawson) [#12309](https://github.com/nodejs/node/pull/12309)
* [[`86996c5838`](https://github.com/nodejs/node/commit/86996c5838)] - **(SEMVER-MAJOR)** **doc**: deprecate private http properties (Brian White) [#10941](https://github.com/nodejs/node/pull/10941)
* [[`3d8379ae60`](https://github.com/nodejs/node/commit/3d8379ae60)] - **(SEMVER-MAJOR)** **doc**: improve assert.md regarding ECMAScript terms (Joyee Cheung) [#11128](https://github.com/nodejs/node/pull/11128)
* [[`d708700c68`](https://github.com/nodejs/node/commit/d708700c68)] - **(SEMVER-MAJOR)** **doc**: deprecate buffer's parent property (Sakthipriyan Vairamani (thefourtheye)) [#8332](https://github.com/nodejs/node/pull/8332)
* [[`03d440e3ce`](https://github.com/nodejs/node/commit/03d440e3ce)] - **(SEMVER-MAJOR)** **doc**: document buffer.buffer property (Sakthipriyan Vairamani (thefourtheye)) [#8332](https://github.com/nodejs/node/pull/8332)
* [[`f0b702555a`](https://github.com/nodejs/node/commit/f0b702555a)] - **(SEMVER-MAJOR)** **errors**: use lazy assert to avoid issues on startup (James M Snell) [#11300](https://github.com/nodejs/node/pull/11300)
* [[`251e5ed8ee`](https://github.com/nodejs/node/commit/251e5ed8ee)] - **(SEMVER-MAJOR)** **errors**: assign error code to bootstrap_node created error (James M Snell) [#11298](https://github.com/nodejs/node/pull/11298)
* [[`e75bc87d22`](https://github.com/nodejs/node/commit/e75bc87d22)] - **(SEMVER-MAJOR)** **errors**: port internal/process errors to internal/errors (James M Snell) [#11294](https://github.com/nodejs/node/pull/11294)
* [[`76327613af`](https://github.com/nodejs/node/commit/76327613af)] - **(SEMVER-MAJOR)** **errors, child_process**: migrate to using internal/errors (James M Snell) [#11300](https://github.com/nodejs/node/pull/11300)
* [[`1c834e78ff`](https://github.com/nodejs/node/commit/1c834e78ff)] - **(SEMVER-MAJOR)** **errors,test**: migrating error to internal/errors (larissayvette) [#11505](https://github.com/nodejs/node/pull/11505)
* [[`2141d37452`](https://github.com/nodejs/node/commit/2141d37452)] - **(SEMVER-MAJOR)** **events**: update and clarify error message (Chris Burkhart) [#10387](https://github.com/nodejs/node/pull/10387)
* [[`221b03ad20`](https://github.com/nodejs/node/commit/221b03ad20)] - **(SEMVER-MAJOR)** **events, doc**: check input in defaultMaxListeners (DavidCai) [#11938](https://github.com/nodejs/node/pull/11938)
* [[`eed87b1637`](https://github.com/nodejs/node/commit/eed87b1637)] - **(SEMVER-MAJOR)** **fs**: (+/-)Infinity and NaN invalid unixtimestamp (Luca Maraschi) [#11919](https://github.com/nodejs/node/pull/11919)
* [[`71097744b2`](https://github.com/nodejs/node/commit/71097744b2)] - **(SEMVER-MAJOR)** **fs**: more realpath*() optimizations (Brian White) [#11665](https://github.com/nodejs/node/pull/11665)
* [[`6a5ab5d550`](https://github.com/nodejs/node/commit/6a5ab5d550)] - **(SEMVER-MAJOR)** **fs**: include more fs.stat*() optimizations (Brian White) [#11665](https://github.com/nodejs/node/pull/11665)
* [[`1c3df96570`](https://github.com/nodejs/node/commit/1c3df96570)] - **(SEMVER-MAJOR)** **fs**: replace regexp with function (Brian White) [#10789](https://github.com/nodejs/node/pull/10789)
* [[`34c9fc2e4e`](https://github.com/nodejs/node/commit/34c9fc2e4e)] - **(SEMVER-MAJOR)** **fs**: avoid multiple conversions to string (Brian White) [#10789](https://github.com/nodejs/node/pull/10789)
* [[`21b2440176`](https://github.com/nodejs/node/commit/21b2440176)] - **(SEMVER-MAJOR)** **fs**: avoid recompilation of closure (Brian White) [#10789](https://github.com/nodejs/node/pull/10789)
* [[`7a55e34ef4`](https://github.com/nodejs/node/commit/7a55e34ef4)] - **(SEMVER-MAJOR)** **fs**: runtime deprecation for fs.SyncWriteStream (James M Snell) [#10467](https://github.com/nodejs/node/pull/10467)
* [[`b1fc7745f2`](https://github.com/nodejs/node/commit/b1fc7745f2)] - **(SEMVER-MAJOR)** **fs**: avoid emitting error EBADF for double close (Matteo Collina) [#11225](https://github.com/nodejs/node/pull/11225)
* [[`3c2a9361ff`](https://github.com/nodejs/node/commit/3c2a9361ff)] - **(SEMVER-MAJOR)** **fs**: remove fs.read's string interface (Nikolai Vavilov) [#9683](https://github.com/nodejs/node/pull/9683)
* [[`f3cf8e9808`](https://github.com/nodejs/node/commit/f3cf8e9808)] - **(SEMVER-MAJOR)** **fs**: do not pass Buffer when toString() fails (Brian White) [#9670](https://github.com/nodejs/node/pull/9670)
* [[`85a4e25775`](https://github.com/nodejs/node/commit/85a4e25775)] - **(SEMVER-MAJOR)** **http**: add type checking for hostname (James M Snell) [#12494](https://github.com/nodejs/node/pull/12494)
* [[`90403dd1d0`](https://github.com/nodejs/node/commit/90403dd1d0)] - **(SEMVER-MAJOR)** **http**: should support userland Agent (fengmk2) [#11567](https://github.com/nodejs/node/pull/11567)
* [[`d3480776c7`](https://github.com/nodejs/node/commit/d3480776c7)] - **(SEMVER-MAJOR)** **http**: concatenate outgoing Cookie headers (Brian White) [#11259](https://github.com/nodejs/node/pull/11259)
* [[`6b2cef65c9`](https://github.com/nodejs/node/commit/6b2cef65c9)] - **(SEMVER-MAJOR)** **http**: append Cookie header values with semicolon (Brian White) [#11259](https://github.com/nodejs/node/pull/11259)
* [[`8243ca0e0e`](https://github.com/nodejs/node/commit/8243ca0e0e)] - **(SEMVER-MAJOR)** **http**: reuse existing StorageObject (Brian White) [#10941](https://github.com/nodejs/node/pull/10941)
* [[`b377034359`](https://github.com/nodejs/node/commit/b377034359)] - **(SEMVER-MAJOR)** **http**: support old private properties and function (Brian White) [#10941](https://github.com/nodejs/node/pull/10941)
* [[`940b5303be`](https://github.com/nodejs/node/commit/940b5303be)] - **(SEMVER-MAJOR)** **http**: use Symbol for outgoing headers (Brian White) [#10941](https://github.com/nodejs/node/pull/10941)
* [[`fb71ba4921`](https://github.com/nodejs/node/commit/fb71ba4921)] - **(SEMVER-MAJOR)** **http**: docs-only deprecation of res.writeHeader() (James M Snell) [#11355](https://github.com/nodejs/node/pull/11355)
* [[`a4bb9fdb89`](https://github.com/nodejs/node/commit/a4bb9fdb89)] - **(SEMVER-MAJOR)** **http**: include provided status code in range error (cjihrig) [#11221](https://github.com/nodejs/node/pull/11221)
* [[`fc7025c9c0`](https://github.com/nodejs/node/commit/fc7025c9c0)] - **(SEMVER-MAJOR)** **http**: throw an error for unexpected agent values (brad-decker) [#10053](https://github.com/nodejs/node/pull/10053)
* [[`176cdc2823`](https://github.com/nodejs/node/commit/176cdc2823)] - **(SEMVER-MAJOR)** **http**: misc optimizations and style fixes (Brian White) [#10558](https://github.com/nodejs/node/pull/10558)
* [[`73d9445782`](https://github.com/nodejs/node/commit/73d9445782)] - **(SEMVER-MAJOR)** **http**: try to avoid lowercasing incoming headers (Brian White) [#10558](https://github.com/nodejs/node/pull/10558)
* [[`c77ed327d9`](https://github.com/nodejs/node/commit/c77ed327d9)] - **(SEMVER-MAJOR)** **http**: avoid using object for removed header status (Brian White) [#10558](https://github.com/nodejs/node/pull/10558)
* [[`c00e4adbb4`](https://github.com/nodejs/node/commit/c00e4adbb4)] - **(SEMVER-MAJOR)** **http**: optimize header storage and matching (Brian White) [#10558](https://github.com/nodejs/node/pull/10558)
* [[`ec8910bcea`](https://github.com/nodejs/node/commit/ec8910bcea)] - **(SEMVER-MAJOR)** **http**: check statusCode early (Brian White) [#10558](https://github.com/nodejs/node/pull/10558)
* [[`a73ab9de0d`](https://github.com/nodejs/node/commit/a73ab9de0d)] - **(SEMVER-MAJOR)** **http**: remove pointless use of arguments (cjihrig) [#10664](https://github.com/nodejs/node/pull/10664)
* [[`df3978421b`](https://github.com/nodejs/node/commit/df3978421b)] - **(SEMVER-MAJOR)** **http**: verify client method is a string (Luca Maraschi) [#10111](https://github.com/nodejs/node/pull/10111)
* [[`90476ac6ee`](https://github.com/nodejs/node/commit/90476ac6ee)] - **(SEMVER-MAJOR)** **lib**: remove _debugger.js (Ben Noordhuis) [#12495](https://github.com/nodejs/node/pull/12495)
* [[`3209a8ebf3`](https://github.com/nodejs/node/commit/3209a8ebf3)] - **(SEMVER-MAJOR)** **lib**: ensure --check flag works for piped-in code (Teddy Katz) [#11689](https://github.com/nodejs/node/pull/11689)
* [[`c67207731f`](https://github.com/nodejs/node/commit/c67207731f)] - **(SEMVER-MAJOR)** **lib**: simplify Module._resolveLookupPaths (Brian White) [#10789](https://github.com/nodejs/node/pull/10789)
* [[`28dc848e70`](https://github.com/nodejs/node/commit/28dc848e70)] - **(SEMVER-MAJOR)** **lib**: improve method of function calling (Brian White) [#10789](https://github.com/nodejs/node/pull/10789)
* [[`a851b868c0`](https://github.com/nodejs/node/commit/a851b868c0)] - **(SEMVER-MAJOR)** **lib**: remove sources of permanent deopts (Brian White) [#10789](https://github.com/nodejs/node/pull/10789)
* [[`62e96096fa`](https://github.com/nodejs/node/commit/62e96096fa)] - **(SEMVER-MAJOR)** **lib**: more consistent use of module.exports = {} model (James M Snell) [#11406](https://github.com/nodejs/node/pull/11406)
* [[`88c3f57cc3`](https://github.com/nodejs/node/commit/88c3f57cc3)] - **(SEMVER-MAJOR)** **lib**: refactor internal/socket_list (James M Snell) [#11406](https://github.com/nodejs/node/pull/11406)
* [[`f04387e9f2`](https://github.com/nodejs/node/commit/f04387e9f2)] - **(SEMVER-MAJOR)** **lib**: refactor internal/freelist (James M Snell) [#11406](https://github.com/nodejs/node/pull/11406)
* [[`d61a511728`](https://github.com/nodejs/node/commit/d61a511728)] - **(SEMVER-MAJOR)** **lib**: refactor internal/linkedlist (James M Snell) [#11406](https://github.com/nodejs/node/pull/11406)
* [[`2ba4eeadbb`](https://github.com/nodejs/node/commit/2ba4eeadbb)] - **(SEMVER-MAJOR)** **lib**: remove simd support from util.format() (Ben Noordhuis) [#11346](https://github.com/nodejs/node/pull/11346)
* [[`dfdd911e17`](https://github.com/nodejs/node/commit/dfdd911e17)] - **(SEMVER-MAJOR)** **lib**: deprecate node --debug at runtime (Josh Gavant) [#10970](https://github.com/nodejs/node/pull/10970)
* [[`5de3cf099c`](https://github.com/nodejs/node/commit/5de3cf099c)] - **(SEMVER-MAJOR)** **lib**: add static identifier codes for all deprecations (James M Snell) [#10116](https://github.com/nodejs/node/pull/10116)
* [[`4775942957`](https://github.com/nodejs/node/commit/4775942957)] - **(SEMVER-MAJOR)** **lib, test**: fix server.listen error message (Joyee Cheung) [#11693](https://github.com/nodejs/node/pull/11693)
* [[`caf9ae748b`](https://github.com/nodejs/node/commit/caf9ae748b)] - **(SEMVER-MAJOR)** **lib,src**: make constants not inherit from Object (Sakthipriyan Vairamani (thefourtheye)) [#10458](https://github.com/nodejs/node/pull/10458)
* [[`e0b076a949`](https://github.com/nodejs/node/commit/e0b076a949)] - **(SEMVER-MAJOR)** **lib,src,test**: update --debug/debug-brk comments (Ben Noordhuis) [#12495](https://github.com/nodejs/node/pull/12495)
* [[`b40dab553f`](https://github.com/nodejs/node/commit/b40dab553f)] - **(SEMVER-MAJOR)** **linkedlist**: remove unused methods (Brian White) [#11726](https://github.com/nodejs/node/pull/11726)
* [[`84a23391f6`](https://github.com/nodejs/node/commit/84a23391f6)] - **(SEMVER-MAJOR)** **linkedlist**: remove public module (Brian White) [#12113](https://github.com/nodejs/node/pull/12113)
* [[`e32425bfcd`](https://github.com/nodejs/node/commit/e32425bfcd)] - **(SEMVER-MAJOR)** **module**: avoid JSON.stringify() for cache key (Brian White) [#10789](https://github.com/nodejs/node/pull/10789)
* [[`403b89e72b`](https://github.com/nodejs/node/commit/403b89e72b)] - **(SEMVER-MAJOR)** **module**: avoid hasOwnProperty() (Brian White) [#10789](https://github.com/nodejs/node/pull/10789)
* [[`298a40e04e`](https://github.com/nodejs/node/commit/298a40e04e)] - **(SEMVER-MAJOR)** **module**: use "clean" objects (Brian White) [#10789](https://github.com/nodejs/node/pull/10789)
* [[`cf980b0311`](https://github.com/nodejs/node/commit/cf980b0311)] - **(SEMVER-MAJOR)** **net**: check and throw on error for getsockname (Daniel Bevenius) [#12871](https://github.com/nodejs/node/pull/12871)
* [[`473572ea25`](https://github.com/nodejs/node/commit/473572ea25)] - **(SEMVER-MAJOR)** **os**: refactor os structure, add Symbol.toPrimitive (James M Snell) [#12654](https://github.com/nodejs/node/pull/12654)
* [[`03e89b3ff2`](https://github.com/nodejs/node/commit/03e89b3ff2)] - **(SEMVER-MAJOR)** **process**: add --redirect-warnings command line argument (James M Snell) [#10116](https://github.com/nodejs/node/pull/10116)
* [[`5e1f32fd94`](https://github.com/nodejs/node/commit/5e1f32fd94)] - **(SEMVER-MAJOR)** **process**: add optional code to warnings + type checking (James M Snell) [#10116](https://github.com/nodejs/node/pull/10116)
* [[`a647d82f83`](https://github.com/nodejs/node/commit/a647d82f83)] - **(SEMVER-MAJOR)** **process**: improve process.hrtime (Joyee Cheung) [#10764](https://github.com/nodejs/node/pull/10764)
* [[`4e259b21a3`](https://github.com/nodejs/node/commit/4e259b21a3)] - **(SEMVER-MAJOR)** **querystring, url**: handle repeated sep in search (Daijiro Wachi) [#10967](https://github.com/nodejs/node/pull/10967)
* [[`39d9afe279`](https://github.com/nodejs/node/commit/39d9afe279)] - **(SEMVER-MAJOR)** **repl**: refactor `LineParser` implementation (Blake Embrey) [#6171](https://github.com/nodejs/node/pull/6171)
* [[`3f27f02da0`](https://github.com/nodejs/node/commit/3f27f02da0)] - **(SEMVER-MAJOR)** **repl**: docs-only deprecation of magic mode (Timothy Gu) [#11599](https://github.com/nodejs/node/pull/11599)
* [[`b77c89022b`](https://github.com/nodejs/node/commit/b77c89022b)] - **(SEMVER-MAJOR)** **repl**: remove magic mode semantics (Timothy Gu) [#11599](https://github.com/nodejs/node/pull/11599)
* [[`007386ee81`](https://github.com/nodejs/node/commit/007386ee81)] - **(SEMVER-MAJOR)** **repl**: remove workaround for function redefinition (Michaël Zasso) [#9618](https://github.com/nodejs/node/pull/9618)
* [[`5b63fabfd8`](https://github.com/nodejs/node/commit/5b63fabfd8)] - **(SEMVER-MAJOR)** **src**: update NODE_MODULE_VERSION to 55 (Michaël Zasso) [#12784](https://github.com/nodejs/node/pull/12784)
* [[`a16b570f8c`](https://github.com/nodejs/node/commit/a16b570f8c)] - **(SEMVER-MAJOR)** **src**: add --pending-deprecation and NODE_PENDING_DEPRECATION (James M Snell) [#11968](https://github.com/nodejs/node/pull/11968)
* [[`faa447b256`](https://github.com/nodejs/node/commit/faa447b256)] - **(SEMVER-MAJOR)** **src**: allow ArrayBufferView as instance of Buffer (Timothy Gu) [#12223](https://github.com/nodejs/node/pull/12223)
* [[`47f8f7462f`](https://github.com/nodejs/node/commit/47f8f7462f)] - **(SEMVER-MAJOR)** **src**: remove support for --debug (Jan Krems) [#12197](https://github.com/nodejs/node/pull/12197)
* [[`a5f91ab230`](https://github.com/nodejs/node/commit/a5f91ab230)] - **(SEMVER-MAJOR)** **src**: throw when -c and -e are used simultaneously (Teddy Katz) [#11689](https://github.com/nodejs/node/pull/11689)
* [[`8a7db9d4b5`](https://github.com/nodejs/node/commit/8a7db9d4b5)] - **(SEMVER-MAJOR)** **src**: add --use-bundled-ca --use-openssl-ca check (Daniel Bevenius) [#12087](https://github.com/nodejs/node/pull/12087)
* [[`ed12ea371c`](https://github.com/nodejs/node/commit/ed12ea371c)] - **(SEMVER-MAJOR)** **src**: update inspector code to match upstream API (Michaël Zasso) [#11752](https://github.com/nodejs/node/pull/11752)
* [[`89d8dc9afd`](https://github.com/nodejs/node/commit/89d8dc9afd)] - **(SEMVER-MAJOR)** **src**: update NODE_MODULE_VERSION to 54 (Michaël Zasso) [#11752](https://github.com/nodejs/node/pull/11752)
* [[`1125c8a814`](https://github.com/nodejs/node/commit/1125c8a814)] - **(SEMVER-MAJOR)** **src**: fix typos in node_lttng_provider.h (Benjamin Fleischer) [#11723](https://github.com/nodejs/node/pull/11723)
* [[`aae8f683b4`](https://github.com/nodejs/node/commit/aae8f683b4)] - **(SEMVER-MAJOR)** **src**: update NODE_MODULE_VERSION to 53 (Michaël Zasso) [#10992](https://github.com/nodejs/node/pull/10992)
* [[`91ab09fe2a`](https://github.com/nodejs/node/commit/91ab09fe2a)] - **(SEMVER-MAJOR)** **src**: update NODE_MODULE_VERSION to 52 (Michaël Zasso) [#9618](https://github.com/nodejs/node/pull/9618)
* [[`334be0feba`](https://github.com/nodejs/node/commit/334be0feba)] - **(SEMVER-MAJOR)** **src**: fix space for module version mismatch error (Yann Pringault) [#10606](https://github.com/nodejs/node/pull/10606)
* [[`45c9ca7fd4`](https://github.com/nodejs/node/commit/45c9ca7fd4)] - **(SEMVER-MAJOR)** **src**: remove redundant spawn/spawnSync type checks (cjihrig) [#8312](https://github.com/nodejs/node/pull/8312)
* [[`b374ee8c3d`](https://github.com/nodejs/node/commit/b374ee8c3d)] - **(SEMVER-MAJOR)** **src**: add handle check to spawn_sync (cjihrig) [#8312](https://github.com/nodejs/node/pull/8312)
* [[`3295a7feba`](https://github.com/nodejs/node/commit/3295a7feba)] - **(SEMVER-MAJOR)** **src**: allow getting Symbols on process.env (Anna Henningsen) [#9631](https://github.com/nodejs/node/pull/9631)
* [[`1aa595e5bd`](https://github.com/nodejs/node/commit/1aa595e5bd)] - **(SEMVER-MAJOR)** **src**: throw on process.env symbol usage (cjihrig) [#9446](https://github.com/nodejs/node/pull/9446)
* [[`a235ccd168`](https://github.com/nodejs/node/commit/a235ccd168)] - **(SEMVER-MAJOR)** **src,test**: debug is now an alias for inspect (Ali Ijaz Sheikh) [#11441](https://github.com/nodejs/node/pull/11441)
* [[`b6e1d22fa6`](https://github.com/nodejs/node/commit/b6e1d22fa6)] - **(SEMVER-MAJOR)** **stream**: add destroy and _destroy methods. (Matteo Collina) [#12925](https://github.com/nodejs/node/pull/12925)
* [[`f36c970cf3`](https://github.com/nodejs/node/commit/f36c970cf3)] - **(SEMVER-MAJOR)** **stream**: improve multiple callback error message (cjihrig) [#12520](https://github.com/nodejs/node/pull/12520)
* [[`202b07f414`](https://github.com/nodejs/node/commit/202b07f414)] - **(SEMVER-MAJOR)** **stream**: fix comment for sync flag of ReadableState (Wang Xinyong) [#11139](https://github.com/nodejs/node/pull/11139)
* [[`1004b9b4ad`](https://github.com/nodejs/node/commit/1004b9b4ad)] - **(SEMVER-MAJOR)** **stream**: remove unused `ranOut` from ReadableState (Wang Xinyong) [#11139](https://github.com/nodejs/node/pull/11139)
* [[`03b9f6fe26`](https://github.com/nodejs/node/commit/03b9f6fe26)] - **(SEMVER-MAJOR)** **stream**: avoid instanceof (Brian White) [#10558](https://github.com/nodejs/node/pull/10558)
* [[`a3539ae3be`](https://github.com/nodejs/node/commit/a3539ae3be)] - **(SEMVER-MAJOR)** **stream**: use plain objects for write/corked reqs (Brian White) [#10558](https://github.com/nodejs/node/pull/10558)
* [[`24ef1e6775`](https://github.com/nodejs/node/commit/24ef1e6775)] - **(SEMVER-MAJOR)** **string_decoder**: align UTF-8 handling with V8 (Brian White) [#9618](https://github.com/nodejs/node/pull/9618)
* [[`23fc082409`](https://github.com/nodejs/node/commit/23fc082409)] - **(SEMVER-MAJOR)** **test**: remove extra console output from test-os.js (James M Snell) [#12654](https://github.com/nodejs/node/pull/12654)
* [[`59c6230861`](https://github.com/nodejs/node/commit/59c6230861)] - **(SEMVER-MAJOR)** **test**: cleanup test-child-process-constructor.js (cjihrig) [#12348](https://github.com/nodejs/node/pull/12348)
* [[`06c29a66d4`](https://github.com/nodejs/node/commit/06c29a66d4)] - **(SEMVER-MAJOR)** **test**: remove common.fail() (Rich Trott) [#12293](https://github.com/nodejs/node/pull/12293)
* [[`0c539faac3`](https://github.com/nodejs/node/commit/0c539faac3)] - **(SEMVER-MAJOR)** **test**: add common.getArrayBufferViews(buf) (Timothy Gu) [#12223](https://github.com/nodejs/node/pull/12223)
* [[`c5d1851ac4`](https://github.com/nodejs/node/commit/c5d1851ac4)] - **(SEMVER-MAJOR)** **test**: drop v5.x-specific code path from simd test (Ben Noordhuis) [#11346](https://github.com/nodejs/node/pull/11346)
* [[`c2c6ae52ea`](https://github.com/nodejs/node/commit/c2c6ae52ea)] - **(SEMVER-MAJOR)** **test**: move test-vm-function-redefinition to parallel (Franziska Hinkelmann) [#9618](https://github.com/nodejs/node/pull/9618)
* [[`5b30c4f24d`](https://github.com/nodejs/node/commit/5b30c4f24d)] - **(SEMVER-MAJOR)** **test**: refactor test-child-process-spawnsync-maxbuf (cjihrig) [#10769](https://github.com/nodejs/node/pull/10769)
* [[`348cc80a3c`](https://github.com/nodejs/node/commit/348cc80a3c)] - **(SEMVER-MAJOR)** **tls**: make rejectUnauthorized default to true (ghaiklor) [#5923](https://github.com/nodejs/node/pull/5923)
* [[`a2ae08999b`](https://github.com/nodejs/node/commit/a2ae08999b)] - **(SEMVER-MAJOR)** **tls**: runtime deprecation for tls.createSecurePair() (James M Snell) [#11349](https://github.com/nodejs/node/pull/11349)
* [[`d523eb9c40`](https://github.com/nodejs/node/commit/d523eb9c40)] - **(SEMVER-MAJOR)** **tls**: use emitWarning() for dhparam \< 2048 bits (James M Snell) [#11447](https://github.com/nodejs/node/pull/11447)
* [[`e03a929648`](https://github.com/nodejs/node/commit/e03a929648)] - **(SEMVER-MAJOR)** **tools**: update test-npm.sh paths (Kat Marchán) [#12936](https://github.com/nodejs/node/pull/12936)
* [[`6f202ef857`](https://github.com/nodejs/node/commit/6f202ef857)] - **(SEMVER-MAJOR)** **tools**: remove assert.fail() lint rule (Rich Trott) [#12293](https://github.com/nodejs/node/pull/12293)
* [[`615789b723`](https://github.com/nodejs/node/commit/615789b723)] - **(SEMVER-MAJOR)** **tools**: enable ES2017 syntax support in ESLint (Michaël Zasso) [#11210](https://github.com/nodejs/node/pull/11210)
* [[`1b63fa1096`](https://github.com/nodejs/node/commit/1b63fa1096)] - **(SEMVER-MAJOR)** **tty**: remove NODE_TTY_UNSAFE_ASYNC (Jeremiah Senkpiel) [#12057](https://github.com/nodejs/node/pull/12057)
* [[`78182458e6`](https://github.com/nodejs/node/commit/78182458e6)] - **(SEMVER-MAJOR)** **url**: fix error message of url.format (DavidCai) [#11162](https://github.com/nodejs/node/pull/11162)
* [[`c65d55f087`](https://github.com/nodejs/node/commit/c65d55f087)] - **(SEMVER-MAJOR)** **url**: do not truncate long hostnames (Junshu Okamoto) [#9292](https://github.com/nodejs/node/pull/9292)
* [[`3cc3e099be`](https://github.com/nodejs/node/commit/3cc3e099be)] - **(SEMVER-MAJOR)** **util**: show External values explicitly in inspect (Anna Henningsen) [#12151](https://github.com/nodejs/node/pull/12151)
* [[`4a5a9445b5`](https://github.com/nodejs/node/commit/4a5a9445b5)] - **(SEMVER-MAJOR)** **util**: use `\[Array\]` for deeply nested arrays (Anna Henningsen) [#12046](https://github.com/nodejs/node/pull/12046)
* [[`5bfd13b81e`](https://github.com/nodejs/node/commit/5bfd13b81e)] - **(SEMVER-MAJOR)** **util**: display Symbol keys in inspect by default (Shahar Or) [#9726](https://github.com/nodejs/node/pull/9726)
* [[`455e6f1dd8`](https://github.com/nodejs/node/commit/455e6f1dd8)] - **(SEMVER-MAJOR)** **util**: throw toJSON errors when formatting %j (Timothy Gu) [#11708](https://github.com/nodejs/node/pull/11708)
* [[`ec2f098156`](https://github.com/nodejs/node/commit/ec2f098156)] - **(SEMVER-MAJOR)** **util**: change sparse arrays inspection format (Alexey Orlenko) [#11576](https://github.com/nodejs/node/pull/11576)
* [[`aab0d202f8`](https://github.com/nodejs/node/commit/aab0d202f8)] - **(SEMVER-MAJOR)** **util**: convert inspect.styles and inspect.colors to prototype-less objects (Nemanja Stojanovic) [#11624](https://github.com/nodejs/node/pull/11624)
* [[`4151ab398b`](https://github.com/nodejs/node/commit/4151ab398b)] - **(SEMVER-MAJOR)** **util**: add createClassWrapper to internal/util (James M Snell) [#11391](https://github.com/nodejs/node/pull/11391)
* [[`f65aa08b52`](https://github.com/nodejs/node/commit/f65aa08b52)] - **(SEMVER-MAJOR)** **util**: improve inspect for (Async|Generator)Function (Michaël Zasso) [#11210](https://github.com/nodejs/node/pull/11210)
* [[`efae43f0ee`](https://github.com/nodejs/node/commit/efae43f0ee)] - **(SEMVER-MAJOR)** **zlib**: fix node crashing on invalid options (Alexey Orlenko) [#13098](https://github.com/nodejs/node/pull/13098)
* [[`2ced07ccaf`](https://github.com/nodejs/node/commit/2ced07ccaf)] - **(SEMVER-MAJOR)** **zlib**: support all ArrayBufferView types (Timothy Gu) [#12223](https://github.com/nodejs/node/pull/12223)
* [[`91383e47fd`](https://github.com/nodejs/node/commit/91383e47fd)] - **(SEMVER-MAJOR)** **zlib**: support Uint8Array in convenience methods (Timothy Gu) [#12001](https://github.com/nodejs/node/pull/12001)
* [[`b514bd231e`](https://github.com/nodejs/node/commit/b514bd231e)] - **(SEMVER-MAJOR)** **zlib**: use RangeError/TypeError consistently (James M Snell) [#11391](https://github.com/nodejs/node/pull/11391)
* [[`8e69f7e385`](https://github.com/nodejs/node/commit/8e69f7e385)] - **(SEMVER-MAJOR)** **zlib**: refactor zlib module (James M Snell) [#11391](https://github.com/nodejs/node/pull/11391)
* [[`dd928b04fc`](https://github.com/nodejs/node/commit/dd928b04fc)] - **(SEMVER-MAJOR)** **zlib**: be strict about what strategies are accepted (Rich Trott) [#10934](https://github.com/nodejs/node/pull/10934)

#### Semver-minor Commits

* [[`7e3a3c962f`](https://github.com/nodejs/node/commit/7e3a3c962f)] - **(SEMVER-MINOR)** **async_hooks**: initial async_hooks implementation (Trevor Norris) [#12892](https://github.com/nodejs/node/pull/12892)
* [[`60a2fe7d47`](https://github.com/nodejs/node/commit/60a2fe7d47)] - **(SEMVER-MINOR)** **async_hooks**: implement C++ embedder API (Anna Henningsen) [#13142](https://github.com/nodejs/node/pull/13142)
* [[`f1ed19d98f`](https://github.com/nodejs/node/commit/f1ed19d98f)] - **(SEMVER-MINOR)** **async_wrap**: use more specific providers (Trevor Norris) [#12892](https://github.com/nodejs/node/pull/12892)
* [[`0432c6e91e`](https://github.com/nodejs/node/commit/0432c6e91e)] - **(SEMVER-MINOR)** **async_wrap**: use double, not int64_t, for async id (Trevor Norris) [#12892](https://github.com/nodejs/node/pull/12892)
* [[`fe2df3b842`](https://github.com/nodejs/node/commit/fe2df3b842)] - **(SEMVER-MINOR)** **async_wrap,src**: add GetAsyncId() method (Trevor Norris) [#12892](https://github.com/nodejs/node/pull/12892)
* [[`6d93508369`](https://github.com/nodejs/node/commit/6d93508369)] - **(SEMVER-MINOR)** **buffer**: expose FastBuffer on internal/buffer (Anna Henningsen) [#11048](https://github.com/nodejs/node/pull/11048)
* [[`fe5ca3ff27`](https://github.com/nodejs/node/commit/fe5ca3ff27)] - **(SEMVER-MINOR)** **child_process**: support promisified `exec(File)` (Anna Henningsen) [#12442](https://github.com/nodejs/node/pull/12442)
* [[`f146fe4ed4`](https://github.com/nodejs/node/commit/f146fe4ed4)] - **(SEMVER-MINOR)** **cmd**: support dash as stdin alias (Ebrahim Byagowi) [#13012](https://github.com/nodejs/node/pull/13012)
* [[`d9f3ec8e09`](https://github.com/nodejs/node/commit/d9f3ec8e09)] - **(SEMVER-MINOR)** **crypto**: use named FunctionTemplate (Trevor Norris) [#12892](https://github.com/nodejs/node/pull/12892)
* [[`0e710aada4`](https://github.com/nodejs/node/commit/0e710aada4)] - **(SEMVER-MINOR)** **crypto**: add sign/verify support for RSASSA-PSS (Tobias Nießen) [#11705](https://github.com/nodejs/node/pull/11705)
* [[`faf6654ff7`](https://github.com/nodejs/node/commit/faf6654ff7)] - **(SEMVER-MINOR)** **dns**: support promisified `lookup(Service)` (Anna Henningsen) [#12442](https://github.com/nodejs/node/pull/12442)
* [[`5077cde71f`](https://github.com/nodejs/node/commit/5077cde71f)] - **(SEMVER-MINOR)** **doc**: restructure url.md (James M Snell) [#12710](https://github.com/nodejs/node/pull/12710)
* [[`d080ead0f9`](https://github.com/nodejs/node/commit/d080ead0f9)] - **(SEMVER-MINOR)** **doc**: graduate WHATWG URL from Experimental (James M Snell) [#12710](https://github.com/nodejs/node/pull/12710)
* [[`c505b8109e`](https://github.com/nodejs/node/commit/c505b8109e)] - **(SEMVER-MINOR)** **doc**: document URLSearchParams constructor (Timothy Gu) [#11060](https://github.com/nodejs/node/pull/11060)
* [[`84dabe8373`](https://github.com/nodejs/node/commit/84dabe8373)] - **(SEMVER-MINOR)** **domain**: support promises (Anna Henningsen) [#12489](https://github.com/nodejs/node/pull/12489)
* [[`fbcb4f50b8`](https://github.com/nodejs/node/commit/fbcb4f50b8)] - **(SEMVER-MINOR)** **fs**: support util.promisify for fs.read/fs.write (Anna Henningsen) [#12442](https://github.com/nodejs/node/pull/12442)
* [[`a7f5c9cb7d`](https://github.com/nodejs/node/commit/a7f5c9cb7d)] - **(SEMVER-MINOR)** **http**: destroy sockets after keepAliveTimeout (Timur Shemsedinov) [#2534](https://github.com/nodejs/node/pull/2534)
* [[`3e6f1032a4`](https://github.com/nodejs/node/commit/3e6f1032a4)] - **(SEMVER-MINOR)** **http**: add new functions to OutgoingMessage (Brian White) [#10805](https://github.com/nodejs/node/pull/10805)
* [[`c7182b9d9c`](https://github.com/nodejs/node/commit/c7182b9d9c)] - **(SEMVER-MINOR)** **inspector**: JavaScript bindings for the inspector (Eugene Ostroukhov) [#12263](https://github.com/nodejs/node/pull/12263)
* [[`4a7233c178`](https://github.com/nodejs/node/commit/4a7233c178)] - **(SEMVER-MINOR)** **lib**: implement async_hooks API in core (Trevor Norris) [#12892](https://github.com/nodejs/node/pull/12892)
* [[`c68ebe8436`](https://github.com/nodejs/node/commit/c68ebe8436)] - **(SEMVER-MINOR)** **makefile**: add async-hooks to test and test-ci (Trevor Norris) [#12892](https://github.com/nodejs/node/pull/12892)
* [[`45139e59f3`](https://github.com/nodejs/node/commit/45139e59f3)] - **(SEMVER-MINOR)** **n-api**: add napi_get_version (Michael Dawson) [#13207](https://github.com/nodejs/node/pull/13207)
* [[`56e881d0b0`](https://github.com/nodejs/node/commit/56e881d0b0)] - **(SEMVER-MINOR)** **n-api**: add support for abi stable module API (Jason Ginchereau) [#11975](https://github.com/nodejs/node/pull/11975)
* [[`dd20e68b0f`](https://github.com/nodejs/node/commit/dd20e68b0f)] - **(SEMVER-MINOR)** **process**: add optional detail to process emitWarning (James M Snell) [#12725](https://github.com/nodejs/node/pull/12725)
* [[`c0bde73f1b`](https://github.com/nodejs/node/commit/c0bde73f1b)] - **(SEMVER-MINOR)** **src**: implement native changes for async_hooks (Trevor Norris) [#12892](https://github.com/nodejs/node/pull/12892)
* [[`e5a25cbc85`](https://github.com/nodejs/node/commit/e5a25cbc85)] - **(SEMVER-MINOR)** **src**: expose `node::AddPromiseHook` (Anna Henningsen) [#12489](https://github.com/nodejs/node/pull/12489)
* [[`ec53921d2e`](https://github.com/nodejs/node/commit/ec53921d2e)] - **(SEMVER-MINOR)** **src**: make AtExit callback's per Environment (Daniel Bevenius) [#9163](https://github.com/nodejs/node/pull/9163)
* [[`ba4847e879`](https://github.com/nodejs/node/commit/ba4847e879)] - **(SEMVER-MINOR)** **src**: Node Tracing Controller (misterpoe) [#9304](https://github.com/nodejs/node/pull/9304)
* [[`6ff3b03240`](https://github.com/nodejs/node/commit/6ff3b03240)] - **(SEMVER-MINOR)** **src, inspector**: add --inspect-brk option (Josh Gavant) [#8979](https://github.com/nodejs/node/pull/8979)
* [[`220186c4a8`](https://github.com/nodejs/node/commit/220186c4a8)] - **(SEMVER-MINOR)** **stream**: support Uint8Array input to methods (Anna Henningsen) [#11608](https://github.com/nodejs/node/pull/11608)
* [[`07c7f198db`](https://github.com/nodejs/node/commit/07c7f198db)] - **(SEMVER-MINOR)** **stream**: add final method (Calvin Metcalf) [#12828](https://github.com/nodejs/node/pull/12828)
* [[`11918c4aed`](https://github.com/nodejs/node/commit/11918c4aed)] - **(SEMVER-MINOR)** **stream**: fix highWaterMark integer overflow (Tobias Nießen) [#12593](https://github.com/nodejs/node/pull/12593)
* [[`c56d6046ec`](https://github.com/nodejs/node/commit/c56d6046ec)] - **(SEMVER-MINOR)** **test**: add AsyncResource addon test (Anna Henningsen) [#13142](https://github.com/nodejs/node/pull/13142)
* [[`e3e56f1d71`](https://github.com/nodejs/node/commit/e3e56f1d71)] - **(SEMVER-MINOR)** **test**: adding tests for initHooks API (Thorsten Lorenz) [#12892](https://github.com/nodejs/node/pull/12892)
* [[`732620cfe9`](https://github.com/nodejs/node/commit/732620cfe9)] - **(SEMVER-MINOR)** **test**: remove unneeded tests (Trevor Norris) [#12892](https://github.com/nodejs/node/pull/12892)
* [[`e965ed16c1`](https://github.com/nodejs/node/commit/e965ed16c1)] - **(SEMVER-MINOR)** **test**: add test for promisify customPromisifyArgs (Gil Tayar) [#12442](https://github.com/nodejs/node/pull/12442)
* [[`3ea2301e38`](https://github.com/nodejs/node/commit/3ea2301e38)] - **(SEMVER-MINOR)** **test**: add a bunch of tests from bluebird (Madara Uchiha) [#12442](https://github.com/nodejs/node/pull/12442)
* [[`dca08152cb`](https://github.com/nodejs/node/commit/dca08152cb)] - **(SEMVER-MINOR)** **test**: introduce `common.crashOnUnhandledRejection` (Anna Henningsen) [#12489](https://github.com/nodejs/node/pull/12489)
* [[`e7c51454b0`](https://github.com/nodejs/node/commit/e7c51454b0)] - **(SEMVER-MINOR)** **timers**: add promisify support (Anna Henningsen) [#12442](https://github.com/nodejs/node/pull/12442)
* [[`e600fbe576`](https://github.com/nodejs/node/commit/e600fbe576)] - **(SEMVER-MINOR)** **tls**: accept `lookup` option for `tls.connect()` (Fedor Indutny) [#12839](https://github.com/nodejs/node/pull/12839)
* [[`c3efe72669`](https://github.com/nodejs/node/commit/c3efe72669)] - **(SEMVER-MINOR)** **tls**: support Uint8Arrays for protocol list buffers (Anna Henningsen) [#11984](https://github.com/nodejs/node/pull/11984)
* [[`29f758731f`](https://github.com/nodejs/node/commit/29f758731f)] - **(SEMVER-MINOR)** **tools**: add MDN link for Iterable (Timothy Gu) [#11060](https://github.com/nodejs/node/pull/11060)
* [[`4b9d84df51`](https://github.com/nodejs/node/commit/4b9d84df51)] - **(SEMVER-MINOR)** **tty_wrap**: throw when uv_tty_init() returns error (Trevor Norris) [#12892](https://github.com/nodejs/node/pull/12892)
* [[`cc48f21c83`](https://github.com/nodejs/node/commit/cc48f21c83)] - **(SEMVER-MINOR)** **url**: extend URLSearchParams constructor (Timothy Gu) [#11060](https://github.com/nodejs/node/pull/11060)
* [[`99da8e8e02`](https://github.com/nodejs/node/commit/99da8e8e02)] - **(SEMVER-MINOR)** **util**: add util.promisify() (Anna Henningsen) [#12442](https://github.com/nodejs/node/pull/12442)
* [[`059f296050`](https://github.com/nodejs/node/commit/059f296050)] - **(SEMVER-MINOR)** **util**: add internal bindings for promise handling (Anna Henningsen) [#12442](https://github.com/nodejs/node/pull/12442)
* [[`1fde98bb4f`](https://github.com/nodejs/node/commit/1fde98bb4f)] - **(SEMVER-MINOR)** **v8**: expose new V8 serialization API (Anna Henningsen) [#11048](https://github.com/nodejs/node/pull/11048)
* [[`70beef97bd`](https://github.com/nodejs/node/commit/70beef97bd)] - **(SEMVER-MINOR)** **v8**: add cachedDataVersionTag (Andres Suarez) [#11515](https://github.com/nodejs/node/pull/11515)

#### Semver-patch commits

* [[`276720921b`](https://github.com/nodejs/node/commit/276720921b)] - **addons**: remove semicolons from after module definition (Gabriel Schulhof) [#12919](https://github.com/nodejs/node/pull/12919)
* [[`f6247a945c`](https://github.com/nodejs/node/commit/f6247a945c)] - **assert**: restore TypeError if no arguments (Rich Trott) [#12843](https://github.com/nodejs/node/pull/12843)
* [[`7e5f500c98`](https://github.com/nodejs/node/commit/7e5f500c98)] - **assert**: improve deepEqual perf for large input (Anna Henningsen) [#12849](https://github.com/nodejs/node/pull/12849)
* [[`3863c3ae66`](https://github.com/nodejs/node/commit/3863c3ae66)] - **async_hooks**: rename AsyncEvent to AsyncResource (Anna Henningsen) [#13192](https://github.com/nodejs/node/pull/13192)
* [[`e554bb449e`](https://github.com/nodejs/node/commit/e554bb449e)] - **async_hooks**: only set up hooks if used (Anna Henningsen) [#13177](https://github.com/nodejs/node/pull/13177)
* [[`6fb27af70a`](https://github.com/nodejs/node/commit/6fb27af70a)] - **async_hooks**: add constructor check to async-hooks (Shadowbeetle) [#13096](https://github.com/nodejs/node/pull/13096)
* [[`a6023ee0b5`](https://github.com/nodejs/node/commit/a6023ee0b5)] - **async_wrap**: fix Promises with later enabled hooks (Anna Henningsen) [#13242](https://github.com/nodejs/node/pull/13242)
* [[`6bfdeedce5`](https://github.com/nodejs/node/commit/6bfdeedce5)] - **async_wrap**: add `asyncReset` to `TLSWrap` (Refael Ackermann) [#13092](https://github.com/nodejs/node/pull/13092)
* [[`4a8ea63b3b`](https://github.com/nodejs/node/commit/4a8ea63b3b)] - **async_wrap,src**: wrap promises directly (Matt Loring) [#13242](https://github.com/nodejs/node/pull/13242)
* [[`6e4394fb0b`](https://github.com/nodejs/node/commit/6e4394fb0b)] - **async_wrap,src**: promise hook integration (Matt Loring) [#13000](https://github.com/nodejs/node/pull/13000)
* [[`72429b3981`](https://github.com/nodejs/node/commit/72429b3981)] - **benchmark**: allow no duration in benchmark tests (Rich Trott) [#13110](https://github.com/nodejs/node/pull/13110)
* [[`f2ba06db92`](https://github.com/nodejs/node/commit/f2ba06db92)] - **benchmark**: remove redundant timers benchmark (Rich Trott) [#13009](https://github.com/nodejs/node/pull/13009)
* [[`3fa5d80eda`](https://github.com/nodejs/node/commit/3fa5d80eda)] - **benchmark**: chunky http client should exit with 0 (Joyee Cheung) [#12916](https://github.com/nodejs/node/pull/12916)
* [[`a82e0e6f36`](https://github.com/nodejs/node/commit/a82e0e6f36)] - **benchmark**: check for time precision in common.js (Rich Trott) [#12934](https://github.com/nodejs/node/pull/12934)
* [[`65d6249979`](https://github.com/nodejs/node/commit/65d6249979)] - **benchmark**: update an obsolete path (Vse Mozhet Byt) [#12904](https://github.com/nodejs/node/pull/12904)
* [[`d8965d5b0e`](https://github.com/nodejs/node/commit/d8965d5b0e)] - **benchmark**: fix typo in _http-benchmarkers.js (Vse Mozhet Byt) [#12455](https://github.com/nodejs/node/pull/12455)
* [[`a3778cb9b1`](https://github.com/nodejs/node/commit/a3778cb9b1)] - **benchmark**: fix URL in _http-benchmarkers.js (Vse Mozhet Byt) [#12455](https://github.com/nodejs/node/pull/12455)
* [[`22aa3d4899`](https://github.com/nodejs/node/commit/22aa3d4899)] - **benchmark**: reduce string concatenations (Vse Mozhet Byt) [#12455](https://github.com/nodejs/node/pull/12455)
* [[`3e3414f45f`](https://github.com/nodejs/node/commit/3e3414f45f)] - **benchmark**: control HTTP benchmarks run time (Joyee Cheung) [#12121](https://github.com/nodejs/node/pull/12121)
* [[`a3e71a8901`](https://github.com/nodejs/node/commit/a3e71a8901)] - **benchmark**: add test double HTTP benchmarker (Joyee Cheung) [#12121](https://github.com/nodejs/node/pull/12121)
* [[`a6e69f8c08`](https://github.com/nodejs/node/commit/a6e69f8c08)] - **benchmark**: add more options to map-bench (Timothy Gu) [#11930](https://github.com/nodejs/node/pull/11930)
* [[`ae8a8691e6`](https://github.com/nodejs/node/commit/ae8a8691e6)] - **benchmark**: add final clean-up to module-loader.js (Vse Mozhet Byt) [#11924](https://github.com/nodejs/node/pull/11924)
* [[`6df23fa39f`](https://github.com/nodejs/node/commit/6df23fa39f)] - **benchmark**: fix punycode and get-ciphers benchmark (Bartosz Sosnowski) [#11720](https://github.com/nodejs/node/pull/11720)
* [[`75cdc895ec`](https://github.com/nodejs/node/commit/75cdc895ec)] - **benchmark**: cleanup after forced optimization drop (Bartosz Sosnowski) [#9615](https://github.com/nodejs/node/pull/9615)
* [[`ca86aa5323`](https://github.com/nodejs/node/commit/ca86aa5323)] - **benchmark**: remove forced optimization from util (Bartosz Sosnowski) [#9615](https://github.com/nodejs/node/pull/9615)
* [[`c5958d20fd`](https://github.com/nodejs/node/commit/c5958d20fd)] - **benchmark**: remove forced optimization from url (Bartosz Sosnowski) [#9615](https://github.com/nodejs/node/pull/9615)
* [[`ea61ce518b`](https://github.com/nodejs/node/commit/ea61ce518b)] - **benchmark**: remove forced optimization from tls (Bartosz Sosnowski) [#9615](https://github.com/nodejs/node/pull/9615)
* [[`541119c6ee`](https://github.com/nodejs/node/commit/541119c6ee)] - **benchmark**: remove streams forced optimization (Bartosz Sosnowski) [#9615](https://github.com/nodejs/node/pull/9615)
* [[`57b5ce1d8e`](https://github.com/nodejs/node/commit/57b5ce1d8e)] - **benchmark**: remove querystring forced optimization (Bartosz Sosnowski) [#9615](https://github.com/nodejs/node/pull/9615)
* [[`eba2c62bb1`](https://github.com/nodejs/node/commit/eba2c62bb1)] - **benchmark**: remove forced optimization from path (Bartosz Sosnowski) [#9615](https://github.com/nodejs/node/pull/9615)
* [[`7587a11adc`](https://github.com/nodejs/node/commit/7587a11adc)] - **benchmark**: remove forced optimization from misc (Bartosz Sosnowski) [#9615](https://github.com/nodejs/node/pull/9615)
* [[`ef8cc301fe`](https://github.com/nodejs/node/commit/ef8cc301fe)] - **benchmark**: remove forced optimization from es (Bartosz Sosnowski) [#9615](https://github.com/nodejs/node/pull/9615)
* [[`17c85ffd80`](https://github.com/nodejs/node/commit/17c85ffd80)] - **benchmark**: remove forced optimization from crypto (Bartosz Sosnowski) [#9615](https://github.com/nodejs/node/pull/9615)
* [[`05ac6e1b01`](https://github.com/nodejs/node/commit/05ac6e1b01)] - **benchmark**: remove forced optimization from buffer (Bartosz Sosnowski) [#9615](https://github.com/nodejs/node/pull/9615)
* [[`6123ed5b25`](https://github.com/nodejs/node/commit/6123ed5b25)] - **benchmark**: add USVString conversion benchmark (Timothy Gu) [#11436](https://github.com/nodejs/node/pull/11436)
* [[`28ddac2ec2`](https://github.com/nodejs/node/commit/28ddac2ec2)] - **buffer**: fix indexOf for empty searches (Anna Henningsen) [#13024](https://github.com/nodejs/node/pull/13024)
* [[`9d723e85fb`](https://github.com/nodejs/node/commit/9d723e85fb)] - **buffer**: remove pointless C++ utility methods (Anna Henningsen) [#12760](https://github.com/nodejs/node/pull/12760)
* [[`7cd0d4f644`](https://github.com/nodejs/node/commit/7cd0d4f644)] - **buffer**: fix backwards incompatibility (Brian White) [#12439](https://github.com/nodejs/node/pull/12439)
* [[`3ee4a1a281`](https://github.com/nodejs/node/commit/3ee4a1a281)] - **buffer**: optimize toString() (Brian White) [#12361](https://github.com/nodejs/node/pull/12361)
* [[`4a86803f60`](https://github.com/nodejs/node/commit/4a86803f60)] - **buffer**: optimize from() and byteLength() (Brian White) [#12361](https://github.com/nodejs/node/pull/12361)
* [[`00c86cc8e9`](https://github.com/nodejs/node/commit/00c86cc8e9)] - **buffer**: remove Uint8Array check (Nikolai Vavilov) [#11324](https://github.com/nodejs/node/pull/11324)
* [[`fb6cf2f861`](https://github.com/nodejs/node/commit/fb6cf2f861)] - **build**: xz tarball extreme compression (Peter Dave Hello) [#10626](https://github.com/nodejs/node/pull/10626)
* [[`4f4d5d24f4`](https://github.com/nodejs/node/commit/4f4d5d24f4)] - **build**: ignore more VC++ artifacts (Refael Ackermann) [#13208](https://github.com/nodejs/node/pull/13208)
* [[`735902f326`](https://github.com/nodejs/node/commit/735902f326)] - **build**: support dtrace on ARM (Bradley T. Hughes) [#12193](https://github.com/nodejs/node/pull/12193)
* [[`46bd32e7e8`](https://github.com/nodejs/node/commit/46bd32e7e8)] - **build**: fix openssl link error on windows (Daniel Bevenius) [#13078](https://github.com/nodejs/node/pull/13078)
* [[`77dfa2b1da`](https://github.com/nodejs/node/commit/77dfa2b1da)] - **build**: avoid /docs/api and /docs/doc/api upload (Rod Vagg) [#12957](https://github.com/nodejs/node/pull/12957)
* [[`6342988053`](https://github.com/nodejs/node/commit/6342988053)] - **build**: clean up napi build in test-addons-clean (Joyee Cheung) [#13034](https://github.com/nodejs/node/pull/13034)
* [[`ad7b98baa8`](https://github.com/nodejs/node/commit/ad7b98baa8)] - **build**: don't print directory for GNUMake (Daniel Bevenius) [#13042](https://github.com/nodejs/node/pull/13042)
* [[`80355271c3`](https://github.com/nodejs/node/commit/80355271c3)] - **build**: simplify `if` in setting of arg_paths (Refael Ackermann) [#12653](https://github.com/nodejs/node/pull/12653)
* [[`4aff0563aa`](https://github.com/nodejs/node/commit/4aff0563aa)] - **build**: reduce one level of spawning in node_gyp (Refael Ackermann) [#12653](https://github.com/nodejs/node/pull/12653)
* [[`9fd22bc4d4`](https://github.com/nodejs/node/commit/9fd22bc4d4)] - **build**: fix ninja build failure (GYP patch) (Daniel Bevenius) [#12484](https://github.com/nodejs/node/pull/12484)
* [[`bb88caec06`](https://github.com/nodejs/node/commit/bb88caec06)] - **build**: fix ninja build failure (Daniel Bevenius) [#12484](https://github.com/nodejs/node/pull/12484)
* [[`e488857402`](https://github.com/nodejs/node/commit/e488857402)] - **build**: add static option to vcbuild.bat (Tony Rice) [#12764](https://github.com/nodejs/node/pull/12764)
* [[`d727d5d2cf`](https://github.com/nodejs/node/commit/d727d5d2cf)] - **build**: enable cctest to use objects (gyp part) (Daniel Bevenius) [#12450](https://github.com/nodejs/node/pull/12450)
* [[`ea44b8b283`](https://github.com/nodejs/node/commit/ea44b8b283)] - **build**: disable -O3 for C++ coverage (Anna Henningsen) [#12406](https://github.com/nodejs/node/pull/12406)
* [[`baa2602539`](https://github.com/nodejs/node/commit/baa2602539)] - **build**: add test-gc-clean and test-gc PHONY rules (Joyee Cheung) [#12059](https://github.com/nodejs/node/pull/12059)
* [[`c694633328`](https://github.com/nodejs/node/commit/c694633328)] - **build**: sort phony rules (Joyee Cheung) [#12059](https://github.com/nodejs/node/pull/12059)
* [[`4dde87620a`](https://github.com/nodejs/node/commit/4dde87620a)] - **build**: don't test addons-napi twice (Gibson Fahnestock) [#12201](https://github.com/nodejs/node/pull/12201)
* [[`d19809a3c5`](https://github.com/nodejs/node/commit/d19809a3c5)] - **build**: avoid passing kill empty input in Makefile (Gibson Fahnestock) [#12158](https://github.com/nodejs/node/pull/12158)
* [[`c68da89694`](https://github.com/nodejs/node/commit/c68da89694)] - **build**: always use V8_ENABLE_CHECKS in debug mode (Anna Henningsen) [#12029](https://github.com/nodejs/node/pull/12029)
* [[`7cd6a7ddcb`](https://github.com/nodejs/node/commit/7cd6a7ddcb)] - **build**: notify about the redundancy of "nosign" (Nikolai Vavilov) [#11119](https://github.com/nodejs/node/pull/11119)
* [[`dd81d539e2`](https://github.com/nodejs/node/commit/dd81d539e2)] - **child_process**: fix deoptimizing use of arguments (Vse Mozhet Byt) [#11535](https://github.com/nodejs/node/pull/11535)
* [[`dc3bbb45a7`](https://github.com/nodejs/node/commit/dc3bbb45a7)] - **cluster**: remove debug arg handling (Rich Trott) [#12738](https://github.com/nodejs/node/pull/12738)
* [[`c969047d62`](https://github.com/nodejs/node/commit/c969047d62)] - **console**: fixup `console.dir()` error handling (Anna Henningsen) [#11443](https://github.com/nodejs/node/pull/11443)
* [[`9fa148909c`](https://github.com/nodejs/node/commit/9fa148909c)] - **crypto**: update root certificates (Ben Noordhuis) [#13279](https://github.com/nodejs/node/pull/13279)
* [[`945916195c`](https://github.com/nodejs/node/commit/945916195c)] - **crypto**: return CHECK_OK in VerifyCallback (Daniel Bevenius) [#13241](https://github.com/nodejs/node/pull/13241)
* [[`7b97f07340`](https://github.com/nodejs/node/commit/7b97f07340)] - **crypto**: remove root_cert_store from node_crypto.h (Daniel Bevenius) [#13194](https://github.com/nodejs/node/pull/13194)
* [[`f06f8365e4`](https://github.com/nodejs/node/commit/f06f8365e4)] - **crypto**: remove unnecessary template class (Daniel Bevenius) [#12993](https://github.com/nodejs/node/pull/12993)
* [[`6c2daf0ce9`](https://github.com/nodejs/node/commit/6c2daf0ce9)] - **crypto**: throw proper errors if out enc is UTF-16 (Anna Henningsen) [#12752](https://github.com/nodejs/node/pull/12752)
* [[`eaa0542eff`](https://github.com/nodejs/node/commit/eaa0542eff)] - **crypto**: remove unused C++ parameter in sign/verify (Tobias Nießen) [#12397](https://github.com/nodejs/node/pull/12397)
* [[`6083c4dc10`](https://github.com/nodejs/node/commit/6083c4dc10)] - **deps**: upgrade npm to 5.0.0 (Kat Marchán) [#13276](https://github.com/nodejs/node/pull/13276)
* [[`f204945495`](https://github.com/nodejs/node/commit/f204945495)] - **deps**: add example of comparing OpenSSL changes (Daniel Bevenius) [#13234](https://github.com/nodejs/node/pull/13234)
* [[`9302f512f8`](https://github.com/nodejs/node/commit/9302f512f8)] - **deps**: cherry-pick 6803eef from V8 upstream (Matt Loring) [#13175](https://github.com/nodejs/node/pull/13175)
* [[`2648c8de30`](https://github.com/nodejs/node/commit/2648c8de30)] - **deps**: backport 6d38f89d from upstream V8 (Ali Ijaz Sheikh) [#13162](https://github.com/nodejs/node/pull/13162)
* [[`6e1407c3b3`](https://github.com/nodejs/node/commit/6e1407c3b3)] - **deps**: backport 4fdf9fd4813 from upstream v8 (Jochen Eisinger) [#12875](https://github.com/nodejs/node/pull/12875)
* [[`385499ccf8`](https://github.com/nodejs/node/commit/385499ccf8)] - **deps**: backport 4acdb5eec2c from upstream v8 (jbroman) [#12875](https://github.com/nodejs/node/pull/12875)
* [[`69161b5f94`](https://github.com/nodejs/node/commit/69161b5f94)] - **deps**: backport 3700a01c82 from upstream v8 (jbroman) [#12875](https://github.com/nodejs/node/pull/12875)
* [[`9edd6d8ddb`](https://github.com/nodejs/node/commit/9edd6d8ddb)] - **deps**: backport 2cd2f5feff3 from upstream v8 (Jochen Eisinger) [#12875](https://github.com/nodejs/node/pull/12875)
* [[`19c0c07446`](https://github.com/nodejs/node/commit/19c0c07446)] - **deps**: backport de1461b7efd from upstream v8 (addaleax) [#12875](https://github.com/nodejs/node/pull/12875)
* [[`95c4b0d8f6`](https://github.com/nodejs/node/commit/95c4b0d8f6)] - **deps**: backport 78867ad8707a016 from v8 upstream (Michael Lippautz) [#12875](https://github.com/nodejs/node/pull/12875)
* [[`986e1d2c6f`](https://github.com/nodejs/node/commit/986e1d2c6f)] - **deps**: cherry-pick f5fad6d from upstream v8 (daniel.bevenius) [#12826](https://github.com/nodejs/node/pull/12826)
* [[`e896898dea`](https://github.com/nodejs/node/commit/e896898dea)] - **deps**: update openssl asm and asm_obsolete files (Shigeki Ohtsu) [#12913](https://github.com/nodejs/node/pull/12913)
* [[`f4390650e3`](https://github.com/nodejs/node/commit/f4390650e3)] - **deps**: cherry-pick 4ae5993 from upstream OpenSSL (Shigeki Ohtsu) [#12913](https://github.com/nodejs/node/pull/12913)
* [[`5d0a770c12`](https://github.com/nodejs/node/commit/5d0a770c12)] - **deps**: ICU 59.1 bump (Steven R. Loomis) [#12486](https://github.com/nodejs/node/pull/12486)
* [[`d74a545535`](https://github.com/nodejs/node/commit/d74a545535)] - **deps**: cherry-pick bfae9db from upstream v8 (Ben Noordhuis) [#12722](https://github.com/nodejs/node/pull/12722)
* [[`6ed791c665`](https://github.com/nodejs/node/commit/6ed791c665)] - **deps**: cherry-pick bfae9db from upstream v8 (Ben Noordhuis) [#12722](https://github.com/nodejs/node/pull/12722)
* [[`0084260448`](https://github.com/nodejs/node/commit/0084260448)] - **deps**: upgrade npm to 4.5.0 (Rebecca Turner) [#12480](https://github.com/nodejs/node/pull/12480)
* [[`021719738e`](https://github.com/nodejs/node/commit/021719738e)] - **deps**: update node-inspect to v1.11.2 (Jan Krems) [#12363](https://github.com/nodejs/node/pull/12363)
* [[`3471d6312d`](https://github.com/nodejs/node/commit/3471d6312d)] - **deps**: cherry-pick 0ba513f05 from V8 upstream (Franziska Hinkelmann) [#11712](https://github.com/nodejs/node/pull/11712)
* [[`dd8982dc74`](https://github.com/nodejs/node/commit/dd8982dc74)] - **deps**: cherry-pick 09de996 from V8 upstream (Franziska Hinkelmann) [#11905](https://github.com/nodejs/node/pull/11905)
* [[`a44aff4770`](https://github.com/nodejs/node/commit/a44aff4770)] - **deps**: cherry-pick 0ba513f05 from V8 upstream (Franziska Hinkelmann) [#11712](https://github.com/nodejs/node/pull/11712)
* [[`2b541471db`](https://github.com/nodejs/node/commit/2b541471db)] - **dns**: fix `resolve` failed starts without network (XadillaX) [#13076](https://github.com/nodejs/node/pull/13076)
* [[`5a948f6f64`](https://github.com/nodejs/node/commit/5a948f6f64)] - **dns**: fix crash using dns.setServers after resolve4 (XadillaX) [#13050](https://github.com/nodejs/node/pull/13050)
* [[`dd14ab608e`](https://github.com/nodejs/node/commit/dd14ab608e)] - **doc**: create list of CTC emeriti (Rich Trott) [#13232](https://github.com/nodejs/node/pull/13232)
* [[`40572c5def`](https://github.com/nodejs/node/commit/40572c5def)] - **doc**: remove Gitter badge from README (Rich Trott) [#13231](https://github.com/nodejs/node/pull/13231)
* [[`686dd36930`](https://github.com/nodejs/node/commit/686dd36930)] - **doc**: fix api docs style (Daijiro Wachi) [#13236](https://github.com/nodejs/node/pull/13236)
* [[`0be029ec97`](https://github.com/nodejs/node/commit/0be029ec97)] - **doc**: make spelling of behavior consistent (Michael Dawson) [#13245](https://github.com/nodejs/node/pull/13245)
* [[`c0a7c8e0d2`](https://github.com/nodejs/node/commit/c0a7c8e0d2)] - **doc**: fix code example in inspector.md (Vse Mozhet Byt) [#13182](https://github.com/nodejs/node/pull/13182)
* [[`cd2824cc93`](https://github.com/nodejs/node/commit/cd2824cc93)] - **doc**: make the style of notes consistent (Alexey Orlenko) [#13133](https://github.com/nodejs/node/pull/13133)
* [[`d4e9e0f7e4`](https://github.com/nodejs/node/commit/d4e9e0f7e4)] - **doc**: add jasongin & kunalspathak to collaborators (Jason Ginchereau) [#13200](https://github.com/nodejs/node/pull/13200)
* [[`db90b505e8`](https://github.com/nodejs/node/commit/db90b505e8)] - **doc**: don't use useless constructors in stream.md (Vse Mozhet Byt) [#13145](https://github.com/nodejs/node/pull/13145)
* [[`2c45e6fd68`](https://github.com/nodejs/node/commit/2c45e6fd68)] - **doc**: update code example for Windows in stream.md (Vse Mozhet Byt) [#13138](https://github.com/nodejs/node/pull/13138)
* [[`3c91145f31`](https://github.com/nodejs/node/commit/3c91145f31)] - **doc**: improve formatting of STYLE_GUIDE.md (Alexey Orlenko) [#13135](https://github.com/nodejs/node/pull/13135)
* [[`1d587ef982`](https://github.com/nodejs/node/commit/1d587ef982)] - **doc**: fix incorrect keyboard shortcut (Alexey Orlenko) [#13134](https://github.com/nodejs/node/pull/13134)
* [[`336d33b646`](https://github.com/nodejs/node/commit/336d33b646)] - **doc**: fix title/function name mismatch (Michael Dawson) [#13123](https://github.com/nodejs/node/pull/13123)
* [[`9f01b34bf9`](https://github.com/nodejs/node/commit/9f01b34bf9)] - **doc**: link to `common` docs in test writing guide (Anna Henningsen) [#13118](https://github.com/nodejs/node/pull/13118)
* [[`1aa68f9a8d`](https://github.com/nodejs/node/commit/1aa68f9a8d)] - **doc**: list macOS as supporting filename argument (Chris Young) [#13111](https://github.com/nodejs/node/pull/13111)
* [[`ef71824740`](https://github.com/nodejs/node/commit/ef71824740)] - **doc**: edit Error.captureStackTrace html comment (Artur Vieira) [#12962](https://github.com/nodejs/node/pull/12962)
* [[`bfade5aacd`](https://github.com/nodejs/node/commit/bfade5aacd)] - **doc**: remove unused/duplicated reference links (Daijiro Wachi) [#13066](https://github.com/nodejs/node/pull/13066)
* [[`4a7b7e8097`](https://github.com/nodejs/node/commit/4a7b7e8097)] - **doc**: add reference to node_api.h in docs (Michael Dawson) [#13084](https://github.com/nodejs/node/pull/13084)
* [[`3702ae732e`](https://github.com/nodejs/node/commit/3702ae732e)] - **doc**: add additional useful ci job to list (Michael Dawson) [#13086](https://github.com/nodejs/node/pull/13086)
* [[`847688018c`](https://github.com/nodejs/node/commit/847688018c)] - **doc**: don't suggest setEncoding for binary streams (Rick Bullotta) [#11363](https://github.com/nodejs/node/pull/11363)
* [[`eff9252181`](https://github.com/nodejs/node/commit/eff9252181)] - **doc**: update doc of publicEncrypt method (Faiz Halde) [#12947](https://github.com/nodejs/node/pull/12947)
* [[`ab34f9dec2`](https://github.com/nodejs/node/commit/ab34f9dec2)] - **doc**: update doc to remove usage of "you" (Michael Dawson) [#13067](https://github.com/nodejs/node/pull/13067)
* [[`5de722ab6d`](https://github.com/nodejs/node/commit/5de722ab6d)] - **doc**: fix links from ToC to subsection for 4.8.x (Frank Lanitz) [#13039](https://github.com/nodejs/node/pull/13039)
* [[`92f3b301ab`](https://github.com/nodejs/node/commit/92f3b301ab)] - **doc**: document method for reverting commits (Gibson Fahnestock) [#13015](https://github.com/nodejs/node/pull/13015)
* [[`1b28022de0`](https://github.com/nodejs/node/commit/1b28022de0)] - **doc**: clarify operation of napi_cancel_async_work (Michael Dawson) [#12974](https://github.com/nodejs/node/pull/12974)
* [[`1d5f5aa7e1`](https://github.com/nodejs/node/commit/1d5f5aa7e1)] - **doc**: update COLLABORATOR_GUIDE.md (morrme) [#12555](https://github.com/nodejs/node/pull/12555)
* [[`d7d16f7b8b`](https://github.com/nodejs/node/commit/d7d16f7b8b)] - **doc**: Change options at STEP 5 in CONTRIBUTING.md (kysnm) [#12830](https://github.com/nodejs/node/pull/12830)
* [[`c79deaab82`](https://github.com/nodejs/node/commit/c79deaab82)] - **doc**: update to add ref to supported platforms (Michael Dawson) [#12931](https://github.com/nodejs/node/pull/12931)
* [[`abfd4bf9df`](https://github.com/nodejs/node/commit/abfd4bf9df)] - **doc**: clarify node.js addons are c++ (Beth Griggs) [#12898](https://github.com/nodejs/node/pull/12898)
* [[`13487c437c`](https://github.com/nodejs/node/commit/13487c437c)] - **doc**: add docs for server.address() for pipe case (Flarna) [#12907](https://github.com/nodejs/node/pull/12907)
* [[`147048a0d3`](https://github.com/nodejs/node/commit/147048a0d3)] - **doc**: fix broken links in n-api doc (Michael Dawson) [#12889](https://github.com/nodejs/node/pull/12889)
* [[`e429f9a42a`](https://github.com/nodejs/node/commit/e429f9a42a)] - **doc**: fix typo in streams.md (Glenn Schlereth) [#12924](https://github.com/nodejs/node/pull/12924)
* [[`ea1b8a5cbc`](https://github.com/nodejs/node/commit/ea1b8a5cbc)] - **doc**: sort bottom-of-file markdown links (Sam Roberts) [#12726](https://github.com/nodejs/node/pull/12726)
* [[`cbd6fde9a3`](https://github.com/nodejs/node/commit/cbd6fde9a3)] - **doc**: improve path.posix.normalize docs (Steven Lehn) [#12700](https://github.com/nodejs/node/pull/12700)
* [[`a398516b4f`](https://github.com/nodejs/node/commit/a398516b4f)] - **doc**: remove test-npm from general build doc (Rich Trott) [#12840](https://github.com/nodejs/node/pull/12840)
* [[`4703824276`](https://github.com/nodejs/node/commit/4703824276)] - **doc**: fix commit guideline url (Thomas Watson) [#12862](https://github.com/nodejs/node/pull/12862)
* [[`2614d247fb`](https://github.com/nodejs/node/commit/2614d247fb)] - **doc**: update readFileSync in fs.md (Aditya Anand) [#12800](https://github.com/nodejs/node/pull/12800)
* [[`0258aed9d2`](https://github.com/nodejs/node/commit/0258aed9d2)] - **doc**: edit CONTRIBUTING.md for clarity etc. (Rich Trott) [#12796](https://github.com/nodejs/node/pull/12796)
* [[`c1b3b95939`](https://github.com/nodejs/node/commit/c1b3b95939)] - **doc**: add WHATWG file URLs in fs module (Olivier Martin) [#12670](https://github.com/nodejs/node/pull/12670)
* [[`2bf461e6f5`](https://github.com/nodejs/node/commit/2bf461e6f5)] - **doc**: document vm timeout option perf impact (Anna Henningsen) [#12751](https://github.com/nodejs/node/pull/12751)
* [[`d8f8096ec6`](https://github.com/nodejs/node/commit/d8f8096ec6)] - **doc**: update example in repl.md (Vse Mozhet Byt) [#12685](https://github.com/nodejs/node/pull/12685)
* [[`deb9622b11`](https://github.com/nodejs/node/commit/deb9622b11)] - **doc**: Add initial documentation for N-API (Michael Dawson) [#12549](https://github.com/nodejs/node/pull/12549)
* [[`71911be1de`](https://github.com/nodejs/node/commit/71911be1de)] - **doc**: clarify arch support for power platforms (Michael Dawson) [#12679](https://github.com/nodejs/node/pull/12679)
* [[`71f22c842b`](https://github.com/nodejs/node/commit/71f22c842b)] - **doc**: replace uses of `you` and other style nits (James M Snell) [#12673](https://github.com/nodejs/node/pull/12673)
* [[`35d2137715`](https://github.com/nodejs/node/commit/35d2137715)] - **doc**: modernize and fix code examples in repl.md (Vse Mozhet Byt) [#12634](https://github.com/nodejs/node/pull/12634)
* [[`6ee6aaefa1`](https://github.com/nodejs/node/commit/6ee6aaefa1)] - **doc**: add no-var, prefer-const in doc eslintrc (Vse Mozhet Byt) [#12563](https://github.com/nodejs/node/pull/12563)
* [[`b4fea2a3d6`](https://github.com/nodejs/node/commit/b4fea2a3d6)] - **doc**: add eslint-plugin-markdown (Vse Mozhet Byt) [#12563](https://github.com/nodejs/node/pull/12563)
* [[`e2c3e4727d`](https://github.com/nodejs/node/commit/e2c3e4727d)] - **doc**: conform to rules for eslint-plugin-markdown (Vse Mozhet Byt) [#12563](https://github.com/nodejs/node/pull/12563)
* [[`211813c99c`](https://github.com/nodejs/node/commit/211813c99c)] - **doc**: unify quotes in an assert.md code example (Vse Mozhet Byt) [#12505](https://github.com/nodejs/node/pull/12505)
* [[`b4f59a7460`](https://github.com/nodejs/node/commit/b4f59a7460)] - **doc**: upgrade Clang requirement to 3.4.2 (Michaël Zasso) [#12388](https://github.com/nodejs/node/pull/12388)
* [[`b837bd2792`](https://github.com/nodejs/node/commit/b837bd2792)] - **doc**: fix typo in CHANGELOG.md (Gautam krishna.R) [#12434](https://github.com/nodejs/node/pull/12434)
* [[`fe1be39b28`](https://github.com/nodejs/node/commit/fe1be39b28)] - **doc**: child_process example for special chars (Cody Deckard)
* [[`e72ea0da0b`](https://github.com/nodejs/node/commit/e72ea0da0b)] - **doc**: modernize and fix code examples in process.md (Vse Mozhet Byt) [#12381](https://github.com/nodejs/node/pull/12381)
* [[`c6e0ba31ec`](https://github.com/nodejs/node/commit/c6e0ba31ec)] - **doc**: update OS level support for AIX (Michael Dawson) [#12235](https://github.com/nodejs/node/pull/12235)
* [[`6ebc806a47`](https://github.com/nodejs/node/commit/6ebc806a47)] - **doc**: correct markdown file line lengths (JR McEntee) [#12106](https://github.com/nodejs/node/pull/12106)
* [[`7a5d07c7fb`](https://github.com/nodejs/node/commit/7a5d07c7fb)] - **doc**: change Mac OS X to macOS (JR McEntee) [#12106](https://github.com/nodejs/node/pull/12106)
* [[`c79b081367`](https://github.com/nodejs/node/commit/c79b081367)] - **doc**: fix typo in CHANGELOG_V6.md (Zero King) [#12206](https://github.com/nodejs/node/pull/12206)
* [[`ba0e3ac53d`](https://github.com/nodejs/node/commit/ba0e3ac53d)] - **doc**: minor improvements in BUILDING.md (Sakthipriyan Vairamani (thefourtheye)) [#11963](https://github.com/nodejs/node/pull/11963)
* [[`e40ac30e05`](https://github.com/nodejs/node/commit/e40ac30e05)] - **doc**: document extent of crypto Uint8Array support (Anna Henningsen) [#11982](https://github.com/nodejs/node/pull/11982)
* [[`ef4768754c`](https://github.com/nodejs/node/commit/ef4768754c)] - **doc**: add supported platforms list (Michael Dawson) [#11872](https://github.com/nodejs/node/pull/11872)
* [[`73e2d0bce6`](https://github.com/nodejs/node/commit/73e2d0bce6)] - **doc**: argument types for crypto methods (Amelia Clarke) [#11799](https://github.com/nodejs/node/pull/11799)
* [[`df97727272`](https://github.com/nodejs/node/commit/df97727272)] - **doc**: improve net.md on sockets and connections (Joyee Cheung) [#11700](https://github.com/nodejs/node/pull/11700)
* [[`3b05153cdc`](https://github.com/nodejs/node/commit/3b05153cdc)] - **doc**: various improvements to net.md (Joyee Cheung) [#11636](https://github.com/nodejs/node/pull/11636)
* [[`289e53265a`](https://github.com/nodejs/node/commit/289e53265a)] - **doc**: add missing entry in v6 changelog table (Luigi Pinca) [#11534](https://github.com/nodejs/node/pull/11534)
* [[`5da952472b`](https://github.com/nodejs/node/commit/5da952472b)] - **doc**: document pending semver-major API changes (Anna Henningsen) [#11489](https://github.com/nodejs/node/pull/11489)
* [[`52b253677a`](https://github.com/nodejs/node/commit/52b253677a)] - **doc**: fix sorting in API references (Vse Mozhet Byt) [#11331](https://github.com/nodejs/node/pull/11331)
* [[`ca8c30a35c`](https://github.com/nodejs/node/commit/ca8c30a35c)] - **doc**: update output examples in debugger.md (Vse Mozhet Byt) [#10944](https://github.com/nodejs/node/pull/10944)
* [[`c5a0dcedd3`](https://github.com/nodejs/node/commit/c5a0dcedd3)] - **doc**: fix math error in process.md (Diego Rodríguez Baquero) [#11158](https://github.com/nodejs/node/pull/11158)
* [[`93c4820458`](https://github.com/nodejs/node/commit/93c4820458)] - ***Revert*** "**doc**: correct vcbuild options for windows testing" (Gibson Fahnestock) [#10839](https://github.com/nodejs/node/pull/10839)
* [[`6d31bdb872`](https://github.com/nodejs/node/commit/6d31bdb872)] - **doc**: move Boron releases to LTS column (Anna Henningsen) [#10827](https://github.com/nodejs/node/pull/10827)
* [[`f3f2468bdc`](https://github.com/nodejs/node/commit/f3f2468bdc)] - **doc**: fix CHANGELOG.md table formatting (Сковорода Никита Андреевич) [#10743](https://github.com/nodejs/node/pull/10743)
* [[`65e7f62400`](https://github.com/nodejs/node/commit/65e7f62400)] - **doc**: fix heading type for v4.6.2 changelog (Myles Borins) [#9515](https://github.com/nodejs/node/pull/9515)
* [[`e1cabf6fbd`](https://github.com/nodejs/node/commit/e1cabf6fbd)] - **doc, test**: add note to response.getHeaders (Refael Ackermann) [#12887](https://github.com/nodejs/node/pull/12887)
* [[`42dca99cd7`](https://github.com/nodejs/node/commit/42dca99cd7)] - **doc, tools**: add doc linting to CI (Vse Mozhet Byt) [#12640](https://github.com/nodejs/node/pull/12640)
* [[`81b9b857aa`](https://github.com/nodejs/node/commit/81b9b857aa)] - **doc,build**: update configure help messages (Gibson Fahnestock) [#12978](https://github.com/nodejs/node/pull/12978)
* [[`50af2b95e0`](https://github.com/nodejs/node/commit/50af2b95e0)] - **errors**: AssertionError moved to internal/error (Faiz Halde) [#12906](https://github.com/nodejs/node/pull/12906)
* [[`7b4a72d797`](https://github.com/nodejs/node/commit/7b4a72d797)] - **errors**: add space between error name and code (James M Snell) [#12099](https://github.com/nodejs/node/pull/12099)
* [[`58066d16d5`](https://github.com/nodejs/node/commit/58066d16d5)] - **events**: remove unreachable code (cjihrig) [#12501](https://github.com/nodejs/node/pull/12501)
* [[`ea9eed5643`](https://github.com/nodejs/node/commit/ea9eed5643)] - **freelist**: simplify export (James M Snell) [#12644](https://github.com/nodejs/node/pull/12644)
* [[`d99b7bc8c9`](https://github.com/nodejs/node/commit/d99b7bc8c9)] - **fs**: fix realpath{Sync} on resolving pipes/sockets (Ebrahim Byagowi) [#13028](https://github.com/nodejs/node/pull/13028)
* [[`6f449db60f`](https://github.com/nodejs/node/commit/6f449db60f)] - **fs**: refactor deprecated functions for readability (Rich Trott) [#12910](https://github.com/nodejs/node/pull/12910)
* [[`08809f28ad`](https://github.com/nodejs/node/commit/08809f28ad)] - **fs**: simplify constant decls (James M Snell) [#12644](https://github.com/nodejs/node/pull/12644)
* [[`2264d9d4ba`](https://github.com/nodejs/node/commit/2264d9d4ba)] - **http**: improve outgoing string write performance (Brian White) [#13013](https://github.com/nodejs/node/pull/13013)
* [[`414f93ecb3`](https://github.com/nodejs/node/commit/414f93ecb3)] - **http**: fix IPv6 Host header check (Brian White) [#13122](https://github.com/nodejs/node/pull/13122)
* [[`55c95b1644`](https://github.com/nodejs/node/commit/55c95b1644)] - **http**: fix first body chunk fast case for UTF-16 (Anna Henningsen) [#12747](https://github.com/nodejs/node/pull/12747)
* [[`e283319969`](https://github.com/nodejs/node/commit/e283319969)] - **http**: fix permanent deoptimizations (Brian White) [#12456](https://github.com/nodejs/node/pull/12456)
* [[`e0a9ad1af2`](https://github.com/nodejs/node/commit/e0a9ad1af2)] - **http**: avoid retaining unneeded memory (Luigi Pinca) [#11926](https://github.com/nodejs/node/pull/11926)
* [[`74c1e02642`](https://github.com/nodejs/node/commit/74c1e02642)] - **http**: replace uses of self (James M Snell) [#11594](https://github.com/nodejs/node/pull/11594)
* [[`5425e0dcbe`](https://github.com/nodejs/node/commit/5425e0dcbe)] - **http**: use more efficient module.exports pattern (James M Snell) [#11594](https://github.com/nodejs/node/pull/11594)
* [[`69f3db4571`](https://github.com/nodejs/node/commit/69f3db4571)] - **http,https**: avoid instanceof for WHATWG URL (Brian White) [#12983](https://github.com/nodejs/node/pull/12983)
* [[`9ce2271e81`](https://github.com/nodejs/node/commit/9ce2271e81)] - **https**: support agent construction without new (cjihrig) [#12927](https://github.com/nodejs/node/pull/12927)
* [[`010f864426`](https://github.com/nodejs/node/commit/010f864426)] - **inspector**: --debug* deprecation and invalidation (Refael Ackermann) [#12949](https://github.com/nodejs/node/pull/12949)
* [[`bb77cce7a1`](https://github.com/nodejs/node/commit/bb77cce7a1)] - **inspector**: add missing virtual destructor (Eugene Ostroukhov) [#13198](https://github.com/nodejs/node/pull/13198)
* [[`39785c7780`](https://github.com/nodejs/node/commit/39785c7780)] - **inspector**: document bad usage for --inspect-port (Sam Roberts) [#12581](https://github.com/nodejs/node/pull/12581)
* [[`77d5e6f8da`](https://github.com/nodejs/node/commit/77d5e6f8da)] - **inspector**: fix process._debugEnd() for inspector (Eugene Ostroukhov) [#12777](https://github.com/nodejs/node/pull/12777)
* [[`7c3a23b9c1`](https://github.com/nodejs/node/commit/7c3a23b9c1)] - **inspector**: handle socket close before close frame (Eugene Ostroukhov) [#12937](https://github.com/nodejs/node/pull/12937)
* [[`15e160e626`](https://github.com/nodejs/node/commit/15e160e626)] - **inspector**: report when main context is destroyed (Eugene Ostroukhov) [#12814](https://github.com/nodejs/node/pull/12814)
* [[`3f48ab3042`](https://github.com/nodejs/node/commit/3f48ab3042)] - **inspector**: do not add 'inspector' property (Eugene Ostroukhov) [#12656](https://github.com/nodejs/node/pull/12656)
* [[`439b35aade`](https://github.com/nodejs/node/commit/439b35aade)] - **inspector**: remove AgentImpl (Eugene Ostroukhov) [#12576](https://github.com/nodejs/node/pull/12576)
* [[`42be835e05`](https://github.com/nodejs/node/commit/42be835e05)] - **inspector**: fix Coverity defects (Eugene Ostroukhov) [#12272](https://github.com/nodejs/node/pull/12272)
* [[`7954d2a4c7`](https://github.com/nodejs/node/commit/7954d2a4c7)] - **inspector**: use inspector API for "break on start" (Eugene Ostroukhov) [#12076](https://github.com/nodejs/node/pull/12076)
* [[`b170fb7c55`](https://github.com/nodejs/node/commit/b170fb7c55)] - **inspector**: proper WS URLs when bound to 0.0.0.0 (Eugene Ostroukhov) [#11755](https://github.com/nodejs/node/pull/11755)
* [[`54d331895c`](https://github.com/nodejs/node/commit/54d331895c)] - **lib**: add guard to originalConsole (Daniel Bevenius) [#12881](https://github.com/nodejs/node/pull/12881)
* [[`824fb49a70`](https://github.com/nodejs/node/commit/824fb49a70)] - **lib**: remove useless default caught (Jackson Tian) [#12884](https://github.com/nodejs/node/pull/12884)
* [[`9077b48271`](https://github.com/nodejs/node/commit/9077b48271)] - **lib**: refactor internal/util (James M Snell) [#11404](https://github.com/nodejs/node/pull/11404)
* [[`cfc8422a68`](https://github.com/nodejs/node/commit/cfc8422a68)] - **lib**: use Object.create(null) directly (Timothy Gu) [#11930](https://github.com/nodejs/node/pull/11930)
* [[`4eb194a2b1`](https://github.com/nodejs/node/commit/4eb194a2b1)] - **lib**: Use regex to compare error message (Kunal Pathak) [#11854](https://github.com/nodejs/node/pull/11854)
* [[`989713d343`](https://github.com/nodejs/node/commit/989713d343)] - **lib**: avoid using forEach (James M Snell) [#11582](https://github.com/nodejs/node/pull/11582)
* [[`4d090855c6`](https://github.com/nodejs/node/commit/4d090855c6)] - **lib**: avoid using forEach in LazyTransform (James M Snell) [#11582](https://github.com/nodejs/node/pull/11582)
* [[`3becb0206c`](https://github.com/nodejs/node/commit/3becb0206c)] - **lib,src**: improve writev() performance for Buffers (Brian White) [#13187](https://github.com/nodejs/node/pull/13187)
* [[`6bcf65d4a7`](https://github.com/nodejs/node/commit/6bcf65d4a7)] - **lib,test**: use regular expression literals (Rich Trott) [#12807](https://github.com/nodejs/node/pull/12807)
* [[`dd0624676c`](https://github.com/nodejs/node/commit/dd0624676c)] - **meta**: fix nits in README.md collaborators list (Vse Mozhet Byt) [#12866](https://github.com/nodejs/node/pull/12866)
* [[`98e54b0bd4`](https://github.com/nodejs/node/commit/98e54b0bd4)] - **meta**: restore original copyright header (James M Snell) [#10155](https://github.com/nodejs/node/pull/10155)
* [[`ed0716f0e9`](https://github.com/nodejs/node/commit/ed0716f0e9)] - **module**: refactor internal/module export style (James M Snell) [#12644](https://github.com/nodejs/node/pull/12644)
* [[`f97156623a`](https://github.com/nodejs/node/commit/f97156623a)] - **module**: standardize strip shebang behaviour (Sebastian Van Sande) [#12202](https://github.com/nodejs/node/pull/12202)
* [[`a63b245b0a`](https://github.com/nodejs/node/commit/a63b245b0a)] - **n-api**: Retain last code when getting error info (Jason Ginchereau) [#13087](https://github.com/nodejs/node/pull/13087)
* [[`008301167e`](https://github.com/nodejs/node/commit/008301167e)] - **n-api**: remove compiler warning (Anna Henningsen) [#13014](https://github.com/nodejs/node/pull/13014)
* [[`2e3fef7628`](https://github.com/nodejs/node/commit/2e3fef7628)] - **n-api**: Handle fatal exception in async callback (Jason Ginchereau) [#12838](https://github.com/nodejs/node/pull/12838)
* [[`2bbabb1f85`](https://github.com/nodejs/node/commit/2bbabb1f85)] - **n-api**: napi_get_cb_info should fill array (Jason Ginchereau) [#12863](https://github.com/nodejs/node/pull/12863)
* [[`cd32b77567`](https://github.com/nodejs/node/commit/cd32b77567)] - **n-api**: remove unnecessary try-catch bracket from certain APIs (Gabriel Schulhof) [#12705](https://github.com/nodejs/node/pull/12705)
* [[`972bfe1514`](https://github.com/nodejs/node/commit/972bfe1514)] - **n-api**: Sync with back-compat changes (Jason Ginchereau) [#12674](https://github.com/nodejs/node/pull/12674)
* [[`427125491f`](https://github.com/nodejs/node/commit/427125491f)] - **n-api**: Reference and external tests (Jason Ginchereau) [#12551](https://github.com/nodejs/node/pull/12551)
* [[`b7a341d7e5`](https://github.com/nodejs/node/commit/b7a341d7e5)] - **n-api**: Enable scope and ref APIs during exception (Jason Ginchereau) [#12524](https://github.com/nodejs/node/pull/12524)
* [[`ba7bac5c37`](https://github.com/nodejs/node/commit/ba7bac5c37)] - **n-api**: tighten null-checking and clean up last error (Gabriel Schulhof) [#12539](https://github.com/nodejs/node/pull/12539)
* [[`468275ac79`](https://github.com/nodejs/node/commit/468275ac79)] - **n-api**: remove napi_get_value_string_length() (Jason Ginchereau) [#12496](https://github.com/nodejs/node/pull/12496)
* [[`46f202690b`](https://github.com/nodejs/node/commit/46f202690b)] - **n-api**: fix coverity scan report (Michael Dawson) [#12365](https://github.com/nodejs/node/pull/12365)
* [[`ad5f987558`](https://github.com/nodejs/node/commit/ad5f987558)] - **n-api**: add string api for latin1 encoding (Sampson Gao) [#12368](https://github.com/nodejs/node/pull/12368)
* [[`affe0f2d2a`](https://github.com/nodejs/node/commit/affe0f2d2a)] - **n-api**: fix -Wmismatched-tags compiler warning (Ben Noordhuis) [#12333](https://github.com/nodejs/node/pull/12333)
* [[`9decfb1521`](https://github.com/nodejs/node/commit/9decfb1521)] - **n-api**: implement async helper methods (taylor.woll) [#12250](https://github.com/nodejs/node/pull/12250)
* [[`ca786c3734`](https://github.com/nodejs/node/commit/ca786c3734)] - **n-api**: change napi_callback to return napi_value (Taylor Woll) [#12248](https://github.com/nodejs/node/pull/12248)
* [[`8fbace163a`](https://github.com/nodejs/node/commit/8fbace163a)] - **n-api**: cache Symbol.hasInstance (Gabriel Schulhof) [#12246](https://github.com/nodejs/node/pull/12246)
* [[`84602845c6`](https://github.com/nodejs/node/commit/84602845c6)] - **n-api**: Update property attrs enum to match JS spec (Jason Ginchereau) [#12240](https://github.com/nodejs/node/pull/12240)
* [[`0a5bf4aee3`](https://github.com/nodejs/node/commit/0a5bf4aee3)] - **n-api**: create napi_env as a real structure (Gabriel Schulhof) [#12195](https://github.com/nodejs/node/pull/12195)
* [[`4a21e398d6`](https://github.com/nodejs/node/commit/4a21e398d6)] - **n-api**: break dep between v8 and napi attributes (Michael Dawson) [#12191](https://github.com/nodejs/node/pull/12191)
* [[`afd5966fa9`](https://github.com/nodejs/node/commit/afd5966fa9)] - **napi**: initialize and check status properly (Gabriel Schulhof) [#12283](https://github.com/nodejs/node/pull/12283)
* [[`491d59da84`](https://github.com/nodejs/node/commit/491d59da84)] - **napi**: supress invalid coverity leak message (Michael Dawson) [#12192](https://github.com/nodejs/node/pull/12192)
* [[`4fabcfc5a2`](https://github.com/nodejs/node/commit/4fabcfc5a2)] - ***Revert*** "**net**: remove unnecessary process.nextTick()" (Evan Lucas) [#12854](https://github.com/nodejs/node/pull/12854)
* [[`51664fc265`](https://github.com/nodejs/node/commit/51664fc265)] - **net**: add symbol to normalized connect() args (cjihrig) [#13069](https://github.com/nodejs/node/pull/13069)
* [[`212a7a609d`](https://github.com/nodejs/node/commit/212a7a609d)] - **net**: ensure net.connect calls Socket connect (Thomas Watson) [#12861](https://github.com/nodejs/node/pull/12861)
* [[`879d6663ea`](https://github.com/nodejs/node/commit/879d6663ea)] - **net**: remove an unused internal module `assertPort` (Daijiro Wachi) [#11812](https://github.com/nodejs/node/pull/11812)
* [[`896be833c8`](https://github.com/nodejs/node/commit/896be833c8)] - **node**: add missing option to --help output (Ruslan Bekenev) [#12763](https://github.com/nodejs/node/pull/12763)
* [[`579ff2a487`](https://github.com/nodejs/node/commit/579ff2a487)] - **process**: refactor internal/process.js export style (James M Snell) [#12644](https://github.com/nodejs/node/pull/12644)
* [[`776028c46b`](https://github.com/nodejs/node/commit/776028c46b)] - **querystring**: improve unescapeBuffer() performance (Jesus Seijas) [#12525](https://github.com/nodejs/node/pull/12525)
* [[`c98a8022b7`](https://github.com/nodejs/node/commit/c98a8022b7)] - **querystring**: move isHexTable to internal (Timothy Gu) [#11858](https://github.com/nodejs/node/pull/11858)
* [[`ff785fd517`](https://github.com/nodejs/node/commit/ff785fd517)] - **querystring**: fix empty pairs and optimize parse() (Brian White) [#11234](https://github.com/nodejs/node/pull/11234)
* [[`4c070d4897`](https://github.com/nodejs/node/commit/4c070d4897)] - **readline**: move escape codes into internal/readline (James M Snell) [#12755](https://github.com/nodejs/node/pull/12755)
* [[`4ac7a68ccd`](https://github.com/nodejs/node/commit/4ac7a68ccd)] - **readline**: multiple code cleanups (James M Snell) [#12755](https://github.com/nodejs/node/pull/12755)
* [[`392a8987c6`](https://github.com/nodejs/node/commit/392a8987c6)] - **readline**: use module.exports = {} on internal/readline (James M Snell) [#12755](https://github.com/nodejs/node/pull/12755)
* [[`9318f82937`](https://github.com/nodejs/node/commit/9318f82937)] - **readline**: use module.exports = {} (James M Snell) [#12755](https://github.com/nodejs/node/pull/12755)
* [[`c20e87a10e`](https://github.com/nodejs/node/commit/c20e87a10e)] - **repl**: fix /dev/null history file regression (Brian White) [#12762](https://github.com/nodejs/node/pull/12762)
* [[`b45abfda5f`](https://github.com/nodejs/node/commit/b45abfda5f)] - **repl**: fix permanent deoptimizations (Brian White) [#12456](https://github.com/nodejs/node/pull/12456)
* [[`c7b60165a6`](https://github.com/nodejs/node/commit/c7b60165a6)] - **repl**: Empty command should be sent to eval function (Jan Krems) [#11871](https://github.com/nodejs/node/pull/11871)
* [[`ac2e8820c4`](https://github.com/nodejs/node/commit/ac2e8820c4)] - **src**: reduce duplicate code in SafeGetenv() (cjihrig) [#13220](https://github.com/nodejs/node/pull/13220)
* [[`ec7cbaf266`](https://github.com/nodejs/node/commit/ec7cbaf266)] - **src**: update NODE_MODULE_VERSION to 57 (Michaël Zasso) [#12995](https://github.com/nodejs/node/pull/12995)
* [[`9d922c6c0e`](https://github.com/nodejs/node/commit/9d922c6c0e)] - **src**: fix InspectorStarted macro guard (Daniel Bevenius) [#13167](https://github.com/nodejs/node/pull/13167)
* [[`e7d098cea6`](https://github.com/nodejs/node/commit/e7d098cea6)] - **src**: ignore unused warning for inspector-agent.cc (Daniel Bevenius) [#13188](https://github.com/nodejs/node/pull/13188)
* [[`145ab052df`](https://github.com/nodejs/node/commit/145ab052df)] - **src**: add comment for TicketKeyCallback (Anna Henningsen) [#13193](https://github.com/nodejs/node/pull/13193)
* [[`b4f6ea06eb`](https://github.com/nodejs/node/commit/b4f6ea06eb)] - **src**: make StreamBase::GetAsyncWrap pure virtual (Anna Henningsen) [#13174](https://github.com/nodejs/node/pull/13174)
* [[`4fa2ee16bb`](https://github.com/nodejs/node/commit/4fa2ee16bb)] - **src**: add linux getauxval(AT_SECURE) in SafeGetenv (Daniel Bevenius) [#12548](https://github.com/nodejs/node/pull/12548)
* [[`287b11dc8c`](https://github.com/nodejs/node/commit/287b11dc8c)] - **src**: allow --tls-cipher-list in NODE_OPTIONS (Sam Roberts) [#13172](https://github.com/nodejs/node/pull/13172)
* [[`3ccef8e267`](https://github.com/nodejs/node/commit/3ccef8e267)] - **src**: correct endif comment SRC_NODE_API_H__ (Daniel Bevenius) [#13190](https://github.com/nodejs/node/pull/13190)
* [[`4cbdac3183`](https://github.com/nodejs/node/commit/4cbdac3183)] - **src**: redirect-warnings to file, not path (Sam Roberts) [#13120](https://github.com/nodejs/node/pull/13120)
* [[`85e2d56df1`](https://github.com/nodejs/node/commit/85e2d56df1)] - **src**: fix typo (Brian White) [#13085](https://github.com/nodejs/node/pull/13085)
* [[`1263b70e9e`](https://github.com/nodejs/node/commit/1263b70e9e)] - **src**: remove unused parameters (Brian White) [#13085](https://github.com/nodejs/node/pull/13085)
* [[`1acd4d2cc4`](https://github.com/nodejs/node/commit/1acd4d2cc4)] - **src**: assert that uv_async_init() succeeds (cjihrig) [#13116](https://github.com/nodejs/node/pull/13116)
* [[`f81281737c`](https://github.com/nodejs/node/commit/f81281737c)] - **src**: remove unnecessary forward declaration (Daniel Bevenius) [#13081](https://github.com/nodejs/node/pull/13081)
* [[`60132e83c3`](https://github.com/nodejs/node/commit/60132e83c3)] - **src**: check IsConstructCall in TLSWrap constructor (Daniel Bevenius) [#13097](https://github.com/nodejs/node/pull/13097)
* [[`57b9b9d7d6`](https://github.com/nodejs/node/commit/57b9b9d7d6)] - **src**: remove unnecessary return statement (Daniel Bevenius) [#13094](https://github.com/nodejs/node/pull/13094)
* [[`94eca79d5d`](https://github.com/nodejs/node/commit/94eca79d5d)] - **src**: remove unused node_buffer.h include (Daniel Bevenius) [#13095](https://github.com/nodejs/node/pull/13095)
* [[`46e773c5db`](https://github.com/nodejs/node/commit/46e773c5db)] - **src**: check if --icu-data-dir= points to valid dir (Ben Noordhuis)
* [[`29d89c9855`](https://github.com/nodejs/node/commit/29d89c9855)] - **src**: split CryptoPemCallback into two functions (Daniel Bevenius) [#12827](https://github.com/nodejs/node/pull/12827)
* [[`d6cd466a25`](https://github.com/nodejs/node/commit/d6cd466a25)] - **src**: whitelist new options for NODE_OPTIONS (Sam Roberts) [#13002](https://github.com/nodejs/node/pull/13002)
* [[`53dae83833`](https://github.com/nodejs/node/commit/53dae83833)] - **src**: fix --abort_on_uncaught_exception arg parsing (Sam Roberts) [#13004](https://github.com/nodejs/node/pull/13004)
* [[`fefab9026b`](https://github.com/nodejs/node/commit/fefab9026b)] - **src**: only call FatalException if not verbose (Daniel Bevenius) [#12826](https://github.com/nodejs/node/pull/12826)
* [[`32f01c8c11`](https://github.com/nodejs/node/commit/32f01c8c11)] - **src**: remove unused uv.h include in async-wrap.h (Daniel Bevenius) [#12973](https://github.com/nodejs/node/pull/12973)
* [[`60f0dc7d42`](https://github.com/nodejs/node/commit/60f0dc7d42)] - **src**: rename CONNECTION provider to SSLCONNECTION (Daniel Bevenius) [#12989](https://github.com/nodejs/node/pull/12989)
* [[`15410797f2`](https://github.com/nodejs/node/commit/15410797f2)] - **src**: add HAVE_OPENSSL guard to crypto providers (Daniel Bevenius) [#12967](https://github.com/nodejs/node/pull/12967)
* [[`9f8e030f1b`](https://github.com/nodejs/node/commit/9f8e030f1b)] - **src**: add/move hasCrypto checks for async tests (Daniel Bevenius) [#12968](https://github.com/nodejs/node/pull/12968)
* [[`b6001a2da5`](https://github.com/nodejs/node/commit/b6001a2da5)] - **src**: make SIGPROF message a real warning (cjihrig) [#12709](https://github.com/nodejs/node/pull/12709)
* [[`dd6e3f69a7`](https://github.com/nodejs/node/commit/dd6e3f69a7)] - **src**: fix comments re PER_ISOLATE macros (Josh Gavant) [#12899](https://github.com/nodejs/node/pull/12899)
* [[`6ade7f3601`](https://github.com/nodejs/node/commit/6ade7f3601)] - **src**: update --inspect hint text (Josh Gavant) [#11207](https://github.com/nodejs/node/pull/11207)
* [[`d0c968ea57`](https://github.com/nodejs/node/commit/d0c968ea57)] - **src**: make root_cert_vector function scoped (Daniel Bevenius) [#12788](https://github.com/nodejs/node/pull/12788)
* [[`ebcd8c6bb8`](https://github.com/nodejs/node/commit/ebcd8c6bb8)] - **src**: rename CryptoPemCallback -\> PasswordCallback (Daniel Bevenius) [#12787](https://github.com/nodejs/node/pull/12787)
* [[`d56a7e640f`](https://github.com/nodejs/node/commit/d56a7e640f)] - **src**: do proper StringBytes error handling (Anna Henningsen) [#12765](https://github.com/nodejs/node/pull/12765)
* [[`9990be2919`](https://github.com/nodejs/node/commit/9990be2919)] - **src**: turn buffer type-CHECK into exception (Anna Henningsen) [#12753](https://github.com/nodejs/node/pull/12753)
* [[`21653b6901`](https://github.com/nodejs/node/commit/21653b6901)] - **src**: add --napi-modules to whitelist (Michael Dawson) [#12733](https://github.com/nodejs/node/pull/12733)
* [[`0f58d3cbef`](https://github.com/nodejs/node/commit/0f58d3cbef)] - **src**: support domains with empty labels (Daijiro Wachi) [#12707](https://github.com/nodejs/node/pull/12707)
* [[`719247ff95`](https://github.com/nodejs/node/commit/719247ff95)] - **src**: remove debugger dead code (Michaël Zasso) [#12621](https://github.com/nodejs/node/pull/12621)
* [[`892ce06dbd`](https://github.com/nodejs/node/commit/892ce06dbd)] - **src**: fix incorrect macro comment (Daniel Bevenius) [#12688](https://github.com/nodejs/node/pull/12688)
* [[`5bb06e8596`](https://github.com/nodejs/node/commit/5bb06e8596)] - **src**: remove GTEST_DONT_DEFINE_ASSERT_EQ in util.h (Daniel Bevenius) [#12638](https://github.com/nodejs/node/pull/12638)
* [[`f2282bb812`](https://github.com/nodejs/node/commit/f2282bb812)] - **src**: allow CLI args in env with NODE_OPTIONS (Sam Roberts) [#12028](https://github.com/nodejs/node/pull/12028)
* [[`6a1275dfec`](https://github.com/nodejs/node/commit/6a1275dfec)] - **src**: add missing "http_parser.h" include (Anna Henningsen) [#12464](https://github.com/nodejs/node/pull/12464)
* [[`5ef6000afd`](https://github.com/nodejs/node/commit/5ef6000afd)] - **src**: don't call uv_run() after 'exit' event (Ben Noordhuis) [#12344](https://github.com/nodejs/node/pull/12344)
* [[`ade80eeb1a`](https://github.com/nodejs/node/commit/ade80eeb1a)] - **src**: clean up WHATWG WG parser (Timothy Gu) [#12251](https://github.com/nodejs/node/pull/12251)
* [[`b2803637e8`](https://github.com/nodejs/node/commit/b2803637e8)] - **src**: replace IsConstructCall functions with lambda (Daniel Bevenius) [#12384](https://github.com/nodejs/node/pull/12384)
* [[`9d522225e7`](https://github.com/nodejs/node/commit/9d522225e7)] - **src**: reduce number of exported symbols (Anna Henningsen) [#12366](https://github.com/nodejs/node/pull/12366)
* [[`a4125a3c49`](https://github.com/nodejs/node/commit/a4125a3c49)] - **src**: remove experimental warning for inspect (Josh Gavant) [#12352](https://github.com/nodejs/node/pull/12352)
* [[`8086cb68ae`](https://github.com/nodejs/node/commit/8086cb68ae)] - **src**: use option parser for expose_internals (Sam Roberts) [#12245](https://github.com/nodejs/node/pull/12245)
* [[`e505c079e0`](https://github.com/nodejs/node/commit/e505c079e0)] - **src**: supply missing comments for CLI options (Sam Roberts) [#12245](https://github.com/nodejs/node/pull/12245)
* [[`de168b4b4a`](https://github.com/nodejs/node/commit/de168b4b4a)] - **src**: guard bundled_ca/openssl_ca with HAVE_OPENSSL (Daniel Bevenius) [#12302](https://github.com/nodejs/node/pull/12302)
* [[`cecdf7c118`](https://github.com/nodejs/node/commit/cecdf7c118)] - **src**: use a std::vector for preload_modules (Sam Roberts) [#12241](https://github.com/nodejs/node/pull/12241)
* [[`65a6e05da5`](https://github.com/nodejs/node/commit/65a6e05da5)] - **src**: only block SIGUSR1 when HAVE_INSPECTOR (Daniel Bevenius) [#12266](https://github.com/nodejs/node/pull/12266)
* [[`ebeee853e6`](https://github.com/nodejs/node/commit/ebeee853e6)] - **src**: Update trace event macros to V8 5.7 version (Matt Loring) [#12127](https://github.com/nodejs/node/pull/12127)
* [[`7c0079f1be`](https://github.com/nodejs/node/commit/7c0079f1be)] - **src**: add .FromJust(), fix -Wunused-result warnings (Ben Noordhuis) [#12118](https://github.com/nodejs/node/pull/12118)
* [[`4ddd23f0ec`](https://github.com/nodejs/node/commit/4ddd23f0ec)] - **src**: WHATWG URL C++ parser cleanup (Timothy Gu) [#11917](https://github.com/nodejs/node/pull/11917)
* [[`d099f8e317`](https://github.com/nodejs/node/commit/d099f8e317)] - **src**: remove explicit UTF-8 validity check in url (Timothy Gu) [#11859](https://github.com/nodejs/node/pull/11859)
* [[`e2f151f5b2`](https://github.com/nodejs/node/commit/e2f151f5b2)] - **src**: make process.env work with symbols in/delete (Timothy Gu) [#11709](https://github.com/nodejs/node/pull/11709)
* [[`e1d8899c28`](https://github.com/nodejs/node/commit/e1d8899c28)] - **src**: add HAVE_OPENSSL directive to openssl_config (Daniel Bevenius) [#11618](https://github.com/nodejs/node/pull/11618)
* [[`a7f7724167`](https://github.com/nodejs/node/commit/a7f7724167)] - **src**: remove misleading flag in TwoByteValue (Timothy Gu) [#11436](https://github.com/nodejs/node/pull/11436)
* [[`046f66a554`](https://github.com/nodejs/node/commit/046f66a554)] - **src**: fix building --without-v8-plartform (Myk Melez) [#11088](https://github.com/nodejs/node/pull/11088)
* [[`d317184f97`](https://github.com/nodejs/node/commit/d317184f97)] - **src**: bump version to v8.0.0 for master (Rod Vagg) [#8956](https://github.com/nodejs/node/pull/8956)
* [[`f077e51c92`](https://github.com/nodejs/node/commit/f077e51c92)] - **src,fs**: calculate fs times without truncation (Daniel Pihlstrom) [#12607](https://github.com/nodejs/node/pull/12607)
* [[`b8b6c2c262`](https://github.com/nodejs/node/commit/b8b6c2c262)] - **stream**: emit finish when using writev and cork (Matteo Collina) [#13195](https://github.com/nodejs/node/pull/13195)
* [[`c15fe8b78e`](https://github.com/nodejs/node/commit/c15fe8b78e)] - **stream**: remove dup property (Calvin Metcalf) [#13216](https://github.com/nodejs/node/pull/13216)
* [[`87cef63ccb`](https://github.com/nodejs/node/commit/87cef63ccb)] - **stream**: fix destroy(err, cb) regression (Matteo Collina) [#13156](https://github.com/nodejs/node/pull/13156)
* [[`8914f7b4b7`](https://github.com/nodejs/node/commit/8914f7b4b7)] - **stream**: improve readable push performance (Brian White) [#13113](https://github.com/nodejs/node/pull/13113)
* [[`6993eb0897`](https://github.com/nodejs/node/commit/6993eb0897)] - **stream**: fix y.pipe(x)+y.pipe(x)+y.unpipe(x) (Anna Henningsen) [#12746](https://github.com/nodejs/node/pull/12746)
* [[`d6a6bcdc47`](https://github.com/nodejs/node/commit/d6a6bcdc47)] - **stream**: remove unnecessary parameter (Leo) [#12767](https://github.com/nodejs/node/pull/12767)
* [[`e2199e0fc2`](https://github.com/nodejs/node/commit/e2199e0fc2)] - **streams**: refactor BufferList into ES6 class (James M Snell) [#12644](https://github.com/nodejs/node/pull/12644)
* [[`ea6941f703`](https://github.com/nodejs/node/commit/ea6941f703)] - **test**: refactor test-fs-assert-encoding-error (Rich Trott) [#13226](https://github.com/nodejs/node/pull/13226)
* [[`8d193919fb`](https://github.com/nodejs/node/commit/8d193919fb)] - **test**: replace `indexOf` with `includes` (Aditya Anand) [#13215](https://github.com/nodejs/node/pull/13215)
* [[`2c5c2bda61`](https://github.com/nodejs/node/commit/2c5c2bda61)] - **test**: check noop invocation with mustNotCall() (Rich Trott) [#13205](https://github.com/nodejs/node/pull/13205)
* [[`d0dbd53eb0`](https://github.com/nodejs/node/commit/d0dbd53eb0)] - **test**: add coverage for socket write after close (cjihrig) [#13171](https://github.com/nodejs/node/pull/13171)
* [[`686e753b7e`](https://github.com/nodejs/node/commit/686e753b7e)] - **test**: use common.mustNotCall in test-crypto-random (Rich Trott) [#13183](https://github.com/nodejs/node/pull/13183)
* [[`4030aed8ce`](https://github.com/nodejs/node/commit/4030aed8ce)] - **test**: skip test-bindings if inspector is disabled (Daniel Bevenius) [#13186](https://github.com/nodejs/node/pull/13186)
* [[`a590709909`](https://github.com/nodejs/node/commit/a590709909)] - **test**: add coverage for napi_has_named_property (Michael Dawson) [#13178](https://github.com/nodejs/node/pull/13178)
* [[`72a319e417`](https://github.com/nodejs/node/commit/72a319e417)] - **test**: refactor event-emitter-remove-all-listeners (Rich Trott) [#13165](https://github.com/nodejs/node/pull/13165)
* [[`c4502728fb`](https://github.com/nodejs/node/commit/c4502728fb)] - **test**: refactor event-emitter-check-listener-leaks (Rich Trott) [#13164](https://github.com/nodejs/node/pull/13164)
* [[`597aff0846`](https://github.com/nodejs/node/commit/597aff0846)] - **test**: cover dgram handle send failures (cjihrig) [#13158](https://github.com/nodejs/node/pull/13158)
* [[`5ad4170cd9`](https://github.com/nodejs/node/commit/5ad4170cd9)] - **test**: cover util.format() format placeholders (cjihrig) [#13159](https://github.com/nodejs/node/pull/13159)
* [[`b781fa7b06`](https://github.com/nodejs/node/commit/b781fa7b06)] - **test**: add override to ServerDone function (Daniel Bevenius) [#13166](https://github.com/nodejs/node/pull/13166)
* [[`a985ed66c4`](https://github.com/nodejs/node/commit/a985ed66c4)] - **test**: refactor test-dns (Rich Trott) [#13163](https://github.com/nodejs/node/pull/13163)
* [[`7fe5303983`](https://github.com/nodejs/node/commit/7fe5303983)] - **test**: fix disabled test-fs-largefile (Rich Trott) [#13147](https://github.com/nodejs/node/pull/13147)
* [[`e012f5a412`](https://github.com/nodejs/node/commit/e012f5a412)] - **test**: move stream2 test from pummel to parallel (Rich Trott) [#13146](https://github.com/nodejs/node/pull/13146)
* [[`9100cac146`](https://github.com/nodejs/node/commit/9100cac146)] - **test**: simplify assert usage in test-stream2-basic (Rich Trott) [#13146](https://github.com/nodejs/node/pull/13146)
* [[`cd70a520d2`](https://github.com/nodejs/node/commit/cd70a520d2)] - **test**: check noop function invocations (Rich Trott) [#13146](https://github.com/nodejs/node/pull/13146)
* [[`110a3b2657`](https://github.com/nodejs/node/commit/110a3b2657)] - **test**: confirm callback is invoked in fs test (Rich Trott) [#13132](https://github.com/nodejs/node/pull/13132)
* [[`1da674e2c0`](https://github.com/nodejs/node/commit/1da674e2c0)] - **test**: check number of message events (Rich Trott) [#13125](https://github.com/nodejs/node/pull/13125)
* [[`4ccfd7cf15`](https://github.com/nodejs/node/commit/4ccfd7cf15)] - **test**: increase n-api constructor coverage (Michael Dawson) [#13124](https://github.com/nodejs/node/pull/13124)
* [[`6cfb876d54`](https://github.com/nodejs/node/commit/6cfb876d54)] - **test**: add regression test for immediate socket errors (Evan Lucas) [#12854](https://github.com/nodejs/node/pull/12854)
* [[`268a39ac2a`](https://github.com/nodejs/node/commit/268a39ac2a)] - **test**: add hasCrypto check to async-wrap-GH13045 (Daniel Bevenius) [#13141](https://github.com/nodejs/node/pull/13141)
* [[`e6c03c78f7`](https://github.com/nodejs/node/commit/e6c03c78f7)] - **test**: fix sequential test-net-connect-local-error (Sebastian Plesciuc) [#13064](https://github.com/nodejs/node/pull/13064)
* [[`511ee24310`](https://github.com/nodejs/node/commit/511ee24310)] - **test**: remove common.PORT from dgram test (Artur Vieira) [#12944](https://github.com/nodejs/node/pull/12944)
* [[`8a4f3b7dfc`](https://github.com/nodejs/node/commit/8a4f3b7dfc)] - **test**: bind to 0 in dgram-send-callback-buffer-length (Artur Vieira) [#12943](https://github.com/nodejs/node/pull/12943)
* [[`9fc47de8e6`](https://github.com/nodejs/node/commit/9fc47de8e6)] - **test**: use dynamic port in test-dgram-send-address-types (Artur Vieira) [#13007](https://github.com/nodejs/node/pull/13007)
* [[`8ef4fe0af2`](https://github.com/nodejs/node/commit/8ef4fe0af2)] - **test**: use dynamic port in test-dgram-send-callback-buffer (Artur Vieira) [#12942](https://github.com/nodejs/node/pull/12942)
* [[`96925e1b93`](https://github.com/nodejs/node/commit/96925e1b93)] - **test**: replace common.PORT in dgram test (Artur Vieira) [#12929](https://github.com/nodejs/node/pull/12929)
* [[`1af8b70c57`](https://github.com/nodejs/node/commit/1af8b70c57)] - **test**: allow for absent nobody user in setuid test (Rich Trott) [#13112](https://github.com/nodejs/node/pull/13112)
* [[`e29477ab25`](https://github.com/nodejs/node/commit/e29477ab25)] - **test**: shorten test-benchmark-http (Rich Trott) [#13109](https://github.com/nodejs/node/pull/13109)
* [[`595e5e3b23`](https://github.com/nodejs/node/commit/595e5e3b23)] - **test**: port disabled readline test (Rich Trott) [#13091](https://github.com/nodejs/node/pull/13091)
* [[`c60a7fa738`](https://github.com/nodejs/node/commit/c60a7fa738)] - **test**: move net reconnect error test to sequential (Artur G Vieira) [#13033](https://github.com/nodejs/node/pull/13033)
* [[`525497596a`](https://github.com/nodejs/node/commit/525497596a)] - **test**: refactor test-net-GH-5504 (Rich Trott) [#13025](https://github.com/nodejs/node/pull/13025)
* [[`658741b9d9`](https://github.com/nodejs/node/commit/658741b9d9)] - **test**: refactor test-https-set-timeout-server (Rich Trott) [#13032](https://github.com/nodejs/node/pull/13032)
* [[`fccc0bf6e6`](https://github.com/nodejs/node/commit/fccc0bf6e6)] - **test**: add mustCallAtLeast (Refael Ackermann) [#12935](https://github.com/nodejs/node/pull/12935)
* [[`6f216710eb`](https://github.com/nodejs/node/commit/6f216710eb)] - **test**: ignore spurious 'EMFILE' (Refael Ackermann) [#12698](https://github.com/nodejs/node/pull/12698)
* [[`6b1819cff5`](https://github.com/nodejs/node/commit/6b1819cff5)] - **test**: use dynamic port in test-cluster-dgram-reuse (Artur Vieira) [#12901](https://github.com/nodejs/node/pull/12901)
* [[`a593c74f81`](https://github.com/nodejs/node/commit/a593c74f81)] - **test**: refactor test-vm-new-script-new-context (Akshay Iyer) [#13035](https://github.com/nodejs/node/pull/13035)
* [[`7e5ed8bad9`](https://github.com/nodejs/node/commit/7e5ed8bad9)] - **test**: track callback invocations (Rich Trott) [#13010](https://github.com/nodejs/node/pull/13010)
* [[`47e3d00241`](https://github.com/nodejs/node/commit/47e3d00241)] - **test**: refactor test-dns-regress-6244.js (Rich Trott) [#13058](https://github.com/nodejs/node/pull/13058)
* [[`6933419cb9`](https://github.com/nodejs/node/commit/6933419cb9)] - **test**: add hasCrypto to tls-lookup (Daniel Bevenius) [#13047](https://github.com/nodejs/node/pull/13047)
* [[`0dd8b9a965`](https://github.com/nodejs/node/commit/0dd8b9a965)] - **test**: Improve N-API test coverage (Michael Dawson) [#13044](https://github.com/nodejs/node/pull/13044)
* [[`5debcceafc`](https://github.com/nodejs/node/commit/5debcceafc)] - **test**: add hasCrypto to tls-wrap-event-emmiter (Daniel Bevenius) [#13041](https://github.com/nodejs/node/pull/13041)
* [[`7906ed50fa`](https://github.com/nodejs/node/commit/7906ed50fa)] - **test**: add regex check in test-url-parse-invalid-input (Andrei Cioromila) [#12879](https://github.com/nodejs/node/pull/12879)
* [[`0c2edd27e6`](https://github.com/nodejs/node/commit/0c2edd27e6)] - **test**: fixed flaky test-net-connect-local-error (Sebastian Plesciuc) [#12964](https://github.com/nodejs/node/pull/12964)
* [[`47c3c58704`](https://github.com/nodejs/node/commit/47c3c58704)] - **test**: improve N-API test coverage (Michael Dawson) [#13006](https://github.com/nodejs/node/pull/13006)
* [[`88d2e699d8`](https://github.com/nodejs/node/commit/88d2e699d8)] - **test**: remove unneeded string splitting (Vse Mozhet Byt) [#12992](https://github.com/nodejs/node/pull/12992)
* [[`72e3dda93c`](https://github.com/nodejs/node/commit/72e3dda93c)] - **test**: use mustCall in tls-connect-given-socket (vperezma) [#12592](https://github.com/nodejs/node/pull/12592)
* [[`b7bc09fd60`](https://github.com/nodejs/node/commit/b7bc09fd60)] - **test**: add not-called check to heap-profiler test (Rich Trott) [#12985](https://github.com/nodejs/node/pull/12985)
* [[`b5ae22dd1c`](https://github.com/nodejs/node/commit/b5ae22dd1c)] - **test**: add hasCrypto check to https-agent-constructor (Daniel Bevenius) [#12987](https://github.com/nodejs/node/pull/12987)
* [[`945f208081`](https://github.com/nodejs/node/commit/945f208081)] - **test**: make the rest of tests path-independent (Vse Mozhet Byt) [#12972](https://github.com/nodejs/node/pull/12972)
* [[`9516aa19c1`](https://github.com/nodejs/node/commit/9516aa19c1)] - **test**: add common.mustCall() to NAPI exception test (Rich Trott) [#12959](https://github.com/nodejs/node/pull/12959)
* [[`84fc069b95`](https://github.com/nodejs/node/commit/84fc069b95)] - **test**: move test-dgram-bind-shared-ports to sequential (Rafael Fragoso) [#12452](https://github.com/nodejs/node/pull/12452)
* [[`642bd4dd6d`](https://github.com/nodejs/node/commit/642bd4dd6d)] - **test**: add a simple abort check in windows (Sreepurna Jasti) [#12914](https://github.com/nodejs/node/pull/12914)
* [[`56812c81a3`](https://github.com/nodejs/node/commit/56812c81a3)] - **test**: use dynamic port in test-https-connect-address-family (Artur G Vieira) [#12915](https://github.com/nodejs/node/pull/12915)
* [[`529e4f206a`](https://github.com/nodejs/node/commit/529e4f206a)] - **test**: make a test path-independent (Vse Mozhet Byt) [#12945](https://github.com/nodejs/node/pull/12945)
* [[`631cb42b4e`](https://github.com/nodejs/node/commit/631cb42b4e)] - **test**: favor deepStrictEqual over deepEqual (Rich Trott) [#12883](https://github.com/nodejs/node/pull/12883)
* [[`654afa2c19`](https://github.com/nodejs/node/commit/654afa2c19)] - **test**: improve n-api array func coverage (Michael Dawson) [#12890](https://github.com/nodejs/node/pull/12890)
* [[`bee250c232`](https://github.com/nodejs/node/commit/bee250c232)] - **test**: dynamic port in cluster disconnect (Sebastian Plesciuc) [#12545](https://github.com/nodejs/node/pull/12545)
* [[`6914aeaefd`](https://github.com/nodejs/node/commit/6914aeaefd)] - **test**: detect all types of aborts in windows (Gireesh Punathil) [#12856](https://github.com/nodejs/node/pull/12856)
* [[`cfe7b34058`](https://github.com/nodejs/node/commit/cfe7b34058)] - **test**: use assert regexp in tls no cert test (Artur Vieira) [#12891](https://github.com/nodejs/node/pull/12891)
* [[`317180ffe5`](https://github.com/nodejs/node/commit/317180ffe5)] - **test**: fix flaky test-https-client-get-url (Sebastian Plesciuc) [#12876](https://github.com/nodejs/node/pull/12876)
* [[`57a08e2f70`](https://github.com/nodejs/node/commit/57a08e2f70)] - **test**: remove obsolete lint config comments (Rich Trott) [#12868](https://github.com/nodejs/node/pull/12868)
* [[`94eed0fb11`](https://github.com/nodejs/node/commit/94eed0fb11)] - **test**: use dynamic port instead of common.PORT (Aditya Anand) [#12473](https://github.com/nodejs/node/pull/12473)
* [[`f72376d323`](https://github.com/nodejs/node/commit/f72376d323)] - **test**: add skipIfInspectorDisabled to debugger-pid (Daniel Bevenius) [#12882](https://github.com/nodejs/node/pull/12882)
* [[`771568a5a5`](https://github.com/nodejs/node/commit/771568a5a5)] - **test**: add test for timers benchmarks (Joyee Cheung) [#12851](https://github.com/nodejs/node/pull/12851)
* [[`dc4313c620`](https://github.com/nodejs/node/commit/dc4313c620)] - **test**: remove unused testpy code (Rich Trott) [#12844](https://github.com/nodejs/node/pull/12844)
* [[`0a734fec88`](https://github.com/nodejs/node/commit/0a734fec88)] - **test**: fix napi test_reference for recent V8 (Michaël Zasso) [#12864](https://github.com/nodejs/node/pull/12864)
* [[`42958d1a75`](https://github.com/nodejs/node/commit/42958d1a75)] - **test**: refactor test-querystring (Łukasz Szewczak) [#12661](https://github.com/nodejs/node/pull/12661)
* [[`152966dbb5`](https://github.com/nodejs/node/commit/152966dbb5)] - **test**: refactoring test with common.mustCall (weewey) [#12702](https://github.com/nodejs/node/pull/12702)
* [[`6058c4349f`](https://github.com/nodejs/node/commit/6058c4349f)] - **test**: refactored test-repl-persistent-history (cool88) [#12703](https://github.com/nodejs/node/pull/12703)
* [[`dac9f42a7e`](https://github.com/nodejs/node/commit/dac9f42a7e)] - **test**: remove common.PORT in test tls ticket cluster (Oscar Martinez) [#12715](https://github.com/nodejs/node/pull/12715)
* [[`d37f27a008`](https://github.com/nodejs/node/commit/d37f27a008)] - **test**: expand test coverage of readline (James M Snell) [#12755](https://github.com/nodejs/node/pull/12755)
* [[`a710e443a2`](https://github.com/nodejs/node/commit/a710e443a2)] - **test**: complete coverage of buffer (David Cai) [#12831](https://github.com/nodejs/node/pull/12831)
* [[`3fd890a06e`](https://github.com/nodejs/node/commit/3fd890a06e)] - **test**: add mustCall in timers-unrefed-in-callback (Zahidul Islam) [#12594](https://github.com/nodejs/node/pull/12594)
* [[`73d9c0f903`](https://github.com/nodejs/node/commit/73d9c0f903)] - **test**: port test for make_callback to n-api (Hitesh Kanwathirtha) [#12409](https://github.com/nodejs/node/pull/12409)
* [[`68c933c01e`](https://github.com/nodejs/node/commit/68c933c01e)] - **test**: fix flakyness with `yes.exe` (Refael Ackermann) [#12821](https://github.com/nodejs/node/pull/12821)
* [[`8b76c3e60c`](https://github.com/nodejs/node/commit/8b76c3e60c)] - **test**: reduce string concatenations (Vse Mozhet Byt) [#12735](https://github.com/nodejs/node/pull/12735)
* [[`f1d593cda1`](https://github.com/nodejs/node/commit/f1d593cda1)] - **test**: make tests cwd-independent (Vse Mozhet Byt) [#12812](https://github.com/nodejs/node/pull/12812)
* [[`94a120cf65`](https://github.com/nodejs/node/commit/94a120cf65)] - **test**: add coverage for error apis (Michael Dawson) [#12729](https://github.com/nodejs/node/pull/12729)
* [[`bc05436a89`](https://github.com/nodejs/node/commit/bc05436a89)] - **test**: add regex check in test-vm-is-context (jeyanthinath) [#12785](https://github.com/nodejs/node/pull/12785)
* [[`665695fbea`](https://github.com/nodejs/node/commit/665695fbea)] - **test**: add callback to fs.close() in test-fs-stat (Vse Mozhet Byt) [#12804](https://github.com/nodejs/node/pull/12804)
* [[`712596fc45`](https://github.com/nodejs/node/commit/712596fc45)] - **test**: add callback to fs.close() in test-fs-chmod (Vse Mozhet Byt) [#12795](https://github.com/nodejs/node/pull/12795)
* [[`f971916885`](https://github.com/nodejs/node/commit/f971916885)] - **test**: fix too optimistic guess in setproctitle (Vse Mozhet Byt) [#12792](https://github.com/nodejs/node/pull/12792)
* [[`4677766d21`](https://github.com/nodejs/node/commit/4677766d21)] - **test**: enable test-debugger-pid (Rich Trott) [#12770](https://github.com/nodejs/node/pull/12770)
* [[`ff001c12b0`](https://github.com/nodejs/node/commit/ff001c12b0)] - **test**: move WPT to its own testing module (Rich Trott) [#12736](https://github.com/nodejs/node/pull/12736)
* [[`b2ab41e5ae`](https://github.com/nodejs/node/commit/b2ab41e5ae)] - **test**: increase readline coverage (Anna Henningsen) [#12761](https://github.com/nodejs/node/pull/12761)
* [[`8aca66a1f3`](https://github.com/nodejs/node/commit/8aca66a1f3)] - **test**: fix warning in n-api reference test (Michael Dawson) [#12730](https://github.com/nodejs/node/pull/12730)
* [[`04796ee97f`](https://github.com/nodejs/node/commit/04796ee97f)] - **test**: increase coverage of buffer (David Cai) [#12714](https://github.com/nodejs/node/pull/12714)
* [[`133fb0c3b7`](https://github.com/nodejs/node/commit/133fb0c3b7)] - **test**: minor fixes to test-module-loading.js (Walter Huang) [#12728](https://github.com/nodejs/node/pull/12728)
* [[`9f7b54945e`](https://github.com/nodejs/node/commit/9f7b54945e)] - ***Revert*** "**test**: remove eslint comments" (Joyee Cheung) [#12743](https://github.com/nodejs/node/pull/12743)
* [[`10ccf56f89`](https://github.com/nodejs/node/commit/10ccf56f89)] - **test**: skipIfInspectorDisabled cluster-inspect-brk (Daniel Bevenius) [#12757](https://github.com/nodejs/node/pull/12757)
* [[`0142276977`](https://github.com/nodejs/node/commit/0142276977)] - **test**: replace indexOf with includes (gwer) [#12604](https://github.com/nodejs/node/pull/12604)
* [[`0324ac686c`](https://github.com/nodejs/node/commit/0324ac686c)] - **test**: add inspect-brk option to cluster module (dave-k) [#12503](https://github.com/nodejs/node/pull/12503)
* [[`d5db4d25dc`](https://github.com/nodejs/node/commit/d5db4d25dc)] - **test**: cleanup handles in test_environment (Anna Henningsen) [#12621](https://github.com/nodejs/node/pull/12621)
* [[`427cd293d5`](https://github.com/nodejs/node/commit/427cd293d5)] - **test**: add hasCrypto check to test-cli-node-options (Daniel Bevenius) [#12692](https://github.com/nodejs/node/pull/12692)
* [[`0101a8f1a6`](https://github.com/nodejs/node/commit/0101a8f1a6)] - **test**: add relative path to accommodate limit (coreybeaumont) [#12601](https://github.com/nodejs/node/pull/12601)
* [[`b16869c4e4`](https://github.com/nodejs/node/commit/b16869c4e4)] - **test**: remove AIX guard in fs-options-immutable (Sakthipriyan Vairamani (thefourtheye)) [#12687](https://github.com/nodejs/node/pull/12687)
* [[`a4fd9e5e6d`](https://github.com/nodejs/node/commit/a4fd9e5e6d)] - **test**: chdir before running test-cli-node-options (Daniel Bevenius) [#12660](https://github.com/nodejs/node/pull/12660)
* [[`d289678352`](https://github.com/nodejs/node/commit/d289678352)] - **test**: dynamic port in dgram tests (Sebastian Plesciuc) [#12623](https://github.com/nodejs/node/pull/12623)
* [[`28f535a923`](https://github.com/nodejs/node/commit/28f535a923)] - **test**: fixup test-http-hostname-typechecking (Anna Henningsen) [#12627](https://github.com/nodejs/node/pull/12627)
* [[`e927809eec`](https://github.com/nodejs/node/commit/e927809eec)] - **test**: dynamic port in parallel regress tests (Sebastian Plesciuc) [#12639](https://github.com/nodejs/node/pull/12639)
* [[`1d968030d4`](https://github.com/nodejs/node/commit/1d968030d4)] - **test**: add coverage for napi_cancel_async_work (Michael Dawson) [#12575](https://github.com/nodejs/node/pull/12575)
* [[`4241577112`](https://github.com/nodejs/node/commit/4241577112)] - **test**: test doc'd napi_get_value_int32 behaviour (Michael Dawson) [#12633](https://github.com/nodejs/node/pull/12633)
* [[`bda34bde56`](https://github.com/nodejs/node/commit/bda34bde56)] - **test**: remove obsolete lint comment (Rich Trott) [#12659](https://github.com/nodejs/node/pull/12659)
* [[`c8c5a528da`](https://github.com/nodejs/node/commit/c8c5a528da)] - **test**: make tests pass when built without inspector (Michaël Zasso) [#12622](https://github.com/nodejs/node/pull/12622)
* [[`d1d9ecfe6e`](https://github.com/nodejs/node/commit/d1d9ecfe6e)] - **test**: support unreleased V8 versions (Michaël Zasso) [#12619](https://github.com/nodejs/node/pull/12619)
* [[`75bfdad037`](https://github.com/nodejs/node/commit/75bfdad037)] - **test**: check that pending warning is emitted once (Rich Trott) [#12527](https://github.com/nodejs/node/pull/12527)
* [[`5e095f699e`](https://github.com/nodejs/node/commit/5e095f699e)] - **test**: verify listener leak is only emitted once (cjihrig) [#12502](https://github.com/nodejs/node/pull/12502)
* [[`4bcbefccce`](https://github.com/nodejs/node/commit/4bcbefccce)] - **test**: add coverage for vm's breakOnSigint option (cjihrig) [#12512](https://github.com/nodejs/node/pull/12512)
* [[`f3f9dd73aa`](https://github.com/nodejs/node/commit/f3f9dd73aa)] - **test**: skip tests using ca flags (Daniel Bevenius) [#12485](https://github.com/nodejs/node/pull/12485)
* [[`86a3ba0c4e`](https://github.com/nodejs/node/commit/86a3ba0c4e)] - **test**: dynamic port in cluster worker wait close (Sebastian Plesciuc) [#12466](https://github.com/nodejs/node/pull/12466)
* [[`6c912a8216`](https://github.com/nodejs/node/commit/6c912a8216)] - **test**: fix coverity UNINIT_CTOR cctest warning (Ben Noordhuis) [#12387](https://github.com/nodejs/node/pull/12387)
* [[`4fc11998b4`](https://github.com/nodejs/node/commit/4fc11998b4)] - **test**: add cwd ENOENT known issue test (cjihrig) [#12343](https://github.com/nodejs/node/pull/12343)
* [[`2e5188de92`](https://github.com/nodejs/node/commit/2e5188de92)] - **test**: remove common.PORT from multiple tests (Tarun Batra) [#12451](https://github.com/nodejs/node/pull/12451)
* [[`7044065f1a`](https://github.com/nodejs/node/commit/7044065f1a)] - **test**: change == to === in crypto test (Fabio Campinho) [#12405](https://github.com/nodejs/node/pull/12405)
* [[`f98db78f77`](https://github.com/nodejs/node/commit/f98db78f77)] - **test**: add internal/fs tests (DavidCai) [#12306](https://github.com/nodejs/node/pull/12306)
* [[`3d2181c5f0`](https://github.com/nodejs/node/commit/3d2181c5f0)] - **test**: run the addon tests last (Sebastian Van Sande) [#12062](https://github.com/nodejs/node/pull/12062)
* [[`8bd26d3aea`](https://github.com/nodejs/node/commit/8bd26d3aea)] - **test**: fix compiler warning in n-api test (Anna Henningsen) [#12318](https://github.com/nodejs/node/pull/12318)
* [[`3900cf66a5`](https://github.com/nodejs/node/commit/3900cf66a5)] - **test**: remove disabled test-dgram-send-error (Rich Trott) [#12330](https://github.com/nodejs/node/pull/12330)
* [[`9de2e159c4`](https://github.com/nodejs/node/commit/9de2e159c4)] - **test**: add second argument to assert.throws (Michaël Zasso) [#12270](https://github.com/nodejs/node/pull/12270)
* [[`0ec0272e10`](https://github.com/nodejs/node/commit/0ec0272e10)] - **test**: improve test coverage for n-api (Michael Dawson) [#12327](https://github.com/nodejs/node/pull/12327)
* [[`569f988be7`](https://github.com/nodejs/node/commit/569f988be7)] - **test**: remove disabled tls_server.js (Rich Trott) [#12275](https://github.com/nodejs/node/pull/12275)
* [[`2555780aa6`](https://github.com/nodejs/node/commit/2555780aa6)] - **test**: check curve algorithm is supported (Karl Cheng) [#12265](https://github.com/nodejs/node/pull/12265)
* [[`2d3d4ccb98`](https://github.com/nodejs/node/commit/2d3d4ccb98)] - **test**: add http benchmark test (Joyee Cheung) [#12121](https://github.com/nodejs/node/pull/12121)
* [[`b03f1f0c01`](https://github.com/nodejs/node/commit/b03f1f0c01)] - **test**: add basic cctest for base64.h (Alexey Orlenko) [#12238](https://github.com/nodejs/node/pull/12238)
* [[`971fe67dce`](https://github.com/nodejs/node/commit/971fe67dce)] - **test**: complete coverage for lib/assert.js (Rich Trott) [#12239](https://github.com/nodejs/node/pull/12239)
* [[`65c100ae8b`](https://github.com/nodejs/node/commit/65c100ae8b)] - **test**: remove disabled debugger test (Rich Trott) [#12199](https://github.com/nodejs/node/pull/12199)
* [[`610ac7d858`](https://github.com/nodejs/node/commit/610ac7d858)] - **test**: increase coverage of internal/socket_list (DavidCai) [#12066](https://github.com/nodejs/node/pull/12066)
* [[`2ff107dad7`](https://github.com/nodejs/node/commit/2ff107dad7)] - **test**: add case for url.parse throwing a URIError (Lovell Fuller) [#12135](https://github.com/nodejs/node/pull/12135)
* [[`5ccaba49f0`](https://github.com/nodejs/node/commit/5ccaba49f0)] - **test**: add variable arguments support for Argv (Daniel Bevenius) [#12166](https://github.com/nodejs/node/pull/12166)
* [[`9348f31c2a`](https://github.com/nodejs/node/commit/9348f31c2a)] - **test**: fix test-cli-syntax assertions on windows (Teddy Katz) [#12212](https://github.com/nodejs/node/pull/12212)
* [[`53828e8bff`](https://github.com/nodejs/node/commit/53828e8bff)] - **test**: extended test to makeCallback cb type check (Luca Maraschi) [#12140](https://github.com/nodejs/node/pull/12140)
* [[`9b05393362`](https://github.com/nodejs/node/commit/9b05393362)] - **test**: fix V8 test on big-endian machines (Anna Henningsen) [#12186](https://github.com/nodejs/node/pull/12186)
* [[`50bfef66f0`](https://github.com/nodejs/node/commit/50bfef66f0)] - **test**: synchronize WPT url test data (Daijiro Wachi) [#12058](https://github.com/nodejs/node/pull/12058)
* [[`92de91d570`](https://github.com/nodejs/node/commit/92de91d570)] - **test**: fix truncation of argv (Daniel Bevenius) [#12110](https://github.com/nodejs/node/pull/12110)
* [[`51b007aaa7`](https://github.com/nodejs/node/commit/51b007aaa7)] - **test**: add cctest for native URL class (James M Snell) [#12042](https://github.com/nodejs/node/pull/12042)
* [[`4f2e372716`](https://github.com/nodejs/node/commit/4f2e372716)] - **test**: add common.noop, default for common.mustCall() (James M Snell) [#12027](https://github.com/nodejs/node/pull/12027)
* [[`4929d12e99`](https://github.com/nodejs/node/commit/4929d12e99)] - **test**: add internal/socket_list tests (DavidCai) [#11989](https://github.com/nodejs/node/pull/11989)
* [[`64d0a73574`](https://github.com/nodejs/node/commit/64d0a73574)] - **test**: minor fixups for REPL eval tests (Anna Henningsen) [#11946](https://github.com/nodejs/node/pull/11946)
* [[`6aed32c579`](https://github.com/nodejs/node/commit/6aed32c579)] - **test**: add tests for unixtimestamp generation (Luca Maraschi) [#11886](https://github.com/nodejs/node/pull/11886)
* [[`1ff6796083`](https://github.com/nodejs/node/commit/1ff6796083)] - **test**: added net.connect lookup type check (Luca Maraschi) [#11873](https://github.com/nodejs/node/pull/11873)
* [[`7b830f4e4a`](https://github.com/nodejs/node/commit/7b830f4e4a)] - **test**: add more and refactor test cases to net.connect (Joyee Cheung) [#11847](https://github.com/nodejs/node/pull/11847)
* [[`474e9d64b5`](https://github.com/nodejs/node/commit/474e9d64b5)] - **test**: add more test cases of server.listen option (Joyee Cheung)
* [[`78cdd4baa4`](https://github.com/nodejs/node/commit/78cdd4baa4)] - **test**: include all stdio strings for fork() (Rich Trott) [#11783](https://github.com/nodejs/node/pull/11783)
* [[`b98004b79c`](https://github.com/nodejs/node/commit/b98004b79c)] - **test**: add hasCrypto check to tls-legacy-deprecated (Daniel Bevenius) [#11747](https://github.com/nodejs/node/pull/11747)
* [[`60c8115f63`](https://github.com/nodejs/node/commit/60c8115f63)] - **test**: clean up comments in test-url-format (Rich Trott) [#11679](https://github.com/nodejs/node/pull/11679)
* [[`1402fef098`](https://github.com/nodejs/node/commit/1402fef098)] - **test**: make tests pass when configured without-ssl (Daniel Bevenius) [#11631](https://github.com/nodejs/node/pull/11631)
* [[`acc3a80546`](https://github.com/nodejs/node/commit/acc3a80546)] - **test**: add two test cases for querystring (Daijiro Wachi) [#11551](https://github.com/nodejs/node/pull/11551)
* [[`a218fa381f`](https://github.com/nodejs/node/commit/a218fa381f)] - **test**: fix WPT.test()'s error handling (Timothy Gu) [#11436](https://github.com/nodejs/node/pull/11436)
* [[`dd2e135560`](https://github.com/nodejs/node/commit/dd2e135560)] - **test**: add two test cases for querystring (Daijiro Wachi) [#11481](https://github.com/nodejs/node/pull/11481)
* [[`82ddf96828`](https://github.com/nodejs/node/commit/82ddf96828)] - **test**: turn on WPT tests on empty param pairs (Joyee Cheung) [#11369](https://github.com/nodejs/node/pull/11369)
* [[`8bcc122349`](https://github.com/nodejs/node/commit/8bcc122349)] - **test**: improve querystring.parse assertion messages (Brian White) [#11234](https://github.com/nodejs/node/pull/11234)
* [[`dd1cf8bb37`](https://github.com/nodejs/node/commit/dd1cf8bb37)] - **test**: refactor test-http-response-statuscode (Rich Trott) [#11274](https://github.com/nodejs/node/pull/11274)
* [[`1544d8f04b`](https://github.com/nodejs/node/commit/1544d8f04b)] - **test**: improve test-buffer-includes.js (toboid) [#11203](https://github.com/nodejs/node/pull/11203)
* [[`f8cdaaa16a`](https://github.com/nodejs/node/commit/f8cdaaa16a)] - **test**: validate error message from buffer.equals (Sebastian Roeder) [#11215](https://github.com/nodejs/node/pull/11215)
* [[`901cb8cb5e`](https://github.com/nodejs/node/commit/901cb8cb5e)] - **test**: increase coverage of buffer (DavidCai) [#11122](https://github.com/nodejs/node/pull/11122)
* [[`78545039d6`](https://github.com/nodejs/node/commit/78545039d6)] - **test**: remove unnecessary eslint-disable max-len (Joyee Cheung) [#11049](https://github.com/nodejs/node/pull/11049)
* [[`6af10907a2`](https://github.com/nodejs/node/commit/6af10907a2)] - **test**: add msg validation to test-buffer-compare (Josh Hollandsworth) [#10807](https://github.com/nodejs/node/pull/10807)
* [[`775de9cc96`](https://github.com/nodejs/node/commit/775de9cc96)] - **test**: improve module version mismatch error check (cjihrig) [#10636](https://github.com/nodejs/node/pull/10636)
* [[`904b66d870`](https://github.com/nodejs/node/commit/904b66d870)] - **test**: increase coverage of Buffer.transcode (Joyee Cheung) [#10437](https://github.com/nodejs/node/pull/10437)
* [[`a180259e42`](https://github.com/nodejs/node/commit/a180259e42)] - **test,lib,doc**: use function declarations (Rich Trott) [#12711](https://github.com/nodejs/node/pull/12711)
* [[`98609fc1c4`](https://github.com/nodejs/node/commit/98609fc1c4)] - **timers**: do not use user object call/apply (Rich Trott) [#12960](https://github.com/nodejs/node/pull/12960)
* [[`b23d414c7e`](https://github.com/nodejs/node/commit/b23d414c7e)] - **tls**: do not wrap net.Socket with StreamWrap (Ruslan Bekenev) [#12799](https://github.com/nodejs/node/pull/12799)
* [[`bfa27d22f5`](https://github.com/nodejs/node/commit/bfa27d22f5)] - **tools**: update certdata.txt (Ben Noordhuis) [#13279](https://github.com/nodejs/node/pull/13279)
* [[`feb90d37ff`](https://github.com/nodejs/node/commit/feb90d37ff)] - **tools**: relax lint rule for regexps (Rich Trott) [#12807](https://github.com/nodejs/node/pull/12807)
* [[`53c88fa411`](https://github.com/nodejs/node/commit/53c88fa411)] - **tools**: remove unused code from test.py (Rich Trott) [#12806](https://github.com/nodejs/node/pull/12806)
* [[`595d4ec114`](https://github.com/nodejs/node/commit/595d4ec114)] - **tools**: ignore node_trace.*.log (Daijiro Wachi) [#12754](https://github.com/nodejs/node/pull/12754)
* [[`aea7269c45`](https://github.com/nodejs/node/commit/aea7269c45)] - **tools**: require function declarations (Rich Trott) [#12711](https://github.com/nodejs/node/pull/12711)
* [[`e7c3f4a97b`](https://github.com/nodejs/node/commit/e7c3f4a97b)] - **tools**: fix gyp to work on MacOSX without XCode (Shigeki Ohtsu) [iojs/io.js#1325](https://github.com/iojs/io.js/pull/1325)
* [[`a4b9c585b3`](https://github.com/nodejs/node/commit/a4b9c585b3)] - **tools**: enforce two arguments in assert.throws (Michaël Zasso) [#12270](https://github.com/nodejs/node/pull/12270)
* [[`b3f2e3b7e2`](https://github.com/nodejs/node/commit/b3f2e3b7e2)] - **tools**: replace custom assert.fail lint rule (Rich Trott) [#12287](https://github.com/nodejs/node/pull/12287)
* [[`8191af5b29`](https://github.com/nodejs/node/commit/8191af5b29)] - **tools**: replace custom new-with-error rule (Rich Trott) [#12249](https://github.com/nodejs/node/pull/12249)
* [[`61ebfa8d1f`](https://github.com/nodejs/node/commit/61ebfa8d1f)] - **tools**: add unescaped regexp dot rule to linter (Brian White) [#11834](https://github.com/nodejs/node/pull/11834)
* [[`20b18236de`](https://github.com/nodejs/node/commit/20b18236de)] - **tools**: add rule prefering common.mustNotCall() (James M Snell) [#12027](https://github.com/nodejs/node/pull/12027)
* [[`096508dfa9`](https://github.com/nodejs/node/commit/096508dfa9)] - **tools,lib**: enable strict equality lint rule (Rich Trott) [#12446](https://github.com/nodejs/node/pull/12446)
* [[`70cdfc5eb1`](https://github.com/nodejs/node/commit/70cdfc5eb1)] - **url**: expose WHATWG url.origin as ASCII (Timothy Gu) [#13126](https://github.com/nodejs/node/pull/13126)
* [[`06a617aa21`](https://github.com/nodejs/node/commit/06a617aa21)] - **url**: update IDNA error conditions (Rajaram Gaunker) [#12966](https://github.com/nodejs/node/pull/12966)
* [[`841bb4c61f`](https://github.com/nodejs/node/commit/841bb4c61f)] - **url**: fix C0 control and whitespace handling (Timothy Gu) [#12846](https://github.com/nodejs/node/pull/12846)
* [[`943dd5f9ed`](https://github.com/nodejs/node/commit/943dd5f9ed)] - **url**: handle windows drive letter in the file state (Daijiro Wachi) [#12808](https://github.com/nodejs/node/pull/12808)
* [[`8491c705b1`](https://github.com/nodejs/node/commit/8491c705b1)] - **url**: fix permanent deoptimizations (Brian White) [#12456](https://github.com/nodejs/node/pull/12456)
* [[`97ec72b76d`](https://github.com/nodejs/node/commit/97ec72b76d)] - **url**: refactor binding imports in internal/url (James M Snell) [#12717](https://github.com/nodejs/node/pull/12717)
* [[`b331ba6ca9`](https://github.com/nodejs/node/commit/b331ba6ca9)] - **url**: move to module.exports = {} pattern (James M Snell) [#12717](https://github.com/nodejs/node/pull/12717)
* [[`d457a986a0`](https://github.com/nodejs/node/commit/d457a986a0)] - **url**: port WHATWG URL API to internal/errors (Timothy Gu) [#12574](https://github.com/nodejs/node/pull/12574)
* [[`061c5da010`](https://github.com/nodejs/node/commit/061c5da010)] - **url**: use internal/util's getConstructorOf (Timothy Gu) [#12526](https://github.com/nodejs/node/pull/12526)
* [[`2841f478e4`](https://github.com/nodejs/node/commit/2841f478e4)] - **url**: improve WHATWG URL inspection (Timothy Gu) [#12253](https://github.com/nodejs/node/pull/12253)
* [[`aff5cc92b9`](https://github.com/nodejs/node/commit/aff5cc92b9)] - **url**: clean up WHATWG URL origin generation (Timothy Gu) [#12252](https://github.com/nodejs/node/pull/12252)
* [[`1b99d8ffe9`](https://github.com/nodejs/node/commit/1b99d8ffe9)] - **url**: disallow invalid IPv4 in IPv6 parser (Daijiro Wachi) [#12315](https://github.com/nodejs/node/pull/12315)
* [[`eb0492d51e`](https://github.com/nodejs/node/commit/eb0492d51e)] - **url**: remove javascript URL special case (Daijiro Wachi) [#12331](https://github.com/nodejs/node/pull/12331)
* [[`b470a85f07`](https://github.com/nodejs/node/commit/b470a85f07)] - **url**: trim leading slashes of file URL paths (Daijiro Wachi) [#12203](https://github.com/nodejs/node/pull/12203)
* [[`b76a350a19`](https://github.com/nodejs/node/commit/b76a350a19)] - **url**: avoid instanceof for WHATWG URL (Brian White) [#11690](https://github.com/nodejs/node/pull/11690)
* [[`c4469c49ec`](https://github.com/nodejs/node/commit/c4469c49ec)] - **url**: error when domainTo*() is called w/o argument (Timothy Gu) [#12134](https://github.com/nodejs/node/pull/12134)
* [[`f8f46f9917`](https://github.com/nodejs/node/commit/f8f46f9917)] - **url**: change path parsing for non-special URLs (Daijiro Wachi) [#12058](https://github.com/nodejs/node/pull/12058)
* [[`7139b93a8b`](https://github.com/nodejs/node/commit/7139b93a8b)] - **url**: add ToObject method to native URL class (James M Snell) [#12056](https://github.com/nodejs/node/pull/12056)
* [[`14a91957f8`](https://github.com/nodejs/node/commit/14a91957f8)] - **url**: use a class for WHATWG url\[context\] (Timothy Gu) [#11930](https://github.com/nodejs/node/pull/11930)
* [[`c515a985ea`](https://github.com/nodejs/node/commit/c515a985ea)] - **url**: spec-compliant URLSearchParams parser (Timothy Gu) [#11858](https://github.com/nodejs/node/pull/11858)
* [[`d77a7588cf`](https://github.com/nodejs/node/commit/d77a7588cf)] - **url**: spec-compliant URLSearchParams serializer (Timothy Gu) [#11626](https://github.com/nodejs/node/pull/11626)
* [[`99b27ce99a`](https://github.com/nodejs/node/commit/99b27ce99a)] - **url**: prioritize toString when stringifying (Timothy Gu) [#11737](https://github.com/nodejs/node/pull/11737)
* [[`b610a4db1c`](https://github.com/nodejs/node/commit/b610a4db1c)] - **url**: enforce valid UTF-8 in WHATWG parser (Timothy Gu) [#11436](https://github.com/nodejs/node/pull/11436)
* [[`147d2a6419`](https://github.com/nodejs/node/commit/147d2a6419)] - **url, test**: break up test-url.js (Joyee Cheung) [#11049](https://github.com/nodejs/node/pull/11049)
* [[`ef16319eff`](https://github.com/nodejs/node/commit/ef16319eff)] - **util**: fixup internal util exports (James M Snell) [#12998](https://github.com/nodejs/node/pull/12998)
* [[`d5925af8d7`](https://github.com/nodejs/node/commit/d5925af8d7)] - **util**: fix permanent deoptimizations (Brian White) [#12456](https://github.com/nodejs/node/pull/12456)
* [[`3c0dd45c88`](https://github.com/nodejs/node/commit/3c0dd45c88)] - **util**: move getConstructorOf() to internal (Timothy Gu) [#12526](https://github.com/nodejs/node/pull/12526)
* [[`a37273c1e4`](https://github.com/nodejs/node/commit/a37273c1e4)] - **util**: use V8 C++ API for inspecting Promises (Timothy Gu) [#12254](https://github.com/nodejs/node/pull/12254)
* [[`c8be718749`](https://github.com/nodejs/node/commit/c8be718749)] - **v8**: backport pieces from 18a26cfe174 from upstream v8 (Peter Marshall) [#13217](https://github.com/nodejs/node/pull/13217)
* [[`cfdcd6cf33`](https://github.com/nodejs/node/commit/cfdcd6cf33)] - **v8**: backport 43791ce02c8 from upstream v8 (kozyatinskiy) [#13217](https://github.com/nodejs/node/pull/13217)
* [[`1061e43739`](https://github.com/nodejs/node/commit/1061e43739)] - **v8**: backport faf5f52627c from upstream v8 (Peter Marshall) [#13217](https://github.com/nodejs/node/pull/13217)
* [[`a56f9698cb`](https://github.com/nodejs/node/commit/a56f9698cb)] - **v8**: backport 4f82f1d948c from upstream v8 (hpayer) [#13217](https://github.com/nodejs/node/pull/13217)
* [[`13a961e9dc`](https://github.com/nodejs/node/commit/13a961e9dc)] - **v8**: backport 4f82f1d948c from upstream v8 (hpayer) [#13217](https://github.com/nodejs/node/pull/13217)
* [[`188630b84c`](https://github.com/nodejs/node/commit/188630b84c)] - **v8**: backport a9e56f4f36d from upstream v8 (Peter Marshall) [#13217](https://github.com/nodejs/node/pull/13217)
* [[`0f3bfaf530`](https://github.com/nodejs/node/commit/0f3bfaf530)] - **v8**: backport bd59e7452be from upstream v8 (Michael Achenbach) [#13217](https://github.com/nodejs/node/pull/13217)
* [[`6d5ca4feb0`](https://github.com/nodejs/node/commit/6d5ca4feb0)] - **v8**: backport pieces of dab18fb0bbcdd (Anna Henningsen) [#12875](https://github.com/nodejs/node/pull/12875)
* [[`62eaa2a186`](https://github.com/nodejs/node/commit/62eaa2a186)] - **v8**: do not test v8 with -Werror (Anna Henningsen) [#12875](https://github.com/nodejs/node/pull/12875)
* [[`f118f7ae90`](https://github.com/nodejs/node/commit/f118f7ae90)] - **v8**: backport header diff from 2e4a68733803 (Anna Henningsen) [#12875](https://github.com/nodejs/node/pull/12875)
* [[`a947cf9a03`](https://github.com/nodejs/node/commit/a947cf9a03)] - **v8**: backport header diff from 94283dcf4459f (Anna Henningsen) [#12875](https://github.com/nodejs/node/pull/12875)
* [[`1bb880b595`](https://github.com/nodejs/node/commit/1bb880b595)] - **v8**: backport pieces of bf463c4dc0 and dc662e5b74 (Anna Henningsen) [#12875](https://github.com/nodejs/node/pull/12875)
* [[`04e646be52`](https://github.com/nodejs/node/commit/04e646be52)] - **v8**: backport header diff from da5b745dba387 (Anna Henningsen) [#12875](https://github.com/nodejs/node/pull/12875)
* [[`39834bc441`](https://github.com/nodejs/node/commit/39834bc441)] - **v8**: backport pieces of 6226576efa82ee (Anna Henningsen) [#12875](https://github.com/nodejs/node/pull/12875)
* [[`25430fd247`](https://github.com/nodejs/node/commit/25430fd247)] - **v8**: backport pieces from 99743ad460e (Anna Henningsen) [#12875](https://github.com/nodejs/node/pull/12875)
* [[`0f3e69db41`](https://github.com/nodejs/node/commit/0f3e69db41)] - **v8**: fix gcc 7 build errors (Zuzana Svetlikova) [#12676](https://github.com/nodejs/node/pull/12676)
* [[`b07e1a828c`](https://github.com/nodejs/node/commit/b07e1a828c)] - **v8**: fix gcc 7 build errors (Zuzana Svetlikova) [#12676](https://github.com/nodejs/node/pull/12676)
* [[`1052383f7c`](https://github.com/nodejs/node/commit/1052383f7c)] - **v8**: refactor struture of v8 module (James M Snell) [#12681](https://github.com/nodejs/node/pull/12681)
* [[`33a19b46ca`](https://github.com/nodejs/node/commit/33a19b46ca)] - **v8**: fix offsets for TypedArray deserialization (Anna Henningsen) [#12143](https://github.com/nodejs/node/pull/12143)
* [[`6b25c75cda`](https://github.com/nodejs/node/commit/6b25c75cda)] - **vm**: fix race condition with timeout param (Marcel Laverdet) [#13074](https://github.com/nodejs/node/pull/13074)
* [[`191bb5a358`](https://github.com/nodejs/node/commit/191bb5a358)] - **vm**: fix displayErrors in runIn.. functions (Marcel Laverdet) [#13074](https://github.com/nodejs/node/pull/13074)
* [[`1c93e8c94b`](https://github.com/nodejs/node/commit/1c93e8c94b)] - **win**: make buildable on VS2017 (Refael Ackermann) [#11852](https://github.com/nodejs/node/pull/11852)
* [[`ea01cd7adb`](https://github.com/nodejs/node/commit/ea01cd7adb)] - **zlib**: remove unused declaration (Anna Henningsen) [#12432](https://github.com/nodejs/node/pull/12432)
