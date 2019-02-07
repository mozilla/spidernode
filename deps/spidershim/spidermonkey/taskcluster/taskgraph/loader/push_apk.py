# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from .transform import loader as base_loader


def loader(kind, path, config, params, loaded_tasks):
    """
    Generate inputs implementing PushApk jobs. These depend on signed multi-locales nightly builds.
    """
    jobs = base_loader(kind, path, config, params, loaded_tasks)

    for job in jobs:
        dependent_tasks = get_dependent_loaded_tasks(config, params, loaded_tasks)
        if not dependent_tasks:
            # PushApk must depend on signed APK. If no dependent task was found,
            # this means another plaform (like windows) is being processed
            continue

        job['dependent-tasks'] = dependent_tasks
        job['label'] = job['name']

        yield job


def get_dependent_loaded_tasks(config, params, loaded_tasks):
    nightly_tasks = (
        task for task in loaded_tasks if task.attributes.get('nightly')
    )
    tasks_with_matching_kind = (
        task for task in nightly_tasks if task.kind in config.get('kind-dependencies')
    )
    android_tasks = (
        task for task in tasks_with_matching_kind
        if task.attributes.get('build_platform', '').startswith('android')
    )

    # TODO Bug 1368484: Aarch64 is not planned to ride the trains regularly. It may stay on nightly
    # for a few cycles. Then, we should activate it on beta then release, once ready.
    aarch64_tasks_only_on_central = (
        task for task in android_tasks
        if params['project'] == 'mozilla-central' or
        'aarch64' not in task.attributes.get('build_platform', '')
    )

    # TODO Bug 1490502: Activate x86-64 once ready
    non_shipping_tasks = [
        task for task in aarch64_tasks_only_on_central
        if 'x86_64' not in task.attributes.get('build_platform', '')
    ]

    return non_shipping_tasks
