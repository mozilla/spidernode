{
  'variables': {
    'target_arch%': 'ia32',
    'library%': 'static_library',     # build spidermonkey as static library
    'component%': 'static_library',   # link crt statically or dynamically
    'spidermonkey_dir%': 'spidermonkey',

    'conditions': [
      ['target_arch=="ia32"', { 'Platform': 'x86' }],
      ['target_arch=="x64"', { 'Platform': 'x64' }],
      ['target_arch=="arm"', { 'Platform': 'arm' }],
    ],
  },

  'targets': [
    {
      'target_name': 'spidermonkey',
      'toolsets': ['host'],
      'type': 'none',

      'variables': {
        'spidermonkey_binaries': [
          '<(spidermonkey_dir)/../build/js/src/<(STATIC_LIB_PREFIX)js_static<(STATIC_LIB_SUFFIX)',
          '<(spidermonkey_dir)/../build/dist/bin/<(SHARED_LIB_PREFIX)mozglue<(SHARED_LIB_SUFFIX)',
        ],
      },

      'actions': [
        {
          'action_name': 'build_spidermonkey',
          'inputs': [
            'spidermonkey/js/src/configure',
          ],
          'outputs': [
            '<@(spidermonkey_binaries)',
          ],
          'action': [
            './build-spidermonkey.sh',
          ],
        },
      ],

      'copies': [
        {
          'destination': '<(PRODUCT_DIR)',
          'files': [ '<@(spidermonkey_binaries)' ],
        },
      ],

    },
  ],
}
