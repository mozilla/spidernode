# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, unicode_literals

import os

import mozpack.path as mozpath
from mozbuild.base import MozbuildObject
from mozbuild.backend.base import PartialBackend, HybridBackend
from mozbuild.backend.recursivemake import RecursiveMakeBackend
from mozbuild.shellutil import quote as shell_quote

from .common import CommonBackend
from ..frontend.data import (
    ContextDerived,
)
from ..util import (
    FileAvoidWrite,
)


class BackendTupfile(object):
    """Represents a generated Tupfile.
    """

    def __init__(self, srcdir, objdir, environment, topsrcdir, topobjdir):
        self.topsrcdir = topsrcdir
        self.srcdir = srcdir
        self.objdir = objdir
        self.relobjdir = mozpath.relpath(objdir, topobjdir)
        self.environment = environment
        self.name = mozpath.join(objdir, 'Tupfile')
        self.rules_included = False

        self.fh = FileAvoidWrite(self.name, capture_diff=True)
        self.fh.write('# THIS FILE WAS AUTOMATICALLY GENERATED. DO NOT EDIT.\n')
        self.fh.write('\n')

    def write(self, buf):
        self.fh.write(buf)

    def include_rules(self):
        if not self.rules_included:
            self.write('include_rules\n')
            self.rules_included = True

    def rule(self, cmd, inputs=None, outputs=None, display=None, extra_outputs=None):
        inputs = inputs or []
        outputs = outputs or []
        self.include_rules()
        self.write(': %(inputs)s |> %(display)s%(cmd)s |> %(outputs)s%(extra_outputs)s\n' % {
            'inputs': ' '.join(inputs),
            'display': '^ %s^ ' % display if display else '',
            'cmd': ' '.join(cmd),
            'outputs': ' '.join(outputs),
            'extra_outputs': ' | ' + ' '.join(extra_outputs) if extra_outputs else '',
        })

    def close(self):
        return self.fh.close()

    @property
    def diff(self):
        return self.fh.diff


