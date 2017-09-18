# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""Pytest fixtures to help set up Firefox and a tests.zip
in test harness selftests.
"""

import os
import shutil
import sys

import mozfile
import mozinstall
import pytest
import requests
from mozbuild.base import MozbuildObject

here = os.path.abspath(os.path.dirname(__file__))
build = MozbuildObject.from_environment(cwd=here)


HARNESS_ROOT_NOT_FOUND = """
Could not find test harness root. Either a build or the 'GECKO_INSTALLER_URL'
environment variable is required.
""".lstrip()


def _get_test_harness(suite, install_dir):
    # Check if there is a local build
    harness_root = os.path.join(build.topobjdir, '_tests', install_dir)
    if os.path.isdir(harness_root):
        return harness_root

    # Check if it was previously set up by another test
    harness_root = os.path.join(os.environ['PYTHON_TEST_TMP'], 'tests', suite)
    if os.path.isdir(harness_root):
        return harness_root

    # Check if there is a test package to download
    if 'GECKO_INSTALLER_URL' in os.environ:
        base_url = os.environ['GECKO_INSTALLER_URL'].rsplit('/', 1)[0]
        test_packages = requests.get(base_url + '/target.test_packages.json').json()

        dest = os.path.join(os.environ['PYTHON_TEST_TMP'], 'tests')
        for name in test_packages[suite]:
            url = base_url + '/' + name
            bundle = os.path.join(os.environ['PYTHON_TEST_TMP'], name)

            r = requests.get(url, stream=True)
            with open(bundle, 'w+b') as fh:
                for chunk in r.iter_content(chunk_size=1024):
                    fh.write(chunk)

            mozfile.extract(bundle, dest)

        return os.path.join(dest, suite)

    # Couldn't find a harness root, let caller do error handling.
    return None


@pytest.fixture(scope='session')
def setup_test_harness(request):
    """Fixture for setting up a mozharness-based test harness like
    mochitest or reftest"""
    def inner(files_dir, *args, **kwargs):
        harness_root = _get_test_harness(*args, **kwargs)
        if harness_root:
            sys.path.insert(0, harness_root)

            # Link the test files to the test package so updates are automatically
            # picked up. Fallback to copy on Windows.
            if files_dir:
                test_root = os.path.join(harness_root, 'tests', 'selftests')
                if not os.path.exists(test_root):
                    if hasattr(os, 'symlink'):
                        os.symlink(files_dir, test_root)
                    else:
                        shutil.copytree(files_dir, test_root)

        elif 'GECKO_INSTALLER_URL' in os.environ:
            # The mochitest tests will run regardless of whether a build exists or not.
            # In a local environment, they should simply be skipped if setup fails. But
            # in automation, we'll need to make sure an error is propagated up.
            pytest.fail(HARNESS_ROOT_NOT_FOUND)
        else:
            # Tests will be marked skipped by the calls to pytest.importorskip() below.
            # We are purposefully not failing here because running |mach python-test|
            # without a build is a perfectly valid use case.
            pass
    return inner


@pytest.fixture(scope='session')
def binary():
    """Return a Firefox binary"""
    try:
        return build.get_binary_path()
    except:
        pass

    app = 'firefox'
    bindir = os.path.join(os.environ['PYTHON_TEST_TMP'], app)
    if os.path.isdir(bindir):
        try:
            return mozinstall.get_binary(bindir, app_name=app)
        except:
            pass

    if 'GECKO_INSTALLER_URL' in os.environ:
        bindir = mozinstall.install(
            os.environ['GECKO_INSTALLER_URL'], os.environ['PYTHON_TEST_TMP'])
        return mozinstall.get_binary(bindir, app_name='firefox')
