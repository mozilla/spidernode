# Onboarding

This document is an outline of the things we tell new Collaborators at their
onboarding session.

## One week before the onboarding session

* Ask the new Collaborator if they are using two-factor authentication on their
  GitHub account. If they are not, suggest that they enable it as their account
  will have elevated privileges in many of the Node.js repositories.

## Fifteen minutes before the onboarding session

* Prior to the onboarding session, add the new Collaborators to
[the Collaborators team](https://github.com/orgs/nodejs/teams/collaborators).

## **thank you** for doing this

  * going to cover four things:
    * local setup
    * some project goals & values
    * issues, labels, and reviewing code
    * merging code

## setup

  * notifications setup
    * use https://github.com/notifications or set up email
    * watching the main repo will flood your inbox, so be prepared


  * git:
    * make sure you have whitespace=fix: `git config --global --add core.whitespace fix`
    * usually PR from your own github fork
    * [**See "Updating Node.js from Upstream"**](./onboarding-extras.md#updating-nodejs-from-upstream)
    * make new branches for all commits you make!


  * `#node-dev` on `chat.freenode.net` is the best place to interact with the CTC / other collaborators


## a little deeper about the project

  * collaborators are effectively part owners
    * the project has the goals of its contributors


  * but, there are some higher-level goals and values
    * not everything belongs in core (if it can be done reasonably in userland, let it stay in userland)
    * empathy towards users matters (this is in part why we onboard people)
    * generally: try to be nice to people


## managing the issue tracker

  * you have (mostly) free rein – don't hesitate to close an issue if you are confident that it should be closed
    * this will come more naturally over time
    * IMPORTANT: be nice about closing issues, let people know why, and that issues and PRs can be reopened if necessary
    * Still need to follow the Code of Conduct.


  * Labels:
    * There is [a bot](https://github.com/nodejs-github-bot/github-bot) that applies subsystem labels (for example, `doc`, `test`, `assert`, or `buffer`) so that we know what parts of the code base the pull request modifies. It is not perfect, of course. Feel free to apply relevant labels and remove irrelevant labels from pull requests and issues.
    * [**See "Labels"**](./onboarding-extras.md#labels)
    * Use the `ctc-review` label if a topic is controversial or isn't coming to
      a conclusion after an extended time.
    * `semver-{minor,major}`:
      * If a change has the remote *chance* of breaking something, use `semver-major`
      * When adding a semver label, add a comment explaining why you're adding it. Do it right away so you don't forget!

  * Notifying humans
    * [**See "Who to CC in issues"**](./onboarding-extras.md#who-to-cc-in-issues)
    * will also come more naturally over time


  * Reviewing:
    * The primary goal is for the codebase to improve.
    * Secondary (but not far off) is for the person submitting code to succeed.
      A pull request from a new contributor is an opportunity to grow the
      community.
    * Review a bit at a time. Do not overwhelm new contributors.
      * It is tempting to micro-optimize and make everything about relative
        performance. Don't succumb to that temptation. We change V8 often.
        Techniques that provide improved performance today may be unnecessary in
        the future.
    * Be aware: Your opinion carries a lot of weight!
    * Nits (requests for small changes that are not essential) are fine, but try
      to avoid stalling the pull request.
      * Note that they are nits when you comment: `Nit: change foo() to bar().`
      * If they are stalling the pull request, fix them yourself on merge.
    * Minimum wait for comments time
      * There is a minimum waiting time which we try to respect for non-trivial
        changes, so that people who may have important input in such a
        distributed project are able to respond.
      * For non-trivial changes, leave the pull request open for at least 48
        hours (72 hours on a weekend).
      * If a pull request is abandoned, check if they'd mind if you took it over
        (especially if it just has nits left).
    * Approving a change
      * Collaborators indicate that they have reviewed and approve of the
        the changes in a pull request by commenting with `LGTM`, which stands
        for "looks good to me".
      * You have the power to `LGTM` another collaborator's (including TSC/CTC
        members) work.
      * You may not `LGTM` your own pull requests.
      * You have the power to `LGTM` anyone else's pull requests.


  * what belongs in node:
    * opinions vary, but I find the following helpful:
    * if node itself needs it (due to historic reasons), then it belongs in node
      * that is to say, url is there because of http, freelist is there because of http, et al
    * also, things that cannot be done outside of core, or only with significant pain (example: async-wrap)


  * Continuous Integration (CI) Testing:
    * https://ci.nodejs.org/
      * It is not automatically run. You need to start it manually.
    * Log in on CI is integrated with GitHub. Try to log in now!
    * You will be using `node-test-pull-request` most of the time. Go there now!
      * Consider bookmarking it: https://ci.nodejs.org/job/node-test-pull-request/
    * To get to the form to start a job, click on `Build with Parameters`. (If you don't see it, that probably means you are not logged in!) Click it now!
    * To start CI testing from this screen, you need to fill in two elements on the form:
      * The `CERTIFY_SAFE` box should be checked. By checking it, you are indicating that you have reviewed the code you are about to test and you are confident that it does not contain any malicious code. (We don't want people hijacking our CI hosts to attack other hosts on the internet, for example!)
      * The `PR_ID` box should be filled in with the number identifying the pull request containing the code you wish to test. For example, if the URL for the pull request is `https://github.com/nodejs/node/issues/7006`, then put `7006` in the `PR_ID`.
      * The remaining elements on the form are typically unchanged with the exception of `POST_STATUS_TO_PR`. Check that if you want a CI status indicator to be automatically inserted into the PR.
    * If you need help with something CI-related:
      * Use #node-dev (IRC) to talk to other Collaborators.
      * Use #node-build (IRC) to talk to the Build WG members who maintain the CI infrastructure.
      * Use the [Build WG repo](https://github.com/nodejs/build) to file issues for the Build WG members who maintain the CI infrastructure.


## Landing PRs: Overview

  * The [Collaborator Guide](https://github.com/nodejs/node/blob/master/COLLABORATOR_GUIDE.md#technical-howto) is a great resource.


  * No one (including TSC or CTC members) pushes directly to master without review.
    * An exception is made for release commits only.


  * One `LGTM` is sufficient, except for semver-major changes.
    * More than one is better.
    * Breaking changes must be LGTM'ed by at least two CTC members.
    * If one or more Collaborators object to a change, it should not land until
    the objection is addressed. The options for such a situation include:
      * Engaging those with objections to determine a viable path forward;
      * Altering the pull request to address the objections;
      * Escalating the discussion to the CTC using the `ctc-review` label. This
        should only be done after the previous options have been exhausted.

  * Wait before merging non-trivial changes.
    * 48 hours during the week and 72 hours on weekends.
    * An example of a trivial change would be correcting the misspelling of a single word in a documentation file. This sort of change still needs to receive at least one `LGTM` but it does not need to wait 48 hours before landing.

  * **Run the PR through CI before merging!**
    * An exception can be made for documentation-only PRs as long as it does not include the `addons.md` documentation file. (Example code from that document is extracted and built as part of the tests!)

  * What if something goes wrong?
    * Ping a CTC member.
    * `#node-dev` on freenode
    * Force-pushing to fix things after is allowed for ~10 minutes. Avoid it if you can.
      * Use `--force-with-lease` to minimize the chance of overwriting someone else's change.
      * Post to `#node-dev` (IRC) if you force push.


## Landing PRs: Details

* Please never use GitHub's green ["Merge Pull Request"](https://help.github.com/articles/merging-a-pull-request/#merging-a-pull-request-using-the-github-web-interface) button.
  * If you do, please force-push removing the merge.
  * Reasons for not using the web interface button:
    * The merge method will add an unnecessary merge commit.
    * The rebase & merge method adds metadata to the commit title.
    * The rebase method changes the author.
    * The squash & merge method has been known to add metadata to the commit title.
    * If more than one author has contributed to the PR, only the latest author will be considered during the squashing.


Update your `master` branch (or whichever branch you are landing on, almost always `master`)

* [**See "Updating Node.js from Upstream"**](./onboarding-extras.md#updating-nodejs-from-upstream)

Landing a PR

* If it all looks good, `curl -L 'url-of-pr.patch' | git am`
  * If `git am` fails, see [the relevant section of the Onboarding Extras doc](./onboarding-extras.md#if-git-am-fails).
* `git rebase -i upstream/master`
* Squash into logical commits if necessary.
* `./configure && make -j8 test` (`-j8` builds node in parallel with 8 threads. adjust to the number of cores (or processor-level threads) your processor has (or slightly more) for best results.)
* Amend the commit description.
  * The commit message text must conform to the [commit message guidelines](../CONTRIBUTING.md#step-3-commit).
  * Add required metadata:
    * `PR-URL: <full-pr-url>`
    * `Reviewed-By: <collaborator name> <collaborator email>`
      * Easiest to use `git log`, then do a search.
      * In vim: `/Name` + `enter` (+ `n` as much as you need to)
      * Only include collaborators who have commented `LGTM`.
  * Add additional metadata as appropriate:
    * `Fixes: <full-issue-url>`
      * Full URL of GitHub issue that the PR fixes.
      * This will automatically close the PR when the commit lands in master.
    * `Refs: <full-url>`
      * Full URL of material that might provide additional useful information or context to someone trying to understand the change set or the thinking behind it.
* Optional: Force push the amended commit to the branch you used to open the pull request. If your branch is called `bugfix`, then the command would be `git push --force-with-lease origin master:bugfix`. When the pull request is closed, this will cause the pull request to show the purple merged status rather than the red closed status that is usually used for pull requests that weren't merged. Only do this when landing your own contributions.
* `git push upstream master`
    * Close the pull request with a "Landed in `<commit hash>`" comment.


## Exercise: Make a PR adding yourself to the README

  * Example: https://github.com/nodejs/node/commit/7b09aade8468e1c930f36b9c81e6ac2ed5bc8732
    * For raw commit message: `git log 7b09aade8468e1c930f36b9c81e6ac2ed5bc8732 -1`
  * Collaborators are in alphabetical order by GitHub username.
  * Label your pull request with the `doc` subsystem label.
  * Run CI on your PR.
  * After a `LGTM` or two, land the PR.
    * Be sure to add the `PR-URL: <full-pr-url>` and appropriate `Reviewed-By:` metadata!


## final notes

  * don't worry about making mistakes: everybody makes them, there's a lot to internalize and that takes time (and we recognize that!)
  * very few (no?) mistakes are unrecoverable
  * the existing node committers trust you and are grateful for your help!
  * other repos:
    * https://github.com/nodejs/dev-policy
    * https://github.com/nodejs/NG
    * https://github.com/nodejs/api
    * https://github.com/nodejs/build
    * https://github.com/nodejs/docs
    * https://github.com/nodejs/nodejs.org
    * https://github.com/nodejs/readable-stream
    * https://github.com/nodejs/LTS
