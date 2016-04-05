{
  'variables': {
    'spidermonkey_dir%': 'spidermonkey',
  },

  'targets': [
    {
      'target_name': 'spidershim',
      'type': '<(library)',
      'dependencies': [
        'tests.gyp:test-spidershim-hello-world', # XXX incorrect
      ],

      'include_dirs': [
        'include',
        '<(spidermonkey_dir)/../build/dist/include',
      ],
      'conditions': [
        [ 'target_arch=="ia32"', { 'defines': [ '__i386__=1' ] } ],
        [ 'target_arch=="x64"', { 'defines': [ '__x86_64__=1' ] } ],
        [ 'target_arch=="arm"', { 'defines': [ '__arm__=1' ] } ],
        ['node_engine=="spidermonkey"', {
          'dependencies': [
            'spidermonkey.gyp:spidermonkey',
          ],
          'export_dependent_settings': [
            'spidermonkey.gyp:spidermonkey',
          ],
        }],
      ],

      'direct_dependent_settings': {
        'include_dirs': [
          'include',
        ],
        'libraries': [
          '-lspidershim',
          '<@(node_engine_libs)',
        ],
        'conditions': [
          [ 'target_arch=="arm"', {
            'defines': [ '__arm__=1' ]
          }],
        ],
      },

      'sources': [
        'src/v8context.cc',
        'src/v8handlescope.cc',
        'src/v8isolate.cc',
        'src/v8script.cc',
        'src/v8string.cc',
        'src/v8v8.cc',
        'src/v8value.cc',
      ],
    },
  ],
}
