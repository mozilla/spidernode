// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @typedef {{
        type: string,
        object: !Object,
        name: (string|undefined),
        startLocation: (!RawLocation|undefined),
        endLocation: (!RawLocation|undefined)
    }} */
var Scope;

/** @typedef {{
        scriptId: string,
        lineNumber: number,
        columnNumber: number
    }} */
var RawLocation;

/** @typedef {{
        functionName: string,
        location: !RawLocation,
        this: !Object,
        scopeChain: !Array<!Scope>,
        functionLocation: (RawLocation|undefined),
        returnValue: (*|undefined)
    }} */
var JavaScriptCallFrameDetails;

/** @typedef {{
        contextId: function():number,
        thisObject: !Object,
        evaluate: function(string, boolean):*,
        restart: function():undefined,
        setVariableValue: function(number, string, *):undefined,
        isAtReturn: boolean,
        details: function():!JavaScriptCallFrameDetails
    }} */
var JavaScriptCallFrame;

/**
 * @const
 */
var Debug = {};

Debug.clearAllBreakPoints = function() {}

/** @return {!Array<!Script>} */
Debug.scripts = function() {}

/**
 * @param {number} scriptId
 * @param {number=} line
 * @param {number=} column
 * @param {string=} condition
 * @param {string=} groupId
 */
Debug.setScriptBreakPointById = function(scriptId, line, column, condition, groupId) {}

/**
 * @param {number} breakId
 * @return {!Array<!SourceLocation>}
 */
Debug.findBreakPointActualLocations = function(breakId) {}

/**
 * @param {number} breakId
 * @param {boolean} remove
 * @return {!BreakPoint|undefined}
 */
Debug.findBreakPoint = function(breakId, remove) {}

/** @const */
var LiveEdit = {}

/**
 * @param {!Script} script
 * @param {string} newSource
 * @param {boolean} previewOnly
 * @return {!{stack_modified: (boolean|undefined)}}
 */
LiveEdit.SetScriptSource = function(script, newSource, previewOnly, change_log) {}

/** @constructor */
function Failure() {}
LiveEdit.Failure = Failure;

Debug.LiveEdit = LiveEdit;

/** @typedef {{
 *    type: string,
 *    syntaxErrorMessage: string,
 *    position: !{start: !{line: number, column: number}},
 *  }}
 */
var LiveEditErrorDetails;

/** @typedef {{
 *    breakpointId: number,
 *    sourceID: number,
 *    lineNumber: (number|undefined),
 *    columnNumber: (number|undefined),
 *    condition: (string|undefined),
 *    interstatementLocation: (boolean|undefined),
 *    }}
 */
var BreakpointInfo;


/** @interface */
function BreakPoint() {}

/** @return {!BreakPoint|undefined} */
BreakPoint.prototype.script_break_point = function() {}

/** @return {number} */
BreakPoint.prototype.number = function() {}


/** @interface */
function ExecutionState() {}

/**
 * @param {string} source
 */
ExecutionState.prototype.evaluateGlobal = function(source) {}

/** @return {number} */
ExecutionState.prototype.frameCount = function() {}

/**
 * @param {number} index
 * @return {!FrameMirror}
 */
ExecutionState.prototype.frame = function(index) {}

/** @param {number} index */
ExecutionState.prototype.setSelectedFrame = function(index) {}

/** @return {number} */
ExecutionState.prototype.selectedFrame = function() {}


/** @enum */
var ScopeType = { Global: 0,
                  Local: 1,
                  With: 2,
                  Closure: 3,
                  Catch: 4,
                  Block: 5,
                  Script: 6,
                  Eval: 7,
                  Module: 8};


/** @typedef {{
 *    script: number,
 *    position: number,
 *    line: number,
 *    column:number,
 *    start: number,
 *    end: number,
 *    }}
 */
var SourceLocation;

/** @typedef{{
 *    id: number,
 *    context_data: (string|undefined),
 *    }}
 */
var Script;

/** @interface */
function ScopeDetails() {}

/** @return {!Object} */
ScopeDetails.prototype.object = function() {}

/** @return {string|undefined} */
ScopeDetails.prototype.name = function() {}

/** @return {number} */
ScopeDetails.prototype.type = function() {}


/** @interface */
function FrameDetails() {}

/** @return {!Object} */
FrameDetails.prototype.receiver = function() {}

