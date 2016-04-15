{
  'variables': {
    'spidermonkey_dir%': 'spidermonkey',
    'spidermonkey_debug%': 0,

    'conditions': [
      ['spidermonkey_debug==1', { 'spidermonkey_build_dir': 'build-debug' },
                                { 'spidermonkey_build_dir': 'build-opt'   }],
    ],
  },

  'targets': [
    {
      'target_name': 'spidershim',
      'type': '<(library)',

      'include_dirs': [
        'include',
        '<(spidermonkey_build_dir)/dist/include',
      ],
      'conditions': [
        [ 'target_arch=="ia32"', { 'defines': [ '__i386__=1' ] } ],
        [ 'target_arch=="x64"', { 'defines': [ '__x86_64__=1' ] } ],
        [ 'target_arch=="arm"', { 'defines': [ '__arm__=1' ] } ],
        ['node_engine=="spidermonkey"', {
          'dependencies': [
            'spidermonkey.gyp:spidermonkey',
          ],
          'export_dependent_settings': [
            'spidermonkey.gyp:spidermonkey',
          ],
        }],
      ],

      'direct_dependent_settings': {
        'include_dirs': [
          'include',
        ],
        'libraries': [
          '-lspidershim',
          '-lz',
          '<@(node_engine_libs)',
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
        'src/v8boolean.cc',
        'src/v8context.cc',
        'src/v8handlescope.cc',
        'src/v8int32.cc',
        'src/v8integer.cc',
        'src/v8isolate.cc',
        'src/v8number.cc',
        'src/v8script.cc',
        'src/v8string.cc',
        'src/v8uint32.cc',
        'src/v8v8.cc',
        'src/v8value.cc',
      ],
    },
  ],
}
