{
  'target_defaults': {
    'type': 'executable',
    'include_dirs': [ '.' ],
    'dependencies': [
      'spidershim.gyp:spidershim',
      '../gtest/gtest.gyp:gtest',
    ],
  },
  'targets': [
    {
      'target_name': 'hello-world',
      'sources': [ 'test/hello-world.cpp' ],
    },
    {
      'target_name': 'value',
      'sources': [ 'test/value.cc' ],
    },
  ],
}
