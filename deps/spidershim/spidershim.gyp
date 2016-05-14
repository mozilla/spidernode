{
  'targets': [
    {
      'target_name': 'spidershim',
      'type': '<(library)',

        'include_dirs': [
          'include',
        ],
      'conditions': [
        [ 'target_arch=="ia32"', { 'defines': [ '__i386__=1' ] } ],
        [ 'target_arch=="x64"', { 'defines': [ '__x86_64__=1' ] } ],
        [ 'target_arch=="arm"', { 'defines': [ '__arm__=1' ] } ],
        ['node_engine=="spidermonkey"', {
          'dependencies': [
            'spidermonkey.gyp:spidermonkey',
            '../zlib/zlib.gyp:zlib',
          ],
          'export_dependent_settings': [
            'spidermonkey.gyp:spidermonkey',
            '../zlib/zlib.gyp:zlib',
          ],
        }],
      ],

      'direct_dependent_settings': {
        'include_dirs': [
          'include',
        ],
        'libraries': [
          '-ljs_static',
          '-lspidershim',
          '-lz',
        ],
        'conditions': [
          [ 'target_arch=="arm"', {
            'defines': [ '__arm__=1' ]
          }],
          ['OS == "linux"', {
            'libraries': [
              '-ldl',
              '-lrt',
              '-lzlib',
              '<(PRODUCT_DIR)/<(STATIC_LIB_PREFIX)mozglue<(STATIC_LIB_SUFFIX)',
            ],
          }],
          ['OS == "mac"', {
            'libraries': [
              '-lmozglue',
            ],
          }],
        ],
      },

      'sources': [
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
        'src/v8global.cc',
        'src/v8handlescope.cc',
        'src/v8int32.cc',
        'src/v8integer.cc',
        'src/v8isolate.cc',
        'src/v8message.cc',
        'src/v8number.cc',
        'src/v8numberobject.cc',
        'src/v8object.cc',
        'src/v8objecttemplate.cc',
        'src/v8private.cc',
        'src/v8script.cc',
        'src/v8stackframe.cc',
        'src/v8stacktrace.cc',
        'src/v8string.cc',
        'src/v8stringobject.cc',
        'src/v8template.cc',
        'src/v8trycatch.cc',
        'src/v8uint32.cc',
        'src/v8v8.cc',
        'src/v8value.cc',
      ],
    },
  ],
}
