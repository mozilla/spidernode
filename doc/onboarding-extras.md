# Additional Onboarding Information

## Who to CC in issues

| subsystem | maintainers |
| --- | --- |
| `benchmark/*` | @nodejs/benchmarking, @mscdex |
| `bootstrap_node.js` | @fishrock123 |
| `doc/*`, `*.md` | @nodejs/documentation |
| `lib/assert` | @nodejs/testing |
| `lib/buffer` | @nodejs/buffer |
| `lib/child_process` | @bnoordhuis, @cjihrig |
| `lib/cluster` | @bnoordhuis, @cjihrig, @mcollina |
| `lib/{crypto,tls,https}` | @nodejs/crypto |
| `lib/dgram` | @cjihrig, @mcollina |
| `lib/domains` | @misterdjules |
| `lib/fs`, `src/{fs,file}` | @nodejs/fs |
| `lib/{_}http{*}` | @nodejs/http |
| `lib/inspector.js`, `src/inspector_*` | @nodejs/v8-inspector |
| `lib/internal/url`, `src/node_url` | @nodejs/url |
| `lib/net` | @bnoordhuis, @indutny, @nodejs/streams |
| `lib/repl` | @addaleax, @fishrock123 |
| `lib/{_}stream{*}` | @nodejs/streams |
| `lib/timers` | @fishrock123, @misterdjules |
| `lib/util` | @bnoordhuis, @cjihrig, @evanlucas |
| `lib/zlib` | @addaleax, @bnoordhuis, @indutny |
| `src/async-wrap.*` | @nodejs/async_hooks |
| `src/node_api.*` | @nodejs/n-api |
| `src/node_crypto.*` | @nodejs/crypto |
| `test/*` | @nodejs/testing |
| `tools/eslint`, `.eslintrc` | @not-an-aardvark, @silverwind, @trott |
| async_hooks | @nodejs/async_hooks for bugs/reviews (+ @nodejs/diagnostics for API) |
| performance | @nodejs/performance |
| platform specific | @nodejs/platform-{aix,arm,freebsd,macos,ppc,smartos,s390,windows} |
| python code | @nodejs/python |
| upgrading c-ares | @jbergstroem |
| upgrading http-parser | @jbergstroem, @nodejs/http |
| upgrading libuv | @saghul |
| upgrading npm | @fishrock123, @MylesBorins |
| upgrading V8 | @nodejs/v8, @nodejs/post-mortem |

When things need extra attention, are controversial, or `semver-major`: @nodejs/ctc

If you cannot find who to cc for a file, `git shortlog -n -s <file>` may help.


## Labels

### By Subsystem

We generally sort issues by a concept of "subsystem" so that we know what part(s) of the codebase it touches.

**Subsystems generally are**:

* `lib/*.js`
* `doc`, `build`, `tools`, `test`, `deps`, `lib / src` (special), and there may be others.
* `meta` for anything non-code (process) related

There may be more than one subsystem valid for any particular issue / PR.


### General

Please use these when possible / appropriate

* `confirmed-bug` - Bugs you have verified exist
* `discuss` - Things that need larger discussion
* `feature request` - Any issue that requests a new feature (usually not PRs)
* `good first contribution` - Issues suitable for newcomers to process

--

* `semver-{minor,major}`
  * be conservative – that is, if a change has the remote *chance* of breaking something, go for semver-major
  * when adding a semver label, add a comment explaining why you're adding it
  * minor vs. patch: roughly: "does it add a new method / does it add a new section to the docs"
  * major vs. everything else: run last versions tests against this version, if they pass, **probably** minor or patch
  * A breaking change helper ([full source](https://gist.github.com/chrisdickinson/ba532fa0e4e243fb7b44)):
  ```sh
  git checkout $(git show -s --pretty='%T' $(git show-ref -d $(git describe --abbrev=0) | tail -n1 | awk '{print $1}')) -- test; make -j4 test
  ```


### Other Labels

* Operating system labels
  * `macos`, `windows`, `smartos`, `aix`
  * No linux, linux is the implied default
* Architecture labels
  * `arm`, `mips`, `s390`, `ppc`
  * No x86{_64}, since that is the implied default
* `lts-agenda`, `lts-watch-v*`
  * tag things that should be discussed to go into LTS or should go into a specific LTS branch
  * (usually only semver-patch things)
  * will come more naturally over time


## Updating Node.js from Upstream

* `git remote add upstream git://github.com/nodejs/node.git`

to update from nodejs/node:
* `git checkout master`
* `git remote update -p` OR `git fetch --all` (I prefer the former)
* `git merge --ff-only upstream/master` (or `REMOTENAME/BRANCH`)


## best practices

* commit often, out to your github fork (origin), open a PR
* when making PRs make sure to spend time on the description:
  * every moment you spend writing a good description quarters the amount of time it takes to understand your code.
* usually prefer to only squash at the *end* of your work, depends on the change
