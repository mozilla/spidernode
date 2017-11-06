// Flags: --expose-internals
'use strict';

require('../common');

// Test coverage for the updateOptionsBuffer method used internally
// by the http2 implementation.

const { updateOptionsBuffer } = require('internal/http2/util');
const { optionsBuffer } = process.binding('http2');
const { ok, strictEqual } = require('assert');

const IDX_OPTIONS_MAX_DEFLATE_DYNAMIC_TABLE_SIZE = 0;
const IDX_OPTIONS_MAX_RESERVED_REMOTE_STREAMS = 1;
const IDX_OPTIONS_MAX_SEND_HEADER_BLOCK_LENGTH = 2;
const IDX_OPTIONS_PEER_MAX_CONCURRENT_STREAMS = 3;
const IDX_OPTIONS_PADDING_STRATEGY = 4;
const IDX_OPTIONS_MAX_HEADER_LIST_PAIRS = 5;
const IDX_OPTIONS_FLAGS = 6;

{
  updateOptionsBuffer({
    maxDeflateDynamicTableSize: 1,
    maxReservedRemoteStreams: 2,
    maxSendHeaderBlockLength: 3,
    peerMaxConcurrentStreams: 4,
    paddingStrategy: 5,
    maxHeaderListPairs: 6
  });

  strictEqual(optionsBuffer[IDX_OPTIONS_MAX_DEFLATE_DYNAMIC_TABLE_SIZE], 1);
  strictEqual(optionsBuffer[IDX_OPTIONS_MAX_RESERVED_REMOTE_STREAMS], 2);
  strictEqual(optionsBuffer[IDX_OPTIONS_MAX_SEND_HEADER_BLOCK_LENGTH], 3);
  strictEqual(optionsBuffer[IDX_OPTIONS_PEER_MAX_CONCURRENT_STREAMS], 4);
  strictEqual(optionsBuffer[IDX_OPTIONS_PADDING_STRATEGY], 5);
  strictEqual(optionsBuffer[IDX_OPTIONS_MAX_HEADER_LIST_PAIRS], 6);

  const flags = optionsBuffer[IDX_OPTIONS_FLAGS];

  ok(flags & (1 << IDX_OPTIONS_MAX_DEFLATE_DYNAMIC_TABLE_SIZE));
  ok(flags & (1 << IDX_OPTIONS_MAX_RESERVED_REMOTE_STREAMS));
  ok(flags & (1 << IDX_OPTIONS_MAX_SEND_HEADER_BLOCK_LENGTH));
  ok(flags & (1 << IDX_OPTIONS_PEER_MAX_CONCURRENT_STREAMS));
  ok(flags & (1 << IDX_OPTIONS_PADDING_STRATEGY));
  ok(flags & (1 << IDX_OPTIONS_MAX_HEADER_LIST_PAIRS));
}

{
  optionsBuffer[IDX_OPTIONS_MAX_SEND_HEADER_BLOCK_LENGTH] = 0;

  updateOptionsBuffer({
    maxDeflateDynamicTableSize: 1,
    maxReservedRemoteStreams: 2,
    peerMaxConcurrentStreams: 4,
    paddingStrategy: 5,
    maxHeaderListPairs: 6
  });

  strictEqual(optionsBuffer[IDX_OPTIONS_MAX_DEFLATE_DYNAMIC_TABLE_SIZE], 1);
  strictEqual(optionsBuffer[IDX_OPTIONS_MAX_RESERVED_REMOTE_STREAMS], 2);
  strictEqual(optionsBuffer[IDX_OPTIONS_MAX_SEND_HEADER_BLOCK_LENGTH], 0);
  strictEqual(optionsBuffer[IDX_OPTIONS_PEER_MAX_CONCURRENT_STREAMS], 4);
  strictEqual(optionsBuffer[IDX_OPTIONS_PADDING_STRATEGY], 5);
  strictEqual(optionsBuffer[IDX_OPTIONS_MAX_HEADER_LIST_PAIRS], 6);

  const flags = optionsBuffer[IDX_OPTIONS_FLAGS];

  ok(flags & (1 << IDX_OPTIONS_MAX_DEFLATE_DYNAMIC_TABLE_SIZE));
  ok(flags & (1 << IDX_OPTIONS_MAX_RESERVED_REMOTE_STREAMS));
  ok(!(flags & (1 << IDX_OPTIONS_MAX_SEND_HEADER_BLOCK_LENGTH)));
  ok(flags & (1 << IDX_OPTIONS_PEER_MAX_CONCURRENT_STREAMS));
  ok(flags & (1 << IDX_OPTIONS_PADDING_STRATEGY));
  ok(flags & (1 << IDX_OPTIONS_MAX_HEADER_LIST_PAIRS));
}
