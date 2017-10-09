# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# /!\ Please make sure to update the following comment when you touch this
# file. Thank you /!\

# The traditional Mozilla build system relied on going through the entire
# build tree a number of times with different targets, and many of the
# things happening at each step required other things happening in previous
# steps without any documentation of those dependencies.
#
# This new build system tries to start afresh by establishing what files or
# operations are needed for the build, and applying the necessary rules to
# have those in place, relying on make dependencies to get them going.
#
# As of writing, only building non-compiled parts of Firefox is supported
# here (a few other things are also left out). This is a starting point, with
# the intent to grow this build system to make it more complete.
#
# This file contains rules and dependencies to get things working. The intent
# is for a Makefile to define some dependencies and variables, and include
# this file. What needs to be defined there, and ends up being generated by
# python/mozbuild/mozbuild/backend/fastermake.py is the following:
# - TOPSRCDIR/TOPOBJDIR, respectively the top source directory and the top
#   object directory
# - PYTHON, the path to the python executable
# - ACDEFINES, which contains a set of -Dvar=name to be used during
#   preprocessing
# - INSTALL_MANIFESTS, which defines the list of base directories handled
#   by install manifests, see further below
#
# A convention used between this file and the Makefile including it is that
# global Make variables names are uppercase, while "local" Make variables
# applied to specific targets are lowercase.

# Targets to be triggered for a default build
default: $(addprefix install-,$(INSTALL_MANIFESTS))

ifndef NO_XPIDL
# Targets from the recursive make backend to be built for a default build
default: $(TOPOBJDIR)/config/makefiles/xpidl/xpidl
endif

# Mac builds require to copy things in dist/bin/*.app
# TODO: remove the MOZ_WIDGET_TOOLKIT and MOZ_BUILD_APP variables from
# faster/Makefile and python/mozbuild/mozbuild/test/backend/test_build.py
# when this is not required anymore.
# We however don't need to do this when using the hybrid
# FasterMake/RecursiveMake backend (FASTER_RECURSIVE_MAKE is set when
# recursing from the RecursiveMake backend)
ifndef FASTER_RECURSIVE_MAKE
ifeq (cocoa,$(MOZ_WIDGET_TOOLKIT))
default:
	$(MAKE) -C $(TOPOBJDIR)/$(MOZ_BUILD_APP)/app repackage
endif
endif

.PHONY: FORCE

# Extra define to trigger some workarounds. We should strive to limit the
# use of those. As of writing the only ones are in
# toolkit/content/buildconfig.html and browser/locales/jar.mn.
ACDEFINES += -DBUILD_FASTER

# Files under the faster/ sub-directory, however, are not meant to use the
# fallback
$(TOPOBJDIR)/faster/%: ;

# Generic rule to fall back to the recursive make backend.
# This needs to stay after other $(TOPOBJDIR)/* rules because GNU Make
# <3.82 apply pattern rules in definition order, not stem length like
# modern GNU Make.
$(TOPOBJDIR)/%: FORCE
	$(MAKE) -C $(dir $@) $(notdir $@)

# Install files using install manifests
#
# The list of base directories is given in INSTALL_MANIFESTS. The
# corresponding install manifests are named correspondingly, with forward
# slashes replaced with underscores, and prefixed with `install_`. That is,
# the install manifest for `dist/bin` would be `install_dist_bin`.
$(addprefix install-,$(INSTALL_MANIFESTS)): install-%: $(addprefix $(TOPOBJDIR)/,buildid.h source-repo.h)
	@# For now, force preprocessed files to be reprocessed every time.
	@# The overhead is not that big, and this avoids waiting for proper
	@# support for defines tracking in process_install_manifest.
	@touch install_$(subst /,_,$*)
	@# BOOKMARKS_INCLUDE_DIR is for bookmarks.html only.
	$(PYTHON) -m mozbuild.action.process_install_manifest \
		--track install_$(subst /,_,$*).track \
		$(TOPOBJDIR)/$* \
		-DAB_CD=en-US \
		-DBOOKMARKS_INCLUDE_DIR=$(TOPSRCDIR)/browser/locales/en-US/profile \
		$(ACDEFINES) \
		install_$(subst /,_,$*)

# ============================================================================
# Below is a set of additional dependencies and variables used to build things
# that are not supported by data in moz.build.

# The xpidl target in config/makefiles/xpidl requires the install manifest for
# dist/idl to have been processed. When using the hybrid
# FasterMake/RecursiveMake backend, this dependency is handled in the top-level
# Makefile.
ifndef FASTER_RECURSIVE_MAKE
$(TOPOBJDIR)/config/makefiles/xpidl/xpidl: $(TOPOBJDIR)/install-dist_idl
endif
# It also requires all the install manifests for dist/bin to have been processed
# because it adds interfaces.manifest references with buildlist.py.
$(TOPOBJDIR)/config/makefiles/xpidl/xpidl: $(addprefix install-,$(filter dist/bin%,$(INSTALL_MANIFESTS)))

$(TOPOBJDIR)/build/application.ini: $(TOPOBJDIR)/buildid.h $(TOPOBJDIR)/source-repo.h

# The manifest of allowed system add-ons should be re-built when using
# "build faster".
ifeq ($(MOZ_BUILD_APP),browser/app)
default: $(TOPOBJDIR)/browser/app/features
endif
