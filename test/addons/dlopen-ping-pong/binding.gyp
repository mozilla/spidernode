{
  'targets': [
    {
      'target_name': 'ping',
      'product_extension': 'so',
      'type': 'shared_library',
      'sources': [ 'ping.c' ],
      'conditions': [
        ['OS=="mac"', {
          'xcode_settings': {
            'OTHER_LDFLAGS': [ '-Wl,-undefined', '-Wl,dynamic_lookup' ]
        }}],
        # Enable the shared object to be linked by runtime linker
        ['OS=="aix"', {
          'ldflags': [ '-Wl,-G' ]
        }]],
    },
    {
      'target_name': 'binding',
      'defines': [ 'V8_DEPRECATION_WARNINGS=1' ],
      'sources': [ 'binding.cc' ],
    }
  ]
}
