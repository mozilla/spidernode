#!/bin/bash

set -xe

# Things to be set by task definition.
# -b branch
# -p pipfile_directory
# -3 use python3


test "${BRANCH}"
test "${PIPFILE_DIRECTORY}"

PIP_ARG="-2"
if [ -n "${PYTHON3}" ]; then
  PIP_ARG="-3"
fi

export ARTIFACTS_DIR="/home/worker/artifacts"
mkdir -p "$ARTIFACTS_DIR"

# duplicate the functionality of taskcluster-lib-urls, but in bash..
if [ "$TASKCLUSTER_ROOT_URL" = "https://taskcluster.net" ]; then
    queue_base='https://queue.taskcluster.net/v1'
else
    queue_base="$TASKCLUSTER_ROOT_URL/api/queue/v1"
fi

# Get Arcanist API token

if [ -n "${TASK_ID}" ]
then
  curl --location --retry 10 --retry-delay 10 -o /home/worker/task.json "$queue_base/task/$TASK_ID"
  ARC_SECRET=$(jq -r '.scopes[] | select(contains ("arc-phabricator-token"))' /home/worker/task.json | awk -F: '{print $3}')
fi
if [ -n "${ARC_SECRET}" ] && getent hosts taskcluster
then
  set +x # Don't echo these
  # Until bug 1460015 is finished, use the old, baseUrl-style proxy URLs
  secrets_url="${TASKCLUSTER_PROXY_URL}/secrets/v1/secret/${ARC_SECRET}"
  SECRET=$(curl "${secrets_url}")
  TOKEN=$(echo "${SECRET}" | jq -r '.secret.token')
elif [ -n "${ARC_TOKEN}" ] # Allow for local testing.
then
  TOKEN="${ARC_TOKEN}"
fi

if [ -n "${TOKEN}" ]
then
  cat >"${HOME}/.arcrc" <<END
{
  "hosts": {
    "https://phabricator.services.mozilla.com/api/": {
      "token": "${TOKEN}"
    }
  }
}
END
  set -x
  chmod 600 "${HOME}/.arcrc"
fi

# shellcheck disable=SC2086
/home/worker/scripts/update_pipfiles.sh -b "${BRANCH}" -p "${PIPFILE_DIRECTORY}" ${PIP_ARG}
