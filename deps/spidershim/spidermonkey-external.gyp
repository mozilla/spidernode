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
              '<(external_spidermonkey_release)/js/src/build',
              '<(external_spidermonkey_release)/mozglue/build',
            ],
            'ldflags': [
              '<(external_spidermonkey_release)/config/external/icu/data/icudata.o',
            ],
            'xcode_settings': { 'OTHER_LDFLAGS': [
              '<(external_spidermonkey_release)/config/external/icu/data/icudata.o',
            ]},
            'conditions': [
              ['OS == "linux" and external_spidermonkey_has_nspr == 1', {
                'library_dirs': [
                  '<(external_spidermonkey_release)/config/external/nspr/pr',
                ],
              }],
              ['OS == "mac" and external_spidermonkey_has_nspr == 1', {
                # On Mac, MOZ_FOLD_LIBS is defined by default, and NSPR is
                # folded into NSS, so dist/lib/libnspr4.dylib is just a link
                # to config/external/nss/libnss3.dylib, and we would be more
                # precise to specify config/external/nss as the library dir
                # and -lnss3 as the library.
                #
                # But specifying dist/lib and -lnspr4 is simpler and accounts
                # for the possibility that someone builds external SpiderMonkey
                # without MOZ_FOLD_LIBS.
                #
                'library_dirs': [
                  '<(external_spidermonkey_release)/dist/lib',
                ],
              }],
            ],
          },
          'Debug': {
            'defines': ['DEBUG'],
            'include_dirs': [ '<(external_spidermonkey_debug)/dist/include', ],
            'library_dirs': [
              '<(external_spidermonkey_debug)/js/src/build',
              '<(external_spidermonkey_debug)/mozglue/build',
            ],

            'ldflags': [
              '<(external_spidermonkey_debug)/config/external/icu/data/icudata.o',
            ],
            'xcode_settings': { 'OTHER_LDFLAGS': [
              '<(external_spidermonkey_debug)/config/external/icu/data/icudata.o',
            ]},
            'conditions': [
              ['OS == "linux" and external_spidermonkey_has_nspr == 1', {
                'library_dirs': [
                  '<(external_spidermonkey_debug)/config/external/nspr/pr',
                ],
              }],
              ['OS == "mac" and external_spidermonkey_has_nspr == 1', {
                # On Mac, MOZ_FOLD_LIBS is defined by default, and NSPR is
                # folded into NSS, so dist/lib/libnspr4.dylib is just a link
                # to config/external/nss/libnss3.dylib, and we would be more
                # precise to specify config/external/nss as the library dir
                # and -lnss3 as the library.
                #
                # But specifying dist/lib and -lnspr4 is simpler and accounts
                # for the possibility that someone builds external SpiderMonkey
                # without MOZ_FOLD_LIBS.
                #
                'library_dirs': [
                  '<(external_spidermonkey_debug)/dist/lib',
                ],
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
              '-Wl,--whole-archive -lmozglue -Wl,--no-whole-archive',
              '-ldl',
              '-lrt',
            ],
          }],
          ['OS != "linux"', {
            'libraries': [
              '-lmozglue',
            ],
          }],
          ['external_spidermonkey_has_nspr == 1', {
            'libraries': [ '-lnspr4' ],
          }],
        ],
      },

    },
  ],
}
