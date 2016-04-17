### Pull Request check-list

_Please make sure to review and check all of these items:_

- [ ] Have you enabled debug builds by passing `--debug` to `./configure`?
- [ ] Have you run the test binaries under `out/{Debug,Release}/` after running
  `make -k`? Do they pass?
- [ ] If this is a change to spidershim, does the commit message begin with
  "spidershim: "?  Does it explain what the commit does?
- [ ] If this change fixes a bug (or a performance problem), is a regression
  test (or a benchmark) included?
- [ ] Is a documentation update included (if this change modifies
  existing APIs, or introduces new ones)?

_NOTE: these things are not required to open a PR and can be done
afterwards / while the PR is open._

### Description of change

_Please provide a description of the change here._