/** @return {function()} */
FrameDetails.prototype.func = function() {}

/** @return {!Object} */
FrameDetails.prototype.script = function() {}

/** @return {boolean} */
FrameDetails.prototype.isAtReturn = function() {}

/** @return {number} */
FrameDetails.prototype.sourcePosition = function() {}

/** @return {*} */
FrameDetails.prototype.returnValue = function() {}

/** @return {number} */
FrameDetails.prototype.scopeCount = function() {}

/**
 * @param {*} value
 * @return {!Mirror}
 */
function MakeMirror(value) {}


/** @interface */
function Mirror() {}

/** @return {boolean} */
Mirror.prototype.isFunction = function() {}

/** @return {boolean} */
Mirror.prototype.isGenerator = function() {}

/**
 * @interface
 * @extends {Mirror}
 */
function ObjectMirror() {}

/** @return {!Array<!PropertyMirror>} */
ObjectMirror.prototype.properties = function() {}


/**
 * @interface
 * @extends {ObjectMirror}
 */
function FunctionMirror () {}

/** @return {number} */
FunctionMirror.prototype.scopeCount = function() {}

/**
 * @param {number} index
 * @return {!ScopeMirror|undefined}
 */
FunctionMirror.prototype.scope = function(index) {}

/** @return {boolean} */
FunctionMirror.prototype.resolved = function() {}

/** @return {function()} */
FunctionMirror.prototype.value = function() {}

/** @return {string} */
FunctionMirror.prototype.debugName = function() {}

/** @return {!ScriptMirror|undefined} */
FunctionMirror.prototype.script = function() {}

/** @return {!SourceLocation|undefined} */
FunctionMirror.prototype.sourceLocation = function() {}

/** @return {!ContextMirror|undefined} */
FunctionMirror.prototype.context = function() {}

/**
 * @constructor
 * @param {*} value
 */
function UnresolvedFunctionMirror(value) {}

/**
 * @interface
 * @extends {ObjectMirror}
 */
function GeneratorMirror () {}

/** @return {number} */
GeneratorMirror.prototype.scopeCount = function() {}

/**
 * @param {number} index
 * @return {!ScopeMirror|undefined}
 */
GeneratorMirror.prototype.scope = function(index) {}


/**
 * @interface
 * @extends {Mirror}
 */
function PropertyMirror() {}

/** @return {!Mirror} */
PropertyMirror.prototype.value = function() {}

/** @return {string} */
PropertyMirror.prototype.name = function() {}

/** @type {*} */
PropertyMirror.prototype.value_;

/**
 * @interface
 * @extends {Mirror}
 */
function FrameMirror() {}

/**
 * @param {boolean=} ignoreNestedScopes
 * @return {!Array<!ScopeMirror>}
 */
FrameMirror.prototype.allScopes = function(ignoreNestedScopes) {}

/** @return {!FrameDetails} */
FrameMirror.prototype.details = function() {}

/** @return {!ScriptMirror} */
FrameMirror.prototype.script = function() {}

/**
 * @param {string} source
 * @param {boolean} throwOnSideEffect
 */
FrameMirror.prototype.evaluate = function(source, throwOnSideEffect) {}

FrameMirror.prototype.restart = function() {}

/** @param {number} index */
FrameMirror.prototype.scope = function(index) {}


/**
 * @interface
 * @extends {Mirror}
 */
function ScriptMirror() {}

/** @return {!Script} */
ScriptMirror.prototype.value = function() {}

/** @return {number} */
ScriptMirror.prototype.id = function() {}

/** @return {ContextMirror} */
ScriptMirror.prototype.context = function() {}

/**
 * @param {number} position
 * @param {boolean=} includeResourceOffset
 */
ScriptMirror.prototype.locationFromPosition = function(position, includeResourceOffset) {}


/**
 * @interface
 * @extends {Mirror}
 */
function ScopeMirror() {}

/** @return {!ScopeDetails} */
ScopeMirror.prototype.details = function() {}

/**
 * @param {string} name
 * @param {*} newValue
 */
ScopeMirror.prototype.setVariableValue = function(name, newValue) {}

/**
 * @interface
 * @extends {Mirror}
 */
function ContextMirror() {}

/** @return {string|undefined} */
ContextMirror.prototype.data = function() {}
