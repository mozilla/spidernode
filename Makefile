-include config.mk

BUILDTYPE ?= Release
PYTHON ?= python
DESTDIR ?=
SIGN ?=
PREFIX ?= /usr/local
FLAKY_TESTS ?= run
TEST_CI_ARGS ?=
STAGINGSERVER ?= node-www
LOGLEVEL ?= silent
OSTYPE := $(shell uname -s | tr '[A-Z]' '[a-z]')

ifdef JOBS
  PARALLEL_ARGS = -j $(JOBS)
endif

ifdef QUICKCHECK
  QUICKCHECK_ARG := --quickcheck
endif

ifdef ENABLE_V8_TAP
  TAP_V8 := --junitout $(PWD)/v8-tap.xml
  TAP_V8_INTL := --junitout $(PWD)/v8-intl-tap.xml
  TAP_V8_BENCHMARKS := --junitout $(PWD)/v8-benchmarks-tap.xml
endif

V8_TEST_OPTIONS = $(V8_EXTRA_TEST_OPTIONS)
ifdef DISABLE_V8_I18N
  V8_TEST_OPTIONS += --noi18n
  V8_BUILD_OPTIONS += i18nsupport=off
endif

BUILDTYPE_LOWER := $(shell echo $(BUILDTYPE) | tr '[A-Z]' '[a-z]')

# Determine EXEEXT
EXEEXT := $(shell $(PYTHON) -c \
		"import sys; print('.exe' if sys.platform == 'win32' else '')")

NODE_EXE = node$(EXEEXT)
NODE ?= ./$(NODE_EXE)
NODE_G_EXE = node_g$(EXEEXT)
NPM ?= ./deps/npm/bin/npm-cli.js

# Flags for packaging.
BUILD_DOWNLOAD_FLAGS ?= --download=all
BUILD_INTL_FLAGS ?= --with-intl=small-icu
BUILD_RELEASE_FLAGS ?= $(BUILD_DOWNLOAD_FLAGS) $(BUILD_INTL_FLAGS)

# Default to verbose builds.
# To do quiet/pretty builds, run `make V=` to set V to an empty string,
# or set the V environment variable to an empty string.
V ?= 1

# BUILDTYPE=Debug builds both release and debug builds. If you want to compile
# just the debug build, run `make -C out BUILDTYPE=Debug` instead.
ifeq ($(BUILDTYPE),Release)
all: out/Makefile $(NODE_EXE)
else
all: out/Makefile $(NODE_EXE) $(NODE_G_EXE)
endif

# The .PHONY is needed to ensure that we recursively use the out/Makefile
# to check for changes.
.PHONY: $(NODE_EXE) $(NODE_G_EXE)

$(NODE_EXE): config.gypi out/Makefile
	$(MAKE) -C out BUILDTYPE=Release V=$(V)
	ln -fs out/Release/$(NODE_EXE) $@

$(NODE_G_EXE): config.gypi out/Makefile
	$(MAKE) -C out BUILDTYPE=Debug V=$(V)
	ln -fs out/Debug/$(NODE_EXE) $@

out/Makefile: common.gypi deps/uv/uv.gyp deps/http_parser/http_parser.gyp \
              deps/zlib/zlib.gyp deps/v8/gypfiles/toolchain.gypi \
              deps/v8/gypfiles/features.gypi deps/v8/src/v8.gyp node.gyp \
              config.gypi
	$(PYTHON) tools/gyp_node.py -f make

config.gypi: configure
	$(error Missing or stale $@, please run ./$<)

install: all
	$(PYTHON) tools/install.py $@ '$(DESTDIR)' '$(PREFIX)'

uninstall:
	$(PYTHON) tools/install.py $@ '$(DESTDIR)' '$(PREFIX)'

clean:
	-rm -rf out/Makefile $(NODE_EXE) $(NODE_G_EXE) out/$(BUILDTYPE)/$(NODE_EXE) \
                out/$(BUILDTYPE)/node.exp
	@if [ -d out ]; then find out/ -name '*.o' -o -name '*.a' -o -name '*.d' | xargs rm -rf; fi
	-rm -rf node_modules
	@if [ -d deps/icu ]; then echo deleting deps/icu; rm -rf deps/icu; fi
	-rm -f test.tap

