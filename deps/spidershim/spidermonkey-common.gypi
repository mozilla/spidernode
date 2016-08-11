{
  'variables': {
    'spidermonkey_binaries': [
      '<(spidermonkey_objdir)/js/src/<(STATIC_LIB_PREFIX)js_static<(STATIC_LIB_SUFFIX)',
    ],
    'conditions': [
      ['OS == "linux"', {
        'spidermonkey_binaries+': [
          '<(spidermonkey_objdir)/mozglue/build/<(STATIC_LIB_PREFIX)mozglue<(STATIC_LIB_SUFFIX)',
        ],
      }],
      ['OS == "mac"', {
        'spidermonkey_binaries': [
          '<(spidermonkey_objdir)/dist/bin/<(SHARED_LIB_PREFIX)mozglue<(SHARED_LIB_SUFFIX)',
        ],
      }],
    ],
  },

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
      '<(spidermonkey_objdir)/dist/include',
    ],
  },
}
