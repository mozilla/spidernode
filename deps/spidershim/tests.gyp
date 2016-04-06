{
  'targets': [
    # TODO: The dependencies for the tests are backwards, we need to fix this.
    # This is currently required to get the tests to build without building the
    # whole tree.
    {
      'target_name': 'test-spidershim-hello-world',
      'type': 'executable',
      'include_dirs': [ '.' ],
      'dependencies': [
        'spidershim.gyp:spidershim', # XXX correct
        '../gtest/gtest.gyp:gtest',
      ],
      'sources': [
        'test/hello-world.cpp'
      ],
    },
  ],
}
