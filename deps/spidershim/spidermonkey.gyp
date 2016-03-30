{
  'variables': {
    'target_arch%': 'ia32',
    'library%': 'shared_library',     # build spidermonkey as shared library
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
        'spidermonkey_header': [
          '<(spidermonkey_dir)/js/src/jsapi.h',
        ],
        'spidermonkey_binaries': [
          '<(spidermonkey_dir)/../build/dist/bin/<(SHARED_LIB_PREFIX)mozjs-48a1<(SHARED_LIB_SUFFIX)',
          '<(spidermonkey_dir)/../build/dist/bin/<(SHARED_LIB_PREFIX)mozglue<(SHARED_LIB_SUFFIX)',
          '<(spidermonkey_dir)/../build/dist/bin/<(EXECUTABLE_PREFIX)js<(EXECUTABLE_SUFFIX)',
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
          'destination': 'include',
          'files': [ '<@(spidermonkey_header)' ],
        },
        {
          'destination': '<(PRODUCT_DIR)',
          'files': [ '<@(spidermonkey_binaries)' ],
        },
      ],

    },
  ],
}
