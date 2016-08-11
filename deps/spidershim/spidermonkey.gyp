{
  'variables': {
    'target_arch%': 'ia32',
    'spidermonkey_gczeal%': 0,
    'asan%': 0,
    'spidermonkey_objdir': '<(PRODUCT_DIR)/spidermonkey',

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
        'spidermonkey_extra_args%': [],  # Allow embedders to customize SpiderMonkey
                                         # configure flags.
        'spidermonkey_args': ['<(PRODUCT_DIR)/spidermonkey'],
        'conditions': [
          ['spidermonkey_gczeal==1', {
            'spidermonkey_args': ['--enable-gczeal'],
          }],
          ['asan==1', {
            'spidermonkey_args': ['--enable-address-sanitizer'],
          }],
        ],
      },

      'includes': [
        'spidermonkey-common.gypi',
        'spidermonkey-files.gypi',
      ],

      'actions': [
        {
          'action_name': 'build_spidermonkey',
          'inputs': [
            '<@(spidermonkey_files)',
          ],
          'outputs': [
            '<@(spidermonkey_binaries)',
          ],
          'action': [
            'scripts/build-spidermonkey.sh',
            '<@(spidermonkey_args)',
            '<@(spidermonkey_extra_args)',
          ],
        },
      ],

    },
  ],
}
