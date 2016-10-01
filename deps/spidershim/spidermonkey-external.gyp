{
  'targets': [
    {
      'target_name': 'spidermonkey',
      'type': 'none',

      'all_dependent_settings': {
        'configurations': {
          'Release': {
            'defines': ['NDEBUG'],
            'include_dirs': [ '<(external_spidermonkey_release)/dist/include', ],
            'library_dirs': [
              '<(external_spidermonkey_release)/js/src',
              '<(external_spidermonkey_release)/dist/sdk/lib',
            ],
            'ldflags': [
              '<(external_spidermonkey_release)/config/external/icu/data/icudata.o',
            ],
            'xcode_settings': { 'OTHER_LDFLAGS': [
              '<(external_spidermonkey_release)/config/external/icu/data/icudata.o',
            ]},
            'conditions': [
              # Normally we'd use libraries here, but gyp doesn't allow us,
              # so we use ldflags instead.
              ['OS == "linux" and external_spidermonkey_release_has_nspr == 1', {
                'library_dirs': [
                  '<(external_spidermonkey_release)/config/external/nspr/pr',
                ],
              }],
              ['OS == "mac"', {
                'xcode_settings': { 'OTHER_LDFLAGS': [
                  '<(external_spidermonkey_release)/dist/bin/<(SHARED_LIB_PREFIX)mozglue<(SHARED_LIB_SUFFIX)',
                ]},
              }],
              ['OS == "mac" and external_spidermonkey_release_has_nspr == 1', {
                'xcode_settings': { 'OTHER_LDFLAGS': [
                  '<(external_spidermonkey_release)/dist/lib/<(SHARED_LIB_PREFIX)nspr4<(SHARED_LIB_SUFFIX)',
                ]},
              }],
            ],
          },
          'Debug': {
            'defines': ['DEBUG'],
            'include_dirs': [ '<(external_spidermonkey_debug)/dist/include', ],
            'library_dirs': [
              '<(external_spidermonkey_debug)/js/src',
              '<(external_spidermonkey_debug)/dist/sdk/lib',
            ],

            'ldflags': [
              '<(external_spidermonkey_debug)/config/external/icu/data/icudata.o',
            ],
            'xcode_settings': { 'OTHER_LDFLAGS': [
              '<(external_spidermonkey_debug)/config/external/icu/data/icudata.o',
            ]},
            'conditions': [
              # Normally we'd use libraries here, but gyp doesn't allow us,
              # so we use ldflags instead.
              ['OS == "linux" and external_spidermonkey_debug_has_nspr == 1', {
                'library_dirs': [
                  '<(external_spidermonkey_debug)/config/external/nspr/pr',
                ],
              }],
              ['OS == "mac"', {
                'xcode_settings': { 'OTHER_LDFLAGS': [
                  '<(external_spidermonkey_debug)/dist/bin/<(SHARED_LIB_PREFIX)mozglue<(SHARED_LIB_SUFFIX)',
                ]},
              }],
              ['OS == "mac" and external_spidermonkey_debug_has_nspr == 1', {
                'xcode_settings': { 'OTHER_LDFLAGS': [
                  '<(external_spidermonkey_debug)/dist/lib/<(SHARED_LIB_PREFIX)nspr4<(SHARED_LIB_SUFFIX)',
                ]},
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
          ['external_spidermonkey_release_has_nspr == 1 or external_spidermonkey_debug_has_nspr == 1', {
            'libraries': [ '-lnspr4' ],
            'xcode_settings': {'OTHER_LDFLAGS': ['-lnspr4']},
          }],
        ],
      },

    },
  ],
}
