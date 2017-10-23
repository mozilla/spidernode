# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# An easy way for distribution-specific bootstrappers to share the code
# needed to install Stylo dependencies.  This class must come before
# BaseBootstrapper in the inheritance list.

from __future__ import absolute_import


class StyloInstall(object):
    def __init__(self, **kwargs):
        pass

    def ensure_stylo_packages(self, state_dir, checkout_root):
        from mozboot import stylo
        self.install_toolchain_artifact(state_dir, checkout_root, stylo.LINUX)
