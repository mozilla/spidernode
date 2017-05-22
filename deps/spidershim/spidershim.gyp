{
  'variables': {
    'library%': 'static_library',
    'node_engine%': 'spidermonkey',
    'external_spidermonkey_release%': '',
    'variables': {
      'external_spidermonkey_debug%': '<(external_spidermonkey_release)',
    },
    'conditions': [
      ['external_spidermonkey_debug=="" and external_spidermonkey_release==""', {
        'spidermonkey_gyp': 'spidermonkey.gyp',
      }, {
        'spidermonkey_gyp': 'spidermonkey-external.gyp',
      }],
    ],
    'python%': 'python',
    'library_files': [
      'lib/spidershim.js',
    ],
  },
  'targets': [
    {
      'target_name': 'spidershim',
      'type': '<(library)',

      'dependencies': [ 'spidershim_js#host' ],

      'include_dirs': [
        'include',
        '<(SHARED_INTERMEDIATE_DIR)',
      ],
      'conditions': [
        [ 'target_arch=="ia32"', { 'defines': [ '__i386__=1' ] } ],
        [ 'target_arch=="x64"', { 'defines': [ '__x86_64__=1' ] } ],
        [ 'target_arch=="arm"', { 'defines': [ '__arm__=1' ] } ],
        ['node_engine=="spidermonkey"', {
          'dependencies': [
            '../zlib/zlib.gyp:zlib',
            '<(spidermonkey_gyp):spidermonkey',
          ],
          'export_dependent_settings': [
            '../zlib/zlib.gyp:zlib',
            '<(spidermonkey_gyp):spidermonkey',
          ],
        }],
        [ 'OS=="mac" or OS=="ios"', {
          'xcode_settings': {
            'GCC_INLINES_ARE_PRIVATE_EXTERN': 'YES',  # -fvisibility-inlines-hidden
            'GCC_SYMBOLS_PRIVATE_EXTERN': 'YES',      # -fvisibility=hidden
            'GCC_TREAT_WARNINGS_AS_ERRORS': 'NO',
          },
        }, {
          'cflags!': ['-Werror'],
        }],
      ],

      'all_dependent_settings': {
        'include_dirs': [
          'include',
        ],
        'library_dirs': [ '<(PRODUCT_DIR)' ],
      },

      'sources': [
        'src/util.cc',
        'src/v8array.cc',
        'src/v8arraybuffer.cc',
        'src/v8arraybufferview.cc',
        'src/v8boolean.cc',
        'src/v8booleanobject.cc',
        'src/v8context.cc',
        'src/v8date.cc',
        'src/v8debug.cc',
        'src/v8exception.cc',
        'src/v8external.cc',
        'src/v8function.cc',
        'src/v8functiontemplate.cc',
        'src/v8global.cc',
        'src/v8handlescope.cc',
        'src/v8int32.cc',
        'src/v8integer.cc',
        'src/v8isolate.cc',
        'src/v8isolate-heapstats.cc',
        'src/v8message.cc',
        'src/v8microtaskscope.cc',
        'src/v8number.cc',
        'src/v8numberobject.cc',
        'src/v8object.cc',
        'src/v8objecttemplate.cc',
        'src/v8propertydescriptor.cc',
        'src/v8private.cc',
        'src/v8proxy.cc',
        'src/v8script.cc',
        'src/v8scriptcompiler.cc',
        'src/v8serialization.cc',
        'src/v8sharedarraybuffer.cc',
        'src/v8signature.cc',
        'src/v8stackframe.cc',
        'src/v8stacktrace.cc',
        'src/v8string.cc',
        'src/v8stringobject.cc',
        'src/v8symbol.cc',
        'src/v8template.cc',
        'src/v8tracing.cc',
        'src/v8trycatch.cc',
        'src/v8typedarray.cc',
        'src/v8uint32.cc',
        'src/v8unboundscript.cc',
        'src/v8v8.cc',
        'src/v8value.cc',
      ],
    },

    {
      'target_name': 'spidershim_js',
      'type': 'none',
      'toolsets': ['host'],
      'actions': [
        {
          'action_name': 'spidershim_js2c',
          'inputs': [
            '<@(library_files)'
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/spidershim_natives.h',
          ],
          'action': [
            '<(python)',
            './../../deps/spidershim/scripts/js2c.py',
            '--namespace=spidershim',
            '<@(_outputs)',
            '<@(_inputs)',
          ],
        },
      ],
    },
  ],
}
