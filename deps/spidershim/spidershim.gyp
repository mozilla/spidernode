{
  'variables': {
    'library%': 'static_library',
    'node_engine%': 'spidermonkey',
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
        '<(spidermonkey_obj_dir)/dist/include',
      ],
      'conditions': [
        [ 'target_arch=="ia32"', { 'defines': [ '__i386__=1' ] } ],
        [ 'target_arch=="x64"', { 'defines': [ '__x86_64__=1' ] } ],
        [ 'target_arch=="arm"', { 'defines': [ '__arm__=1' ] } ],
        ['node_engine=="spidermonkey"', {
          'dependencies': [
            '../zlib/zlib.gyp:zlib',
          ],
          'export_dependent_settings': [
            '../zlib/zlib.gyp:zlib',
          ],
        }],
        [ 'OS=="mac" or OS=="ios"', {
          'xcode_settings': { 'GCC_TREAT_WARNINGS_AS_ERRORS': 'NO' },
        }, {
          'cflags!': ['-Werror'],
        }],
      ],

      'all_dependent_settings': {
        'include_dirs': [
          'include',
        ],
        'libraries': [
          '-lspidershim',
          '-lz',
          '<(spidermonkey_obj_dir)/js/src/libjs_static.a',
          '<(spidermonkey_obj_dir)/mozglue/build/libmozglue.dylib',
          '<(spidermonkey_obj_dir)/dist/PositronDebug.app/Contents/MacOS/libnss3.dylib',
        ],
        'conditions': [
          [ 'target_arch=="arm"', {
            'defines': [ '__arm__=1' ]
          }],
          ['OS == "linux"', {
            'libraries': [
              '-ldl',
              '-lzlib',
              '<(PRODUCT_DIR)/<(STATIC_LIB_PREFIX)mozglue<(STATIC_LIB_SUFFIX)',
              '-lrt',
            ],
          }],
          # ['OS == "mac"', {
          #   'libraries': [
          #     '-lmozglue',
          #   ],
          # }],
        ],
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
        'src/v8private.cc',
        'src/v8proxy.cc',
        'src/v8script.cc',
        'src/v8scriptcompiler.cc',
        'src/v8signature.cc',
        'src/v8stackframe.cc',
        'src/v8stacktrace.cc',
        'src/v8string.cc',
        'src/v8stringobject.cc',
        'src/v8template.cc',
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
            './../../tools/js2c.py',
            '--namespace=spidershim',
            '<@(_outputs)',
            '<@(_inputs)',
          ],
        },
      ],
    },
  ],
}