distclean:
	-rm -rf out
	-rm -f config.gypi icu_config.gypi config_fips.gypi
	-rm -f config.mk
	-rm -rf $(NODE_EXE) $(NODE_G_EXE)
	-rm -rf node_modules
	-rm -rf deps/icu
	-rm -rf deps/icu4c*.tgz deps/icu4c*.zip deps/icu-tmp
	-rm -f $(BINARYTAR).* $(TARBALL).*
	-rm -rf deps/v8/testing/gmock

check: test

cctest: all
	@out/$(BUILDTYPE)/$@

v8:
	tools/make-v8.sh
	$(MAKE) -C deps/v8 $(V8_ARCH).$(BUILDTYPE_LOWER) $(V8_BUILD_OPTIONS)

test: all
	$(MAKE) build-addons
	$(MAKE) cctest
	$(PYTHON) tools/test.py --mode=release -J \
		addons doctool inspector known_issues message pseudo-tty parallel sequential
	$(MAKE) lint

test-parallel: all
	$(PYTHON) tools/test.py --mode=release parallel -J

test-valgrind: all
	$(PYTHON) tools/test.py --mode=release --valgrind sequential parallel message

test/gc/node_modules/weak/build/Release/weakref.node: $(NODE_EXE)
	$(NODE) deps/npm/node_modules/node-gyp/bin/node-gyp rebuild \
		--python="$(PYTHON)" \
		--directory="$(shell pwd)/test/gc/node_modules/weak" \
		--nodedir="$(shell pwd)"

# Implicitly depends on $(NODE_EXE), see the build-addons rule for rationale.
DOCBUILDSTAMP_PREREQS = tools/doc/addon-verify.js doc/api/addons.md

ifeq ($(OSTYPE),aix)
DOCBUILDSTAMP_PREREQS := $(DOCBUILDSTAMP_PREREQS) out/$(BUILDTYPE)/node.exp
endif

test/addons/.docbuildstamp: $(DOCBUILDSTAMP_PREREQS)
	$(RM) -r test/addons/??_*/
	$(NODE) $<
	touch $@

