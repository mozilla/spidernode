'use strict';
const common = require('../common');
const assert = require('assert');

const http = require('http');
const OutgoingMessage = http.OutgoingMessage;
const ClientRequest = http.ClientRequest;
const ServerResponse = http.ServerResponse;

common.expectsError(
  OutgoingMessage.prototype._implicitHeader,
  {
    code: 'ERR_METHOD_NOT_IMPLEMENTED',
    type: Error,
    message: 'The _implicitHeader() method is not implemented'
  }
);
assert.strictEqual(
  typeof ClientRequest.prototype._implicitHeader, 'function');
assert.strictEqual(
  typeof ServerResponse.prototype._implicitHeader, 'function');

// validateHeader
assert.throws(() => {
  const outgoingMessage = new OutgoingMessage();
  outgoingMessage.setHeader();
}, common.expectsError({
  code: 'ERR_INVALID_HTTP_TOKEN',
  type: TypeError,
  message: 'Header name must be a valid HTTP token ["undefined"]'
}));

assert.throws(() => {
  const outgoingMessage = new OutgoingMessage();
  outgoingMessage.setHeader('test');
}, common.expectsError({
  code: 'ERR_HTTP_INVALID_HEADER_VALUE',
  type: TypeError,
  message: 'Invalid value "undefined" for header "test"'
}));

assert.throws(() => {
  const outgoingMessage = new OutgoingMessage();
  outgoingMessage.setHeader(404);
}, common.expectsError({
  code: 'ERR_INVALID_HTTP_TOKEN',
  type: TypeError,
  message: 'Header name must be a valid HTTP token ["404"]'
}));

assert.throws(() => {
  const outgoingMessage = new OutgoingMessage();
  outgoingMessage.setHeader.call({ _header: 'test' }, 'test', 'value');
}, common.expectsError({
  code: 'ERR_HTTP_HEADERS_SENT',
  type: Error,
  message: 'Cannot set headers after they are sent to the client'
}));

assert.throws(() => {
  const outgoingMessage = new OutgoingMessage();
  outgoingMessage.setHeader('200', 'あ');
}, common.expectsError({
  code: 'ERR_INVALID_CHAR',
  type: TypeError,
  message: 'Invalid character in header content ["200"]'
}));

// write
assert.throws(() => {
  const outgoingMessage = new OutgoingMessage();
  outgoingMessage.write();
}, common.expectsError({
  code: 'ERR_METHOD_NOT_IMPLEMENTED',
  type: Error,
  message: 'The _implicitHeader() method is not implemented'
}));

assert(OutgoingMessage.prototype.write.call({ _header: 'test' }));

assert.throws(() => {
  const outgoingMessage = new OutgoingMessage();
  outgoingMessage.write.call({ _header: 'test', _hasBody: 'test' });
}, common.expectsError({
  code: 'ERR_INVALID_ARG_TYPE',
  type: TypeError,
  message: 'The first argument must be one of type string or Buffer'
}));

assert.throws(() => {
  const outgoingMessage = new OutgoingMessage();
  outgoingMessage.write.call({ _header: 'test', _hasBody: 'test' }, 1);
}, common.expectsError({
  code: 'ERR_INVALID_ARG_TYPE',
  type: TypeError,
  message: 'The first argument must be one of type string or Buffer'
}));

// addTrailers()
// The `Error` comes from the JavaScript engine so confirm that it is a
// `TypeError` but do not check the message. It will be different in different
// JavaScript engines.
assert.throws(() => {
  const outgoingMessage = new OutgoingMessage();
  outgoingMessage.addTrailers();
}, TypeError);

assert.throws(() => {
  const outgoingMessage = new OutgoingMessage();
  outgoingMessage.addTrailers({ 'あ': 'value' });
}, common.expectsError({
  code: 'ERR_INVALID_HTTP_TOKEN',
  type: TypeError,
  message: 'Trailer name must be a valid HTTP token ["あ"]'
}));

assert.throws(() => {
  const outgoingMessage = new OutgoingMessage();
  outgoingMessage.addTrailers({ 404: 'あ' });
}, common.expectsError({
  code: 'ERR_INVALID_CHAR',
  type: TypeError,
  message: 'Invalid character in trailer content ["404"]'
}));
