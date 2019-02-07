# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Apply some defaults and minor modifications to the pgo jobs.
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.base import TransformSequence

import logging
logger = logging.getLogger(__name__)

transforms = TransformSequence()


@transforms.add
def run_profile_data(config, jobs):
    for job in jobs:
        instr = 'instrumented-build-{}'.format(job['name'])
        job.setdefault('fetches', {})[instr] = ['target.tar.bz2']
        yield job
