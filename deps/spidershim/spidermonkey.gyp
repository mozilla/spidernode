{
  'variables': {
    'target_arch%': 'ia32',
    'library%': 'static_library',     # build spidermonkey as static library
    'component%': 'static_library',   # link crt statically or dynamically
    'spidermonkey_dir%': 'spidermonkey',
    'spidermonkey_debug%': 0,
    'spidermonkey_args': [],
    'spidermonkey_gczeal%': 0,

    'conditions': [
      ['target_arch=="ia32"', { 'Platform': 'x86' }],
      ['target_arch=="x64"', { 'Platform': 'x64' }],
      ['target_arch=="arm"', { 'Platform': 'arm' }],
    ],
  },

  'targets': [
    {
      'target_name': 'spidermonkey',
      'type': 'none',

      'configurations': {
        'Release': {
          'defines': ['NDEBUG'],
        },
        'Debug': {
          'defines': ['DEBUG'],
        },
      },

      'variables': {
        'spidermonkey_binaries': [
          '<(PRODUCT_DIR)/spidermonkey/js/src/<(STATIC_LIB_PREFIX)js_static<(STATIC_LIB_SUFFIX)',
        ],
        'conditions': [
          ['OS == "linux"', {
            'spidermonkey_binaries+': [
              '<(PRODUCT_DIR)/spidermonkey/mozglue/build/<(STATIC_LIB_PREFIX)mozglue<(STATIC_LIB_SUFFIX)',
            ],
          }],
          ['OS == "mac"', {
            'spidermonkey_binaries': [
              '<(PRODUCT_DIR)/spidermonkey/dist/bin/<(SHARED_LIB_PREFIX)mozglue<(SHARED_LIB_SUFFIX)',
            ],
          }],
          ['spidermonkey_gczeal==1', {
            'spidermonkey_args': ['--enable-gczeal'],
            'spidermonkey_defines': ['JS_GC_ZEAL'],
          }],
        ],
      },

      'actions': [
        {
          'action_name': 'build_spidermonkey',
          'inputs': [],
          'outputs': [
            '<@(spidermonkey_binaries)',
          ],
          'action': [
            'scripts/build-spidermonkey.sh',
            '<@(spidermonkey_args)',
          ],
        },
      ],

      'copies': [
        {
          'destination': '<(PRODUCT_DIR)',
          'files': [ '<@(spidermonkey_binaries)' ],
        },
      ],

      'direct_dependent_settings': {
        'library_dirs': [ '<(PRODUCT_DIR)' ],
        'configurations': {
          'Release': {
            'defines': ['NDEBUG'],
          },
          'Debug': {
            'defines': ['DEBUG'],
          },
        },
        'include_dirs': [
          '<(PRODUCT_DIR)/spidermonkey/dist/include',
        ],
      },

    },
  ],
}