class TupOnly(CommonBackend, PartialBackend):
    """Backend that generates Tupfiles for the tup build system.
    """

    def _init(self):
        CommonBackend._init(self)

        self._backend_files = {}
        self._cmd = MozbuildObject.from_environment()

    def _get_backend_file(self, relativedir):
        objdir = mozpath.join(self.environment.topobjdir, relativedir)
        srcdir = mozpath.join(self.environment.topsrcdir, relativedir)
        if objdir not in self._backend_files:
            self._backend_files[objdir] = \
                    BackendTupfile(srcdir, objdir, self.environment,
                                   self.environment.topsrcdir, self.environment.topobjdir)
        return self._backend_files[objdir]

    def consume_object(self, obj):
        """Write out build files necessary to build with tup."""

        if not isinstance(obj, ContextDerived):
            return False

        consumed = CommonBackend.consume_object(self, obj)

        # Even if CommonBackend acknowledged the object, we still need to let
        # the RecursiveMake backend also handle these objects.
        if consumed:
            return False

        return True

    def consume_finished(self):
        CommonBackend.consume_finished(self)

        for objdir, backend_file in sorted(self._backend_files.items()):
            with self._write_file(fh=backend_file):
                pass

        with self._write_file(mozpath.join(self.environment.topobjdir, 'Tuprules.tup')) as fh:
            acdefines = [name for name in self.environment.defines
                if not name in self.environment.non_global_defines]
            acdefines_flags = ' '.join(['-D%s=%s' % (name,
                shell_quote(self.environment.defines[name]))
                for name in sorted(acdefines)])
            fh.write('MOZ_OBJ_ROOT = $(TUP_CWD)\n')
            fh.write('DIST = $(MOZ_OBJ_ROOT)/dist\n')
            fh.write('ACDEFINES = %s\n' % acdefines_flags)
            fh.write('topsrcdir = $(MOZ_OBJ_ROOT)/%s\n' % (
                os.path.relpath(self.environment.topsrcdir, self.environment.topobjdir)
            ))
            fh.write('PYTHON = $(MOZ_OBJ_ROOT)/_virtualenv/bin/python -B\n')
            fh.write('PYTHON_PATH = $(PYTHON) $(topsrcdir)/config/pythonpath.py\n')
            fh.write('PLY_INCLUDE = -I$(topsrcdir)/other-licenses/ply\n')
            fh.write('IDL_PARSER_DIR = $(topsrcdir)/xpcom/idl-parser\n')
            fh.write('IDL_PARSER_CACHE_DIR = $(MOZ_OBJ_ROOT)/xpcom/idl-parser\n')

        # Run 'tup init' if necessary.
        if not os.path.exists(mozpath.join(self.environment.topsrcdir, ".tup")):
            tup = self.environment.substs.get('TUP', 'tup')
            self._cmd.run_process(cwd=self.environment.topsrcdir, log_name='tup', args=[tup, 'init'])

    def _handle_idl_manager(self, manager):

        # TODO: This should come from GENERATED_FILES, and can be removed once
        # those are implemented.
        backend_file = self._get_backend_file('xpcom/idl-parser')
        backend_file.rule(
            display='python header.py -> [%o]',
            cmd=[
                '$(PYTHON_PATH)',
                '$(PLY_INCLUDE)',
                '$(topsrcdir)/xpcom/idl-parser/xpidl/header.py',
            ],
            outputs=['xpidlyacc.py', 'xpidllex.py'],
        )

        backend_file = self._get_backend_file('xpcom/xpidl')

        # These are used by mach/mixin/process.py to determine the current
        # shell.
        for var in ('SHELL', 'MOZILLABUILD', 'COMSPEC'):
            backend_file.write('export %s\n' % var)

        for module, data in sorted(manager.modules.iteritems()):
            dest, idls = data
            cmd = [
                '$(PYTHON_PATH)',
                '$(PLY_INCLUDE)',
                '-I$(IDL_PARSER_DIR)',
                '-I$(IDL_PARSER_CACHE_DIR)',
                '$(topsrcdir)/python/mozbuild/mozbuild/action/xpidl-process.py',
                '--cache-dir', '$(MOZ_OBJ_ROOT)/xpcom/idl-parser',
                '$(DIST)/idl',
                '$(DIST)/include',
                '$(MOZ_OBJ_ROOT)/%s/components' % dest,
                module,
            ]
            cmd.extend(sorted(idls))

            outputs = ['$(MOZ_OBJ_ROOT)/%s/components/%s.xpt' % (dest, module)]
            outputs.extend(['$(MOZ_OBJ_ROOT)/dist/include/%s.h' % f for f in sorted(idls)])
            backend_file.rule(
                inputs=[
                    '$(MOZ_OBJ_ROOT)/xpcom/idl-parser/xpidllex.py',
                    '$(MOZ_OBJ_ROOT)/xpcom/idl-parser/xpidlyacc.py',
                ],
                display='XPIDL %s' % module,
                cmd=cmd,
                outputs=outputs,
            )

    def _handle_ipdl_sources(self, ipdl_dir, sorted_ipdl_sources,
                             unified_ipdl_cppsrcs_mapping):
        # TODO: This isn't implemented yet in the tup backend, but it is called
        # by the CommonBackend.
        pass

    def _handle_webidl_build(self, bindings_dir, unified_source_mapping,
                             webidls, expected_build_output_files,
                             global_define_files):
        # TODO: This isn't implemented yet in the tup backend, but it is called
        # by the CommonBackend.
        pass


class TupBackend(HybridBackend(TupOnly, RecursiveMakeBackend)):
    def build(self, config, output, jobs, verbose):
        status = config._run_make(directory=self.environment.topobjdir, target='tup',
                                  line_handler=output.on_line, log=False, print_directory=False,
                                  ensure_exit_code=False, num_jobs=jobs, silent=not verbose,
                                  append_env={b'NO_BUILDSTATUS_MESSAGES': b'1'})
        return status
