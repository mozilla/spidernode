{
  'variables': {
    'target_arch%': 'ia32',
    'spidermonkey_gczeal%': 0,
    'asan%': 0,

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
        'spidermonkey_binaries': [
          '<(PRODUCT_DIR)/spidermonkey/js/src/build/<(STATIC_LIB_PREFIX)js_static<(STATIC_LIB_SUFFIX)',
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
          }],
          ['asan==1', {
            'spidermonkey_args': ['--enable-address-sanitizer'],
          }],
        ],
      },

      'includes': [
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

      'copies': [
        {
          'destination': '<(PRODUCT_DIR)',
          'files': [ '<@(spidermonkey_binaries)' ],
        },
      ],

      'all_dependent_settings': {
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
	'libraries': [
	  '-ljs_static',
	  '-lz',
	],
	'conditions': [
	  [ 'target_arch=="arm"', {
	    'defines': [ '__arm__=1' ]
	  }],
	  ['OS == "linux"', {
	    'libraries': [
	      '-ldl',
              '-Wl,--whole-archive <(PRODUCT_DIR)/<(STATIC_LIB_PREFIX)mozglue<(STATIC_LIB_SUFFIX) -Wl,--no-whole-archive',
	      '-lrt',
	    ],
	  }],
	  ['OS == "mac"', {
	    'libraries': [
	      '-lmozglue',
	    ],
	  }],
	],
      },

    },
  ],
}
