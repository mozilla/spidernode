{
  'variables': {
    'external_spidermonkey_debug': '<(external_spidermonkey_release)',
    'spidermonkey_binaries_debug': [
      '<(external_spidermonkey_debug)/js/src/<(STATIC_LIB_PREFIX)js_static<(STATIC_LIB_SUFFIX)',
      '<(external_spidermonkey_debug)/config/external/icu/data/icudata.o',
    ],
    'spidermonkey_binaries_release': [
      '<(external_spidermonkey_release)/js/src/<(STATIC_LIB_PREFIX)js_static<(STATIC_LIB_SUFFIX)',
      '<(external_spidermonkey_release)/config/external/icu/data/icudata.o',
    ],
    'conditions': [
      ['external_spidermonkey_debug_has_nss == 1', {
        'spidermonkey_binaries_debug': [
          '<(external_spidermonkey_debug)/dist/lib/<(SHARED_LIB_PREFIX)nss3<(SHARED_LIB_SUFFIX)',
        ],
      }],
      ['external_spidermonkey_release_has_nss == 1', {
        'spidermonkey_binaries_release': [
          '<(external_spidermonkey_release)/dist/lib/<(SHARED_LIB_PREFIX)nss3<(SHARED_LIB_SUFFIX)',
        ],
      }],
      ['OS == "linux"', {
        'spidermonkey_binaries_debug+': [
          '<(external_spidermonkey_debug)/mozglue/build/<(STATIC_LIB_PREFIX)mozglue<(STATIC_LIB_SUFFIX)',
        ],
        'spidermonkey_binaries_release+': [
          '<(external_spidermonkey_release)/mozglue/build/<(STATIC_LIB_PREFIX)mozglue<(STATIC_LIB_SUFFIX)',
        ],
      }],
      ['OS == "linux" and external_spidermonkey_debug_has_nspr == 1', {
        'spidermonkey_binaries_debug+': [
          '<(external_spidermonkey_debug)/config/external/nspr/pr/<(SHARED_LIB_PREFIX)nspr4<(SHARED_LIB_SUFFIX)',
        ],
      }],
      ['OS == "linux" and external_spidermonkey_release_has_nspr == 1', {
        'spidermonkey_binaries_release+': [
          '<(external_spidermonkey_release)/config/external/nspr/pr/<(SHARED_LIB_PREFIX)nspr4<(SHARED_LIB_SUFFIX)',
        ],
      }],
      ['OS == "mac"', {
        'spidermonkey_binaries_debug': [
          '<(external_spidermonkey_debug)/dist/bin/<(SHARED_LIB_PREFIX)mozglue<(SHARED_LIB_SUFFIX)',
        ],
        'spidermonkey_binaries_release': [
          '<(external_spidermonkey_release)/dist/bin/<(SHARED_LIB_PREFIX)mozglue<(SHARED_LIB_SUFFIX)',
        ],
      }],
      ['OS == "mac" and external_spidermonkey_debug_has_nspr == 1', {
        'spidermonkey_binaries_debug': [
          '<(external_spidermonkey_debug)/dist/lib/<(SHARED_LIB_PREFIX)nspr4<(SHARED_LIB_SUFFIX)',
        ],
      }],
      ['OS == "mac" and external_spidermonkey_release_has_nspr == 1', {
        'spidermonkey_binaries_release': [
          '<(external_spidermonkey_release)/dist/lib/<(SHARED_LIB_PREFIX)nspr4<(SHARED_LIB_SUFFIX)',
        ],
      }],
    ],
  },

  'targets': [
    {
      'target_name': 'spidermonkey',
      'type': 'none',

      'actions': [
        {
          'action_name': 'symlink_spidermonkey_debug',
          'inputs': [ '<(external_spidermonkey_debug)' ],
          'outputs': [
            '<(PRODUCT_DIR)/spidermonkey/Debug/dist/include',
          ],
          'action': [
            'ln', '-s', '<(external_spidermonkey_debug)/dist/include',
            '<(PRODUCT_DIR)/spidermonkey/Debug/dist/include'
          ],
        },
        {
          'action_name': 'symlink_spidermonkey_release',
          'inputs': [ '<(external_spidermonkey_release)' ],
          'outputs': [
            '<(PRODUCT_DIR)/spidermonkey/Release/dist/include',
          ],
          'action': [
            'ln', '-s', '<(external_spidermonkey_release)/dist/include',
            '<(PRODUCT_DIR)/spidermonkey/Release/dist/include'
          ],
        },
      ],

      'copies': [
        {
          'destination': '<(PRODUCT_DIR)/spidermonkey/Debug',
          'files': [ '<@(spidermonkey_binaries_debug)' ],
        },
        {
          'destination': '<(PRODUCT_DIR)/spidermonkey/Release',
          'files': [ '<@(spidermonkey_binaries_release)' ],
        },
      ],

      'all_dependent_settings': {
        'configurations': {
          'Release': {
            'defines': ['NDEBUG'],
            'library_dirs': [ '<(PRODUCT_DIR)/spidermonkey/Release' ],
            'include_dirs': [ '<(PRODUCT_DIR)/spidermonkey/Release/dist/include', ],
            'ldflags':      [ '<(PRODUCT_DIR)/spidermonkey/Release/icudata.o', ],
            'xcode_settings': {'OTHER_LDFLAGS': ['<(PRODUCT_DIR)/spidermonkey/Release/icudata.o']},
            'conditions': [
              ['external_spidermonkey_release_has_nspr == 1', {
                # Normally we'd use libraries here, but gyp doesn't allow us.
                'ldflags': [ '-lnspr4' ],
                'xcode_settings': {'OTHER_LDFLAGS': ['-lnspr4']},
              }],
            ],
          },
          'Debug': {
            'defines': ['DEBUG'],
            'library_dirs': [ '<(PRODUCT_DIR)/spidermonkey/Debug' ],
            'include_dirs': [ '<(PRODUCT_DIR)/spidermonkey/Debug/dist/include', ],
            'ldflags':      [ '<(PRODUCT_DIR)/spidermonkey/Debug/icudata.o', ],
            'xcode_settings': {'OTHER_LDFLAGS': ['<(PRODUCT_DIR)/spidermonkey/Debug/icudata.o']},
            'conditions': [
              ['external_spidermonkey_debug_has_nspr == 1', {
                # Normally we'd use libraries here, but gyp doesn't allow us.
                'ldflags': [ '-lnspr4' ],
                'xcode_settings': {'OTHER_LDFLAGS': ['-lnspr4']},
              }],
            ],
          },
        },
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
              '-lmozglue',
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
