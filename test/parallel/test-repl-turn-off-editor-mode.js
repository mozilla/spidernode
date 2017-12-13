'use strict';
const common = require('../common');
const repl = require('repl');

testTurnOffEditorMode();

function testTurnOffEditorMode() {
  const server = repl.start({ prompt: '> ' });
  const warn = 'REPLServer.turnOffEditorMode() is deprecated';

  common.expectWarning('DeprecationWarning', warn);
  server.turnOffEditorMode();
  server.close();
}
