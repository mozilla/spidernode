{
  'targets': [
    {
      'target_name': 'spidermonkey',
      'type': 'none',

      'all_dependent_settings': {
        'configurations': {
          'Release': {
            'defines': ['NDEBUG'],
            'ldflags': [
              '<(external_spidermonkey_release)/js/src/<(STATIC_LIB_PREFIX)js_static<(STATIC_LIB_SUFFIX)',
              '<(external_spidermonkey_release)/config/external/icu/data/icudata.o',
            ],
            'xcode_settings': { 'OTHER_LDFLAGS': [
              '<(external_spidermonkey_release)/js/src/<(STATIC_LIB_PREFIX)js_static<(STATIC_LIB_SUFFIX)',
              '<(external_spidermonkey_release)/config/external/icu/data/icudata.o',
            ]},
            'include_dirs': [ '<(external_spidermonkey_release)/dist/include', ],
            'conditions': [
              # Normally we'd use libraries here, but gyp doesn't allow us,
              # so we use ldflags instead.
              ['external_spidermonkey_release_has_nss == 1', {
                'ldflags': [
                  '<(external_spidermonkey_release)/dist/lib/<(SHARED_LIB_PREFIX)nss3<(SHARED_LIB_SUFFIX)',
                ],
                'xcode_settings': { 'OTHER_LDFLAGS': [
                  '<(external_spidermonkey_release)/dist/lib/<(SHARED_LIB_PREFIX)nss3<(SHARED_LIB_SUFFIX)',
                ]},
              }],
              ['OS == "linux"', {
                'ldflags+': [
                  '<(external_spidermonkey_release)/mozglue/build/<(STATIC_LIB_PREFIX)mozglue<(STATIC_LIB_SUFFIX)',
                ],
              }],
              ['OS == "linux" and external_spidermonkey_release_has_nspr == 1', {
                'ldflags+': [
                  '<(external_spidermonkey_release)/config/external/nspr/pr/<(SHARED_LIB_PREFIX)nspr4<(SHARED_LIB_SUFFIX)',
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
              ['external_spidermonkey_release_has_nspr == 1', {
                'ldflags': [ '-lnspr4' ],
                'xcode_settings': {'OTHER_LDFLAGS': ['-lnspr4']},
              }],
            ],
          },
          'Debug': {
            'defines': ['DEBUG'],
            'include_dirs': [ '<(external_spidermonkey_debug)/dist/include', ],
            'ldflags': [
              '<(external_spidermonkey_debug)/js/src/<(STATIC_LIB_PREFIX)js_static<(STATIC_LIB_SUFFIX)',
              '<(external_spidermonkey_debug)/config/external/icu/data/icudata.o',
            ],
            'xcode_settings': { 'OTHER_LDFLAGS': [
              '<(external_spidermonkey_debug)/js/src/<(STATIC_LIB_PREFIX)js_static<(STATIC_LIB_SUFFIX)',
              '<(external_spidermonkey_debug)/config/external/icu/data/icudata.o',
            ]},
            'conditions': [
              # Normally we'd use libraries here, but gyp doesn't allow us,
              # so we use ldflags instead.
              ['external_spidermonkey_debug_has_nss == 1', {
                'ldflags': [
                  '<(external_spidermonkey_debug)/dist/lib/<(SHARED_LIB_PREFIX)nss3<(SHARED_LIB_SUFFIX)',
                ],
                'xcode_settings': { 'OTHER_LDFLAGS': [
                  '<(external_spidermonkey_debug)/dist/lib/<(SHARED_LIB_PREFIX)nss3<(SHARED_LIB_SUFFIX)',
                ]},
              }],
              ['OS == "linux"', {
                'ldflags+': [
                  '<(external_spidermonkey_debug)/mozglue/build/<(STATIC_LIB_PREFIX)mozglue<(STATIC_LIB_SUFFIX)',
                ],
              }],
              ['OS == "linux" and external_spidermonkey_debug_has_nspr == 1', {
                'ldflags+': [
                  '<(external_spidermonkey_debug)/config/external/nspr/pr/<(SHARED_LIB_PREFIX)nspr4<(SHARED_LIB_SUFFIX)',
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
              ['external_spidermonkey_debug_has_nspr == 1', {
                # Normally we'd use libraries here, but gyp doesn't allow us.
                'ldflags': [ '-lnspr4' ],
                'xcode_settings': {'OTHER_LDFLAGS': ['-lnspr4']},
              }],
            ],
          },
        },
        'libraries': [
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
            ],
          }],
        ],
      },

    },
  ],
}
