{
  'target_defaults': {
    'type': 'executable',
    'include_dirs': [ 'test' ],
    'dependencies': [
      'spidershim.gyp:spidershim',
      '../gtest/gtest.gyp:gtest',
    ],
  },
  'targets': [
    {
      'target_name': 'exception',
      'sources': [ 'test/exception.cc' ],
    },
    {
      'target_name': 'hello-world',
      'sources': [ 'test/hello-world.cc' ],
    },
    {
      'target_name': 'persistent',
      'sources': [ 'test/persistent.cc' ],
    },
    {
      'target_name': 'trycatch',
      'sources': [ 'test/trycatch.cc' ],
    },
    {
      'target_name': 'value',
      'sources': [ 'test/value.cc' ],
    },
    {
      'target_name': 'v8',
      'sources': [ 'test/v8.cc' ],
    },
    {
      'target_name': 'isolate',
      'sources': [ 'test/isolate.cc' ],
    },
    {
      'target_name': 'template',
      'sources': [ 'test/template.cc' ],
    },
  ],
}
