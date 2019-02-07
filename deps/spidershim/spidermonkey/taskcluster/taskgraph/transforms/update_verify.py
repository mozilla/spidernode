# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the beetmover task into an actual task description.
"""

from __future__ import absolute_import, print_function, unicode_literals

from copy import deepcopy

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.schema import resolve_keyed_by
from taskgraph.util.treeherder import add_suffix

transforms = TransformSequence()


@transforms.add
def add_command(config, tasks):
    for task in tasks:
        total_chunks = task["extra"]["chunks"]

        for this_chunk in range(1, total_chunks+1):
            chunked = deepcopy(task)
            chunked["treeherder"]["symbol"] = add_suffix(
                chunked["treeherder"]["symbol"], this_chunk)
            chunked["label"] = "release-update-verify-{}-{}/{}".format(
                chunked["name"], this_chunk, total_chunks
            )
            if not chunked["worker"].get("env"):
                chunked["worker"]["env"] = {}
            chunked["run"] = {
                'using': 'run-task',
                'command': 'cd /builds/worker/checkouts/gecko && '
                           'tools/update-verify/scripts/chunked-verify.sh '
                           '{} {}'.format(
                               total_chunks,
                               this_chunk,
                           ),
                'sparse-profile': 'update-verify',
            }
            for thing in ("CHANNEL", "VERIFY_CONFIG", "BUILD_TOOLS_REPO"):
                thing = "worker.env.{}".format(thing)
                resolve_keyed_by(
                    chunked, thing, thing,
                    **{
                        'project': config.params['project'],
                        'release-type': config.params['release_type'],
                    }
                )

            for upstream in chunked.get("dependencies", {}).keys():
                if 'update-verify-config' in upstream:
                    chunked.setdefault('fetches', {})[upstream] = [
                        "update-verify.cfg",
                    ]
                    break
            else:
                raise Exception("Couldn't find upate verify config")

            yield chunked