ADDONS_BINDING_GYPS := \
	$(filter-out test/addons/??_*/binding.gyp, \
		$(wildcard test/addons/*/binding.gyp))

ADDONS_BINDING_SOURCES := \
	$(filter-out test/addons/??_*/*.cc, $(wildcard test/addons/*/*.cc)) \
	$(filter-out test/addons/??_*/*.h, $(wildcard test/addons/*/*.h))

# Implicitly depends on $(NODE_EXE), see the build-addons rule for rationale.
# Depends on node-gyp package.json so that build-addons is (re)executed when
# node-gyp is updated as part of an npm update.
test/addons/.buildstamp: config.gypi \
	deps/npm/node_modules/node-gyp/package.json \
	$(ADDONS_BINDING_GYPS) $(ADDONS_BINDING_SOURCES) \
	deps/uv/include/*.h deps/v8/include/*.h \
	src/node.h src/node_buffer.h src/node_object_wrap.h src/node_version.h \
	test/addons/.docbuildstamp
#	Cannot use $(wildcard test/addons/*/) here, it's evaluated before
#	embedded addons have been generated from the documentation.
	@for dirname in test/addons/*/; do \
		echo "\nBuilding addon $$PWD/$$dirname" ; \
		env MAKEFLAGS="-j1" $(NODE) deps/npm/node_modules/node-gyp/bin/node-gyp \
		        --loglevel=$(LOGLEVEL) rebuild \
			--python="$(PYTHON)" \
			--directory="$$PWD/$$dirname" \
			--nodedir="$$PWD" || exit 1 ; \
	done
	touch $@

# .buildstamp and .docbuildstamp need $(NODE_EXE) but cannot depend on it
# directly because it calls make recursively.  The parent make cannot know
# if the subprocess touched anything so it pessimistically assumes that
# .buildstamp and .docbuildstamp are out of date and need a rebuild.
# Just goes to show that recursive make really is harmful...
# TODO(bnoordhuis) Force rebuild after gyp update.
build-addons: $(NODE_EXE) test/addons/.buildstamp

test-gc: all test/gc/node_modules/weak/build/Release/weakref.node
	$(PYTHON) tools/test.py --mode=release gc

test-build: | all build-addons

test-all: test-build test/gc/node_modules/weak/build/Release/weakref.node
	$(PYTHON) tools/test.py --mode=debug,release

test-all-valgrind: test-build
	$(PYTHON) tools/test.py --mode=debug,release --valgrind

CI_NATIVE_SUITES := addons
CI_JS_SUITES := doctool inspector known_issues message parallel pseudo-tty sequential

# Build and test addons without building anything else
test-ci-native: LOGLEVEL := info
test-ci-native: | test/addons/.buildstamp
	$(PYTHON) tools/test.py $(PARALLEL_ARGS) -p tap --logfile test.tap \
		--mode=release --flaky-tests=$(FLAKY_TESTS) \
		$(TEST_CI_ARGS) $(CI_NATIVE_SUITES)

# This target should not use a native compiler at all
test-ci-js:
	$(PYTHON) tools/test.py $(PARALLEL_ARGS) -p tap --logfile test.tap \
		--mode=release --flaky-tests=$(FLAKY_TESTS) \
		$(TEST_CI_ARGS) $(CI_JS_SUITES)

test-ci: LOGLEVEL := info
test-ci: | build-addons
	out/Release/cctest --gtest_output=tap:cctest.tap
	$(PYTHON) tools/test.py $(PARALLEL_ARGS) -p tap --logfile test.tap \
		--mode=release --flaky-tests=$(FLAKY_TESTS) \
		$(TEST_CI_ARGS) $(CI_NATIVE_SUITES) $(CI_JS_SUITES)

test-release: test-build
	$(PYTHON) tools/test.py --mode=release

test-debug: test-build
	$(PYTHON) tools/test.py --mode=debug

test-message: test-build
	$(PYTHON) tools/test.py message

test-simple: | cctest  # Depends on 'all'.
	$(PYTHON) tools/test.py parallel sequential

test-pummel: all
	$(PYTHON) tools/test.py pummel

test-internet: all
	$(PYTHON) tools/test.py internet

test-debugger: all
	$(PYTHON) tools/test.py debugger

test-inspector: all
	$(PYTHON) tools/test.py inspector

test-tick-processor: all
	$(PYTHON) tools/test.py tick-processor

test-known-issues: all
	$(PYTHON) tools/test.py known_issues

test-npm: $(NODE_EXE)
	NODE=$(NODE) tools/test-npm.sh

test-npm-publish: $(NODE_EXE)
	npm_package_config_publishtest=true $(NODE) deps/npm/test/run.js

test-addons: test-build
	$(PYTHON) tools/test.py --mode=release addons

test-timers:
	$(MAKE) --directory=tools faketime
	$(PYTHON) tools/test.py --mode=release timers

test-timers-clean:
	$(MAKE) --directory=tools clean


ifneq ("","$(wildcard deps/v8/tools/run-tests.py)")
test-v8: v8
#	note: performs full test unless QUICKCHECK is specified
	deps/v8/tools/run-tests.py --arch=$(V8_ARCH) \
        --mode=$(BUILDTYPE_LOWER) $(V8_TEST_OPTIONS) $(QUICKCHECK_ARG) \
        --no-presubmit \
        --shell-dir=$(PWD)/deps/v8/out/$(V8_ARCH).$(BUILDTYPE_LOWER) \
	 $(TAP_V8)

test-v8-intl: v8
#	note: performs full test unless QUICKCHECK is specified
	deps/v8/tools/run-tests.py --arch=$(V8_ARCH) \
        --mode=$(BUILDTYPE_LOWER) --no-presubmit $(QUICKCHECK_ARG) \
        --shell-dir=deps/v8/out/$(V8_ARCH).$(BUILDTYPE_LOWER) intl \
        $(TAP_V8_INTL)

test-v8-benchmarks: v8
	deps/v8/tools/run-tests.py --arch=$(V8_ARCH) --mode=$(BUILDTYPE_LOWER) \
        --download-data $(QUICKCHECK_ARG) --no-presubmit \
        --shell-dir=deps/v8/out/$(V8_ARCH).$(BUILDTYPE_LOWER) benchmarks \
	 $(TAP_V8_BENCHMARKS)

test-v8-all: test-v8 test-v8-intl test-v8-benchmarks
# runs all v8 tests
else
test-v8 test-v8-intl test-v8-benchmarks test-v8-all:
	@echo "Testing v8 is not available through the source tarball."
	@echo "Use the git repo instead:" \
		"$ git clone https://github.com/nodejs/node.git"
endif

apidoc_sources = $(wildcard doc/api/*.md)
apidocs_html = $(apidoc_dirs) $(apiassets) $(addprefix out/,$(apidoc_sources:.md=.html))
apidocs_json = $(apidoc_dirs) $(apiassets) $(addprefix out/,$(apidoc_sources:.md=.json))

apidoc_dirs = out/doc out/doc/api/ out/doc/api/assets

apiassets = $(subst api_assets,api/assets,$(addprefix out/,$(wildcard doc/api_assets/*)))

doc-only: $(apidocs_html) $(apidocs_json)
doc: $(NODE_EXE) doc-only

$(apidoc_dirs):
	mkdir -p $@

out/doc/api/assets/%: doc/api_assets/% out/doc/api/assets/
	cp $< $@

out/doc/%: doc/%
	cp -r $< $@

# check if ./node is actually set, else use user pre-installed binary
gen-json = tools/doc/generate.js --format=json $< > $@
out/doc/api/%.json: doc/api/%.md
	@[ -e tools/doc/node_modules/js-yaml/package.json ] || \
		[ -e tools/eslint/node_modules/js-yaml/package.json ] || \
		if [ -x $(NODE) ]; then \
			cd tools/doc && ../../$(NODE) ../../$(NPM) install; \
		else \
			cd tools/doc && node ../../$(NPM) install; \
		fi
	[ -x $(NODE) ] && $(NODE) $(gen-json) || node $(gen-json)

# check if ./node is actually set, else use user pre-installed binary
gen-html = tools/doc/generate.js --node-version=$(FULLVERSION) --format=html --template=doc/template.html $< > $@
out/doc/api/%.html: doc/api/%.md
	@[ -e tools/doc/node_modules/js-yaml/package.json ] || \
		[ -e tools/eslint/node_modules/js-yaml/package.json ] || \
		if [ -x $(NODE) ]; then \
			cd tools/doc && ../../$(NODE) ../../$(NPM) install; \
		else \
			cd tools/doc && node ../../$(NPM) install; \
		fi
	[ -x $(NODE) ] && $(NODE) $(gen-html) || node $(gen-html)

docopen: $(apidocs_html)
	@$(PYTHON) -mwebbrowser file://$(PWD)/out/doc/api/all.html

docclean:
	-rm -rf out/doc

build-ci:
	$(PYTHON) ./configure $(CONFIG_FLAGS)
	$(MAKE)

run-ci: build-ci
	$(MAKE) test-ci

RAWVER=$(shell $(PYTHON) tools/getnodeversion.py)
VERSION=v$(RAWVER)

# For nightly builds, you must set DISTTYPE to "nightly", "next-nightly" or
# "custom". For the nightly and next-nightly case, you need to set DATESTRING
# and COMMIT in order to properly name the build.
# For the rc case you need to set CUSTOMTAG to an appropriate CUSTOMTAG number

ifndef DISTTYPE
DISTTYPE=release
endif
ifeq ($(DISTTYPE),release)
FULLVERSION=$(VERSION)
else # ifeq ($(DISTTYPE),release)
ifeq ($(DISTTYPE),custom)
ifndef CUSTOMTAG
$(error CUSTOMTAG is not set for DISTTYPE=custom)
endif # ifndef CUSTOMTAG
TAG=$(CUSTOMTAG)
else # ifeq ($(DISTTYPE),custom)
ifndef DATESTRING
$(error DATESTRING is not set for nightly)
endif # ifndef DATESTRING
ifndef COMMIT
$(error COMMIT is not set for nightly)
endif # ifndef COMMIT
ifneq ($(DISTTYPE),nightly)
ifneq ($(DISTTYPE),next-nightly)
$(error DISTTYPE is not release, custom, nightly or next-nightly)
endif # ifneq ($(DISTTYPE),next-nightly)
endif # ifneq ($(DISTTYPE),nightly)
TAG=$(DISTTYPE)$(DATESTRING)$(COMMIT)
endif # ifeq ($(DISTTYPE),custom)
FULLVERSION=$(VERSION)-$(TAG)
endif # ifeq ($(DISTTYPE),release)

DISTTYPEDIR ?= $(DISTTYPE)
RELEASE=$(shell sed -ne 's/\#define NODE_VERSION_IS_RELEASE \([01]\)/\1/p' src/node_version.h)
PLATFORM=$(shell uname | tr '[:upper:]' '[:lower:]')
NPMVERSION=v$(shell cat deps/npm/package.json | grep '"version"' | sed 's/^[^:]*: "\([^"]*\)",.*/\1/')

UNAME_M=$(shell uname -m)
ifeq ($(findstring x86_64,$(UNAME_M)),x86_64)
DESTCPU ?= x64
else
ifeq ($(findstring ppc64,$(UNAME_M)),ppc64)
DESTCPU ?= ppc64
else
ifeq ($(findstring ppc,$(UNAME_M)),ppc)
DESTCPU ?= ppc
else
ifeq ($(findstring s390x,$(UNAME_M)),s390x)
DESTCPU ?= s390x
else
ifeq ($(findstring s390,$(UNAME_M)),s390)
DESTCPU ?= s390
else
ifeq ($(findstring arm,$(UNAME_M)),arm)
DESTCPU ?= arm
else
ifeq ($(findstring aarch64,$(UNAME_M)),aarch64)
DESTCPU ?= aarch64
else
ifeq ($(findstring powerpc,$(shell uname -p)),powerpc)
DESTCPU ?= ppc64
else
DESTCPU ?= x86
endif
endif
endif
endif
endif
endif
endif
endif
ifeq ($(DESTCPU),x64)
ARCH=x64
else
ifeq ($(DESTCPU),arm)
ARCH=arm
else
ifeq ($(DESTCPU),aarch64)
ARCH=arm64
else
ifeq ($(DESTCPU),ppc64)
ARCH=ppc64
else
ifeq ($(DESTCPU),ppc)
ARCH=ppc
else
ifeq ($(DESTCPU),s390)
ARCH=s390
else
ifeq ($(DESTCPU),s390x)
ARCH=s390x
else
ARCH=x86
endif
endif
endif
endif
endif
endif
endif

# node and v8 use different arch names (e.g. node 'x86' vs v8 'ia32').
# pass the proper v8 arch name to $V8_ARCH based on user-specified $DESTCPU.
ifeq ($(DESTCPU),x86)
V8_ARCH=ia32
else
V8_ARCH ?= $(DESTCPU)

endif

# enforce "x86" over "ia32" as the generally accepted way of referring to 32-bit intel
ifeq ($(ARCH),ia32)
override ARCH=x86
endif
ifeq ($(DESTCPU),ia32)
override DESTCPU=x86
endif

TARNAME=node-$(FULLVERSION)
TARBALL=$(TARNAME).tar
# Custom user-specified variation, use it directly
ifdef VARIATION
BINARYNAME=$(TARNAME)-$(PLATFORM)-$(ARCH)-$(VARIATION)
else
BINARYNAME=$(TARNAME)-$(PLATFORM)-$(ARCH)
endif
BINARYTAR=$(BINARYNAME).tar
# OSX doesn't have xz installed by default, http://macpkg.sourceforge.net/
XZ=$(shell which xz > /dev/null 2>&1; echo $$?)
XZ_COMPRESSION ?= 9
PKG=$(TARNAME).pkg
PACKAGEMAKER ?= /Developer/Applications/Utilities/PackageMaker.app/Contents/MacOS/PackageMaker
PKGDIR=out/dist-osx

release-only:
	@if [ "$(DISTTYPE)" != "nightly" ] && [ "$(DISTTYPE)" != "next-nightly" ] && \
		`grep -q REPLACEME doc/api/*.md`; then \
		echo 'Please update Added: tags in the documentation first.' ; \
		exit 1 ; \
	fi
	@if [ "$(shell git status --porcelain | egrep -v '^\?\? ')" = "" ]; then \
		exit 0 ; \
	else \
		echo "" >&2 ; \
		echo "The git repository is not clean." >&2 ; \
		echo "Please commit changes before building release tarball." >&2 ; \
		echo "" >&2 ; \
		git status --porcelain | egrep -v '^\?\?' >&2 ; \
		echo "" >&2 ; \
		exit 1 ; \
	fi
	@if [ "$(DISTTYPE)" != "release" -o "$(RELEASE)" = "1" ]; then \
		exit 0; \
	else \
		echo "" >&2 ; \
		echo "#NODE_VERSION_IS_RELEASE is set to $(RELEASE)." >&2 ; \
		echo "Did you remember to update src/node_version.h?" >&2 ; \
		echo "" >&2 ; \
		exit 1 ; \
	fi

$(PKG): release-only
	rm -rf $(PKGDIR)
	rm -rf out/deps out/Release
	$(PYTHON) ./configure \
		--dest-cpu=x64 \
		--tag=$(TAG) \
		--release-urlbase=$(RELEASE_URLBASE) \
		$(CONFIG_FLAGS) $(BUILD_RELEASE_FLAGS)
	$(MAKE) install V=$(V) DESTDIR=$(PKGDIR)
	SIGN="$(CODESIGN_CERT)" PKGDIR="$(PKGDIR)" bash tools/osx-codesign.sh
	cat tools/osx-pkg.pmdoc/index.xml.tmpl \
		| sed -E "s/\\{nodeversion\\}/$(FULLVERSION)/g" \
		| sed -E "s/\\{npmversion\\}/$(NPMVERSION)/g" \
		> tools/osx-pkg.pmdoc/index.xml
	$(PACKAGEMAKER) \
		--id "org.nodejs.pkg" \
		--doc tools/osx-pkg.pmdoc \
		--out $(PKG)
	SIGN="$(PRODUCTSIGN_CERT)" PKG="$(PKG)" bash tools/osx-productsign.sh

pkg: $(PKG)

pkg-upload: pkg
	ssh $(STAGINGSERVER) "mkdir -p nodejs/$(DISTTYPEDIR)/$(FULLVERSION)"
	chmod 664 node-$(FULLVERSION).pkg
	scp -p node-$(FULLVERSION).pkg $(STAGINGSERVER):nodejs/$(DISTTYPEDIR)/$(FULLVERSION)/node-$(FULLVERSION).pkg
	ssh $(STAGINGSERVER) "touch nodejs/$(DISTTYPEDIR)/$(FULLVERSION)/node-$(FULLVERSION).pkg.done"

$(TARBALL): release-only $(NODE_EXE) doc
	git checkout-index -a -f --prefix=$(TARNAME)/
	mkdir -p $(TARNAME)/doc/api
	cp doc/node.1 $(TARNAME)/doc/node.1
	cp -r out/doc/api/* $(TARNAME)/doc/api/
	rm -rf $(TARNAME)/deps/v8/{test,samples,tools/profviz,tools/run-tests.py}
	rm -rf $(TARNAME)/doc/images # too big
	rm -rf $(TARNAME)/deps/uv/{docs,samples,test}
	rm -rf $(TARNAME)/deps/openssl/openssl/{doc,demos,test}
	rm -rf $(TARNAME)/deps/zlib/contrib # too big, unused
	rm -rf $(TARNAME)/.{editorconfig,git*,mailmap}
	rm -rf $(TARNAME)/tools/{eslint,eslint-rules,osx-pkg.pmdoc,pkgsrc}
	rm -rf $(TARNAME)/tools/{osx-*,license-builder.sh,cpplint.py}
	rm -rf $(TARNAME)/test*.tap
	find $(TARNAME)/ -name ".eslint*" -maxdepth 2 | xargs rm
	find $(TARNAME)/ -type l | xargs rm # annoying on windows
	tar -cf $(TARNAME).tar $(TARNAME)
	rm -rf $(TARNAME)
	gzip -c -f -9 $(TARNAME).tar > $(TARNAME).tar.gz
ifeq ($(XZ), 0)
	xz -c -f -$(XZ_COMPRESSION) $(TARNAME).tar > $(TARNAME).tar.xz
endif
	rm $(TARNAME).tar

tar: $(TARBALL)

tar-upload: tar
	ssh $(STAGINGSERVER) "mkdir -p nodejs/$(DISTTYPEDIR)/$(FULLVERSION)"
	chmod 664 node-$(FULLVERSION).tar.gz
	scp -p node-$(FULLVERSION).tar.gz $(STAGINGSERVER):nodejs/$(DISTTYPEDIR)/$(FULLVERSION)/node-$(FULLVERSION).tar.gz
	ssh $(STAGINGSERVER) "touch nodejs/$(DISTTYPEDIR)/$(FULLVERSION)/node-$(FULLVERSION).tar.gz.done"
ifeq ($(XZ), 0)
	chmod 664 node-$(FULLVERSION).tar.xz
	scp -p node-$(FULLVERSION).tar.xz $(STAGINGSERVER):nodejs/$(DISTTYPEDIR)/$(FULLVERSION)/node-$(FULLVERSION).tar.xz
	ssh $(STAGINGSERVER) "touch nodejs/$(DISTTYPEDIR)/$(FULLVERSION)/node-$(FULLVERSION).tar.xz.done"
endif

doc-upload: tar
	ssh $(STAGINGSERVER) "mkdir -p nodejs/$(DISTTYPEDIR)/$(FULLVERSION)"
	chmod -R ug=rw-x+X,o=r+X out/doc/
	scp -pr out/doc/ $(STAGINGSERVER):nodejs/$(DISTTYPEDIR)/$(FULLVERSION)/docs/
	ssh $(STAGINGSERVER) "touch nodejs/$(DISTTYPEDIR)/$(FULLVERSION)/docs.done"

$(TARBALL)-headers: release-only
	$(PYTHON) ./configure \
		--prefix=/ \
		--dest-cpu=$(DESTCPU) \
		--tag=$(TAG) \
		--release-urlbase=$(RELEASE_URLBASE) \
		$(CONFIG_FLAGS) $(BUILD_RELEASE_FLAGS)
	HEADERS_ONLY=1 $(PYTHON) tools/install.py install '$(TARNAME)' '/'
	find $(TARNAME)/ -type l | xargs rm -f
	tar -cf $(TARNAME)-headers.tar $(TARNAME)
	rm -rf $(TARNAME)
	gzip -c -f -9 $(TARNAME)-headers.tar > $(TARNAME)-headers.tar.gz
ifeq ($(XZ), 0)
	xz -c -f -$(XZ_COMPRESSION) $(TARNAME)-headers.tar > $(TARNAME)-headers.tar.xz
endif
	rm $(TARNAME)-headers.tar

tar-headers: $(TARBALL)-headers

tar-headers-upload: tar-headers
	ssh $(STAGINGSERVER) "mkdir -p nodejs/$(DISTTYPEDIR)/$(FULLVERSION)"
	chmod 664 $(TARNAME)-headers.tar.gz
	scp -p $(TARNAME)-headers.tar.gz $(STAGINGSERVER):nodejs/$(DISTTYPEDIR)/$(FULLVERSION)/$(TARNAME)-headers.tar.gz
	ssh $(STAGINGSERVER) "touch nodejs/$(DISTTYPEDIR)/$(FULLVERSION)/$(TARNAME)-headers.tar.gz.done"
ifeq ($(XZ), 0)
	chmod 664 $(TARNAME)-headers.tar.xz
	scp -p $(TARNAME)-headers.tar.xz $(STAGINGSERVER):nodejs/$(DISTTYPEDIR)/$(FULLVERSION)/$(TARNAME)-headers.tar.xz
	ssh $(STAGINGSERVER) "touch nodejs/$(DISTTYPEDIR)/$(FULLVERSION)/$(TARNAME)-headers.tar.xz.done"
endif

$(BINARYTAR): release-only
	rm -rf $(BINARYNAME)
	rm -rf out/deps out/Release
	$(PYTHON) ./configure \
		--prefix=/ \
		--dest-cpu=$(DESTCPU) \
		--tag=$(TAG) \
		--release-urlbase=$(RELEASE_URLBASE) \
		$(CONFIG_FLAGS) $(BUILD_RELEASE_FLAGS)
	$(MAKE) install DESTDIR=$(BINARYNAME) V=$(V) PORTABLE=1
	cp README.md $(BINARYNAME)
	cp LICENSE $(BINARYNAME)
	cp CHANGELOG.md $(BINARYNAME)
	tar -cf $(BINARYNAME).tar $(BINARYNAME)
	rm -rf $(BINARYNAME)
	gzip -c -f -9 $(BINARYNAME).tar > $(BINARYNAME).tar.gz
ifeq ($(XZ), 0)
	xz -c -f -$(XZ_COMPRESSION) $(BINARYNAME).tar > $(BINARYNAME).tar.xz
endif
	rm $(BINARYNAME).tar

binary: $(BINARYTAR)

binary-upload: binary
	ssh $(STAGINGSERVER) "mkdir -p nodejs/$(DISTTYPEDIR)/$(FULLVERSION)"
	chmod 664 node-$(FULLVERSION)-$(OSTYPE)-$(ARCH).tar.gz
	scp -p node-$(FULLVERSION)-$(OSTYPE)-$(ARCH).tar.gz $(STAGINGSERVER):nodejs/$(DISTTYPEDIR)/$(FULLVERSION)/node-$(FULLVERSION)-$(OSTYPE)-$(ARCH).tar.gz
	ssh $(STAGINGSERVER) "touch nodejs/$(DISTTYPEDIR)/$(FULLVERSION)/node-$(FULLVERSION)-$(OSTYPE)-$(ARCH).tar.gz.done"
ifeq ($(XZ), 0)
	chmod 664 node-$(FULLVERSION)-$(OSTYPE)-$(ARCH).tar.xz
	scp -p node-$(FULLVERSION)-$(OSTYPE)-$(ARCH).tar.xz $(STAGINGSERVER):nodejs/$(DISTTYPEDIR)/$(FULLVERSION)/node-$(FULLVERSION)-$(OSTYPE)-$(ARCH).tar.xz
	ssh $(STAGINGSERVER) "touch nodejs/$(DISTTYPEDIR)/$(FULLVERSION)/node-$(FULLVERSION)-$(OSTYPE)-$(ARCH).tar.xz.done"
endif

bench-net: all
	@$(NODE) benchmark/run.js net

bench-crypto: all
	@$(NODE) benchmark/run.js crypto

bench-tls: all
	@$(NODE) benchmark/run.js tls

bench-http: all
	@$(NODE) benchmark/run.js http

bench-fs: all
	@$(NODE) benchmark/run.js fs

bench-misc: all
	@$(MAKE) -C benchmark/misc/function_call/
	@$(NODE) benchmark/run.js misc

bench-array: all
	@$(NODE) benchmark/run.js arrays

bench-buffer: all
	@$(NODE) benchmark/run.js buffers

bench-url: all
	@$(NODE) benchmark/run.js url

bench-events: all
	@$(NODE) benchmark/run.js events

bench-util: all
	@$(NODE) benchmark/run.js util

bench-dgram: all
	@$(NODE) benchmark/run.js dgram

bench-all: bench bench-misc bench-array bench-buffer bench-url bench-events bench-dgram bench-util

bench: bench-net bench-http bench-fs bench-tls

bench-ci: bench

jslint:
	$(NODE) tools/eslint/bin/eslint.js --cache --rulesdir=tools/eslint-rules \
	  benchmark lib test tools

jslint-ci:
	$(NODE) tools/jslint.js $(PARALLEL_ARGS) -f tap -o test-eslint.tap \
		benchmark lib test tools

CPPLINT_EXCLUDE ?=
CPPLINT_EXCLUDE += src/node_root_certs.h
CPPLINT_EXCLUDE += src/queue.h
CPPLINT_EXCLUDE += src/tree.h
CPPLINT_EXCLUDE += $(wildcard test/addons/??_*/*.cc test/addons/??_*/*.h)

CPPLINT_FILES = $(filter-out $(CPPLINT_EXCLUDE), $(wildcard \
	src/*.c \
	src/*.cc \
	src/*.h \
	test/addons/*/*.cc \
	test/addons/*/*.h \
	tools/icu/*.cc \
	tools/icu/*.h \
	))

cpplint:
	@$(PYTHON) tools/cpplint.py $(CPPLINT_FILES)
	@$(PYTHON) tools/check-imports.py

ifneq ("","$(wildcard tools/eslint/lib/eslint.js)")
lint: jslint cpplint
CONFLICT_RE=^>>>>>>> [0-9A-Fa-f]+|^<<<<<<< [A-Za-z]+
lint-ci: jslint-ci cpplint
	@if ! ( grep -IEqrs "$(CONFLICT_RE)" benchmark deps doc lib src test tools ) \
		&& ! ( find . -maxdepth 1 -type f | xargs grep -IEqs "$(CONFLICT_RE)" ); then \
		exit 0 ; \
	else \
		echo "" >&2 ; \
		echo "Conflict marker detected in one or more files. Please fix them first." >&2 ; \
		exit 1 ; \
	fi
else
lint:
	@echo "Linting is not available through the source tarball."
	@echo "Use the git repo instead:" \
		"$ git clone https://github.com/nodejs/node.git"

lint-ci: lint
endif

.PHONY: lint cpplint jslint bench clean docopen docclean doc dist distclean \
	check uninstall install install-includes install-bin all staticlib \
	dynamiclib test test-all test-addons build-addons website-upload pkg \
	blog blogclean tar binary release-only bench-http-simple bench-idle \
	bench-all bench bench-misc bench-array bench-buffer bench-net \
	bench-http bench-fs bench-tls cctest run-ci test-v8 test-v8-intl \
	test-v8-benchmarks test-v8-all v8 lint-ci bench-ci jslint-ci doc-only \
	$(TARBALL)-headers test-ci test-ci-native test-ci-js build-ci
