/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* JS parser. */

#ifndef frontend_Parser_h
#define frontend_Parser_h

#include "mozilla/Array.h"
#include "mozilla/Maybe.h"
#include "mozilla/TypeTraits.h"

#include "jsiter.h"
#include "jspubtd.h"

#include "ds/Nestable.h"
#include "frontend/BytecodeCompiler.h"
#include "frontend/FullParseHandler.h"
#include "frontend/LanguageExtensions.h"
#include "frontend/NameAnalysisTypes.h"
#include "frontend/NameCollections.h"
#include "frontend/ParseContext.h"
#include "frontend/SharedContext.h"
#include "frontend/SyntaxParseHandler.h"

namespace js {

class ModuleObject;

namespace frontend {

class ParserBase;

template <class ParseHandler, typename CharT> class Parser;

class SourceParseContext: public ParseContext
{
public:
    template<typename ParseHandler, typename CharT>
    SourceParseContext(Parser<ParseHandler, CharT>* prs, SharedContext* sc, Directives* newDirectives)
        : ParseContext(prs->context, prs->pc, sc, prs->tokenStream,
            prs->usedNames, newDirectives, mozilla::IsSame<ParseHandler,
            FullParseHandler>::value)
    { }
};

template <>
inline bool
ParseContext::Statement::is<ParseContext::LabelStatement>() const
{
    return kind_ == StatementKind::Label;
}

template <>
inline bool
ParseContext::Statement::is<ParseContext::ClassStatement>() const
{
    return kind_ == StatementKind::Class;
}

template <typename T>
inline T&
ParseContext::Statement::as()
{
    MOZ_ASSERT(is<T>());
    return static_cast<T&>(*this);
}

inline ParseContext::Scope::BindingIter
ParseContext::Scope::bindings(ParseContext* pc)
{
    // In function scopes with parameter expressions, function special names
    // (like '.this') are declared as vars in the function scope, despite its
    // not being the var scope.
    return BindingIter(*this, pc->varScope_ == this || pc->functionScope_.ptrOr(nullptr) == this);
}

inline
Directives::Directives(ParseContext* parent)
  : strict_(parent->sc()->strict()),
    asmJS_(parent->useAsmOrInsideUseAsm())
{}

enum VarContext { HoistVars, DontHoistVars };
enum PropListType { ObjectLiteral, ClassBody, DerivedClassBody };
enum class PropertyType {
    Normal,
    Shorthand,
    CoverInitializedName,
    Getter,
    GetterNoExpressionClosure,
    Setter,
    SetterNoExpressionClosure,
    Method,
    GeneratorMethod,
    AsyncMethod,
    AsyncGeneratorMethod,
    Constructor,
    DerivedConstructor
};

// Specify a value for an ES6 grammar parametrization.  We have no enum for
// [Return] because its behavior is exactly equivalent to checking whether
// we're in a function box -- easier and simpler than passing an extra
// parameter everywhere.
enum YieldHandling { YieldIsName, YieldIsKeyword };
enum AwaitHandling : uint8_t { AwaitIsName, AwaitIsKeyword, AwaitIsModuleKeyword };
enum InHandling { InAllowed, InProhibited };
enum DefaultHandling { NameRequired, AllowDefaultName };
enum TripledotHandling { TripledotAllowed, TripledotProhibited };


template <class Parser>
class AutoAwaitIsKeyword;

class ParserBase : public StrictModeGetter
{
  private:
    ParserBase* thisForCtor() { return this; }

  public:
    JSContext* const context;

    LifoAlloc& alloc;

    TokenStream tokenStream;
    LifoAlloc::Mark tempPoolMark;

    /* list of parsed objects for GC tracing */
    ObjectBox* traceListHead;

    /* innermost parse context (stack-allocated) */
    ParseContext* pc;

    // For tracking used names in this parsing session.
    UsedNameTracker& usedNames;

    ScriptSource*       ss;

    /* Root atoms and objects allocated for the parsed tree. */
    AutoKeepAtoms       keepAtoms;

    /* Perform constant-folding; must be true when interfacing with the emitter. */
    const bool          foldConstants:1;

  protected:
#if DEBUG
    /* Our fallible 'checkOptions' member function has been called. */
    bool checkOptionsCalled:1;
#endif

    /* Unexpected end of input, i.e. TOK_EOF not at top-level. */
    bool isUnexpectedEOF_:1;

    /* AwaitHandling */ uint8_t awaitHandling_:2;

  public:
    bool awaitIsKeyword() const {
      return awaitHandling_ != AwaitIsName;
    }

    ParserBase(JSContext* cx, LifoAlloc& alloc, const ReadOnlyCompileOptions& options,
               const char16_t* chars, size_t length, bool foldConstants,
               UsedNameTracker& usedNames, LazyScript* lazyOuterFunction);
    ~ParserBase();

    const char* getFilename() const { return tokenStream.getFilename(); }
    JSVersion versionNumber() const { return tokenStream.versionNumber(); }
    TokenPos pos() const { return tokenStream.currentToken().pos; }

    // Determine whether |yield| is a valid name in the current context, or
    // whether it's prohibited due to strictness, JS version, or occurrence
    // inside a star generator.
    bool yieldExpressionsSupported() {
        return (versionNumber() >= JSVERSION_1_7 && !pc->isAsync()) ||
               pc->isStarGenerator() ||
               pc->isLegacyGenerator();
    }

    virtual bool strictMode() { return pc->sc()->strict(); }
    bool setLocalStrictMode(bool strict) {
        MOZ_ASSERT(tokenStream.debugHasNoLookahead());
        return pc->sc()->setLocalStrictMode(strict);
    }

    const ReadOnlyCompileOptions& options() const {
        return tokenStream.options();
    }

    bool isUnexpectedEOF() const { return isUnexpectedEOF_; }

    bool reportNoOffset(ParseReportKind kind, bool strict, unsigned errorNumber, ...);

    /* Report the given error at the current offset. */
    void error(unsigned errorNumber, ...);
    void errorWithNotes(UniquePtr<JSErrorNotes> notes, unsigned errorNumber, ...);

    /* Report the given error at the given offset. */
    void errorAt(uint32_t offset, unsigned errorNumber, ...);
    void errorWithNotesAt(UniquePtr<JSErrorNotes> notes, uint32_t offset,
                          unsigned errorNumber, ...);

    /*
     * Handle a strict mode error at the current offset.  Report an error if in
     * strict mode code, or warn if not, using the given error number and
     * arguments.
     */
    MOZ_MUST_USE bool strictModeError(unsigned errorNumber, ...);

    /*
     * Handle a strict mode error at the given offset.  Report an error if in
     * strict mode code, or warn if not, using the given error number and
     * arguments.
     */
    MOZ_MUST_USE bool strictModeErrorAt(uint32_t offset, unsigned errorNumber, ...);

    /* Report the given warning at the current offset. */
    MOZ_MUST_USE bool warning(unsigned errorNumber, ...);

    /* Report the given warning at the given offset. */
    MOZ_MUST_USE bool warningAt(uint32_t offset, unsigned errorNumber, ...);

    /*
     * If extra warnings are enabled, report the given warning at the current
     * offset.
     */
    MOZ_MUST_USE bool extraWarning(unsigned errorNumber, ...);

    /*
     * If extra warnings are enabled, report the given warning at the given
     * offset.
     */
    MOZ_MUST_USE bool extraWarningAt(uint32_t offset, unsigned errorNumber, ...);

    bool isValidStrictBinding(PropertyName* name);

    void addTelemetry(DeprecatedLanguageExtension e);

    bool warnOnceAboutExprClosure();
    bool warnOnceAboutForEach();
    bool warnOnceAboutLegacyGenerator();

    bool allowsForEachIn() {
#if !JS_HAS_FOR_EACH_IN
        return false;
#else
        return options().forEachStatementOption && versionNumber() >= JSVERSION_1_6;
#endif
    }

    bool hasValidSimpleStrictParameterNames();


    /*
     * Create a new function object given a name (which is optional if this is
     * a function expression).
     */
    JSFunction* newFunction(HandleAtom atom, FunctionSyntaxKind kind,
                            GeneratorKind generatorKind, FunctionAsyncKind asyncKind,
                            HandleObject proto);

    // A Parser::Mark is the extension of the LifoAlloc::Mark to the entire
    // Parser's state. Note: clients must still take care that any ParseContext
    // that points into released ParseNodes is destroyed.
    class Mark
    {
        friend class ParserBase;
        LifoAlloc::Mark mark;
        ObjectBox* traceListHead;
    };
    Mark mark() const {
        Mark m;
        m.mark = alloc.mark();
        m.traceListHead = traceListHead;
        return m;
    }
    void release(Mark m) {
        alloc.release(m.mark);
        traceListHead = m.traceListHead;
    }

    ObjectBox* newObjectBox(JSObject* obj);

    mozilla::Maybe<GlobalScope::Data*> newGlobalScopeData(ParseContext::Scope& scope);
    mozilla::Maybe<ModuleScope::Data*> newModuleScopeData(ParseContext::Scope& scope);
    mozilla::Maybe<EvalScope::Data*> newEvalScopeData(ParseContext::Scope& scope);
    mozilla::Maybe<FunctionScope::Data*> newFunctionScopeData(ParseContext::Scope& scope,
                                                              bool hasParameterExprs);
    mozilla::Maybe<VarScope::Data*> newVarScopeData(ParseContext::Scope& scope);
    mozilla::Maybe<LexicalScope::Data*> newLexicalScopeData(ParseContext::Scope& scope);

  protected:
    enum InvokedPrediction { PredictUninvoked = false, PredictInvoked = true };
    enum ForInitLocation { InForInit, NotInForInit };
};

inline
ParseContext::Scope::Scope(ParserBase* parser)
  : Nestable<Scope>(&parser->pc->innermostScope_),
    declared_(parser->context->frontendCollectionPool()),
    possibleAnnexBFunctionBoxes_(parser->context->frontendCollectionPool()),
    id_(parser->usedNames.nextScopeId())
{ }

inline
ParseContext::Scope::Scope(JSContext* cx, ParseContext* pc, UsedNameTracker& usedNames)
  : Nestable<Scope>(&pc->innermostScope_),
    declared_(cx->frontendCollectionPool()),
    possibleAnnexBFunctionBoxes_(cx->frontendCollectionPool()),
    id_(usedNames.nextScopeId())
{ }

inline
ParseContext::VarScope::VarScope(ParserBase* parser)
  : Scope(parser)
{
    useAsVarScope(parser->pc);
}

template <class ParseHandler, typename CharT>
class Parser final : public ParserBase, private JS::AutoGCRooter
{
  private:
    using Node = typename ParseHandler::Node;

    /*
     * A class for temporarily stashing errors while parsing continues.
     *
     * The ability to stash an error is useful for handling situations where we
     * aren't able to verify that an error has occurred until later in the parse.
     * For instance | ({x=1}) | is always parsed as an object literal with
     * a SyntaxError, however, in the case where it is followed by '=>' we rewind
     * and reparse it as a valid arrow function. Here a PossibleError would be
     * set to 'pending' when the initial SyntaxError was encountered then 'resolved'
     * just before rewinding the parser.
     *
     * There are currently two kinds of PossibleErrors: Expression and
     * Destructuring errors. Expression errors are used to mark a possible
     * syntax error when a grammar production is used in an expression context.
     * For example in |{x = 1}|, we mark the CoverInitializedName |x = 1| as a
     * possible expression error, because CoverInitializedName productions
     * are disallowed when an actual ObjectLiteral is expected.
     * Destructuring errors are used to record possible syntax errors in
     * destructuring contexts. For example in |[...rest, ] = []|, we initially
     * mark the trailing comma after the spread expression as a possible
     * destructuring error, because the ArrayAssignmentPattern grammar
     * production doesn't allow a trailing comma after the rest element.
     *
     * When using PossibleError one should set a pending error at the location
     * where an error occurs. From that point, the error may be resolved
     * (invalidated) or left until the PossibleError is checked.
     *
     * Ex:
     *   PossibleError possibleError(*this);
     *   possibleError.setPendingExpressionErrorAt(pos, JSMSG_BAD_PROP_ID);
     *   // A JSMSG_BAD_PROP_ID ParseError is reported, returns false.
     *   if (!possibleError.checkForExpressionError())
     *       return false; // we reach this point with a pending exception
     *
     *   PossibleError possibleError(*this);
     *   possibleError.setPendingExpressionErrorAt(pos, JSMSG_BAD_PROP_ID);
     *   // Returns true, no error is reported.
     *   if (!possibleError.checkForDestructuringError())
     *       return false; // not reached, no pending exception
     *
     *   PossibleError possibleError(*this);
     *   // Returns true, no error is reported.
     *   if (!possibleError.checkForExpressionError())
     *       return false; // not reached, no pending exception
     */
    class MOZ_STACK_CLASS PossibleError
    {
      private:
        enum class ErrorKind { Expression, Destructuring, DestructuringWarning };

        enum class ErrorState { None, Pending };

        struct Error {
            ErrorState state_ = ErrorState::None;

            // Error reporting fields.
            uint32_t offset_;
            unsigned errorNumber_;
        };

        Parser<ParseHandler, CharT>& parser_;
        Error exprError_;
        Error destructuringError_;
        Error destructuringWarning_;

        // Returns the error report.
        Error& error(ErrorKind kind);

        // Return true if an error is pending without reporting.
        bool hasError(ErrorKind kind);

        // Resolve any pending error.
        void setResolved(ErrorKind kind);

        // Set a pending error. Only a single error may be set per instance and
        // error kind.
        void setPending(ErrorKind kind, const TokenPos& pos, unsigned errorNumber);

        // If there is a pending error, report it and return false, otherwise
        // return true.
        MOZ_MUST_USE bool checkForError(ErrorKind kind);

        // If there is a pending warning, report it and return either false or
        // true depending on the werror option, otherwise return true.
        MOZ_MUST_USE bool checkForWarning(ErrorKind kind);

        // Transfer an existing error to another instance.
        void transferErrorTo(ErrorKind kind, PossibleError* other);

      public:
        explicit PossibleError(Parser<ParseHandler, CharT>& parser);

        // Return true if a pending destructuring error is present.
        bool hasPendingDestructuringError();

        // Set a pending destructuring error. Only a single error may be set
        // per instance, i.e. subsequent calls to this method are ignored and
        // won't overwrite the existing pending error.
        void setPendingDestructuringErrorAt(const TokenPos& pos, unsigned errorNumber);

        // Set a pending destructuring warning. Only a single warning may be
        // set per instance, i.e. subsequent calls to this method are ignored
        // and won't overwrite the existing pending warning.
        void setPendingDestructuringWarningAt(const TokenPos& pos, unsigned errorNumber);

        // Set a pending expression error. Only a single error may be set per
        // instance, i.e. subsequent calls to this method are ignored and won't
        // overwrite the existing pending error.
        void setPendingExpressionErrorAt(const TokenPos& pos, unsigned errorNumber);

        // If there is a pending destructuring error or warning, report it and
        // return false, otherwise return true. Clears any pending expression
        // error.
        MOZ_MUST_USE bool checkForDestructuringErrorOrWarning();

        // If there is a pending expression error, report it and return false,
        // otherwise return true. Clears any pending destructuring error or
        // warning.
        MOZ_MUST_USE bool checkForExpressionError();

        // Pass pending errors between possible error instances. This is useful
        // for extending the lifetime of a pending error beyond the scope of
        // the PossibleError where it was initially set (keeping in mind that
        // PossibleError is a MOZ_STACK_CLASS).
        void transferErrorsTo(PossibleError* other);
    };

    // When ParseHandler is FullParseHandler:
    //
    //   If non-null, this field holds the syntax parser used to attempt lazy
    //   parsing of inner functions. If null, then lazy parsing is disabled.
    //
    // When ParseHandler is SyntaxParseHandler:
    //
    //   If non-null, this field must be a sentinel value signaling that the
    //   syntax parse was aborted. If null, then lazy parsing was aborted due
    //   to encountering unsupported language constructs.
    using SyntaxParser = Parser<SyntaxParseHandler, CharT>;
    SyntaxParser* syntaxParser_;

  public:
    /* State specific to the kind of parse being performed. */
    ParseHandler handler;

    void prepareNodeForMutation(Node node) { handler.prepareNodeForMutation(node); }
    void freeTree(Node node) { handler.freeTree(node); }

  public:
    Parser(JSContext* cx, LifoAlloc& alloc, const ReadOnlyCompileOptions& options,
           const CharT* chars, size_t length, bool foldConstants, UsedNameTracker& usedNames,
           SyntaxParser* syntaxParser, LazyScript* lazyOuterFunction);
    ~Parser();

    friend class AutoAwaitIsKeyword<Parser>;
    void setAwaitHandling(AwaitHandling awaitHandling);

    // If ParseHandler is SyntaxParseHandler, whether the last syntax parse was
    // aborted due to unsupported language constructs.
    //
    // If ParseHandler is FullParseHandler, false.
    bool hadAbortedSyntaxParse();

    // If ParseHandler is SyntaxParseHandler, clear whether the last syntax
    // parse was aborted.
    //
    // If ParseHandler is FullParseHandler, do nothing.
    void clearAbortedSyntaxParse();

    bool checkOptions();

    friend void js::frontend::TraceParser(JSTracer* trc, JS::AutoGCRooter* parser);

    /*
     * Parse a top-level JS script.
     */
    Node parse();

    FunctionBox* newFunctionBox(Node fn, JSFunction* fun, uint32_t toStringStart,
                                Directives directives,
                                GeneratorKind generatorKind, FunctionAsyncKind asyncKind);

    void trace(JSTracer* trc);

  private:
    Parser* thisForCtor() { return this; }

    Node stringLiteral();
    Node noSubstitutionTaggedTemplate();
    Node noSubstitutionUntaggedTemplate();
    Node templateLiteral(YieldHandling yieldHandling);
    bool taggedTemplate(YieldHandling yieldHandling, Node nodeList, TokenKind tt);
    bool appendToCallSiteObj(Node callSiteObj);
    bool addExprAndGetNextTemplStrToken(YieldHandling yieldHandling, Node nodeList,
                                        TokenKind* ttp);
    bool checkStatementsEOF();

    inline Node newName(PropertyName* name);
    inline Node newName(PropertyName* name, TokenPos pos);

    // If ParseHandler is SyntaxParseHandler, flag the current syntax parse as
    // aborted due to unsupported language constructs and return
    // false. Aborting the current syntax parse does not disable attempts to
    // syntax parse future inner functions.
    //
    // If ParseHandler is FullParseHandler, disable syntax parsing of all
    // future inner functions and return true.
    inline bool abortIfSyntaxParser();

    // If ParseHandler is SyntaxParseHandler, do nothing.
    //
    // If ParseHandler is FullParseHandler, disable syntax parsing of all
    // future inner functions.
    void disableSyntaxParser();

  public:
    /* Public entry points for parsing. */
    Node statementListItem(YieldHandling yieldHandling, bool canHaveDirectives = false);

    // Parse the body of an eval.
    //
    // Eval scripts are distinguished from global scripts in that in ES6, per
    // 18.2.1.1 steps 9 and 10, all eval scripts are executed under a fresh
    // lexical scope.
    Node evalBody(EvalSharedContext* evalsc);

    // Parse the body of a global script.
    Node globalBody(GlobalSharedContext* globalsc);

    // Parse a module.
    Node moduleBody(ModuleSharedContext* modulesc);

    // Parse a function, used for the Function, GeneratorFunction, and
    // AsyncFunction constructors.
    Node standaloneFunction(HandleFunction fun, HandleScope enclosingScope,
                            const mozilla::Maybe<uint32_t>& parameterListEnd,
                            GeneratorKind generatorKind, FunctionAsyncKind asyncKind,
                            Directives inheritedDirectives, Directives* newDirectives);

    // Parse a function, given only its arguments and body. Used for lazily
    // parsed functions.
    Node standaloneLazyFunction(HandleFunction fun, uint32_t toStringStart, bool strict,
                                GeneratorKind generatorKind, FunctionAsyncKind asyncKind);

    // Parse an inner function given an enclosing ParseContext and a
    // FunctionBox for the inner function.
    bool innerFunction(Node pn, ParseContext* outerpc, FunctionBox* funbox, uint32_t toStringStart,
                       InHandling inHandling, YieldHandling yieldHandling,
                       FunctionSyntaxKind kind,
                       Directives inheritedDirectives, Directives* newDirectives);

    // Parse a function's formal parameters and its body assuming its function
    // ParseContext is already on the stack.
    bool functionFormalParametersAndBody(InHandling inHandling, YieldHandling yieldHandling,
                                         Node pn, FunctionSyntaxKind kind,
                                         const mozilla::Maybe<uint32_t>& parameterListEnd = mozilla::Nothing(),
                                         bool isStandaloneFunction = false);

    // Match the current token against the BindingIdentifier production with
    // the given Yield parameter.  If there is no match, report a syntax
    // error.
    PropertyName* bindingIdentifier(YieldHandling yieldHandling);

  private:
    /*
     * JS parsers, from lowest to highest precedence.
     *
     * Each parser must be called during the dynamic scope of a ParseContext
     * object, pointed to by this->pc.
     *
     * Each returns a parse node tree or null on error.
     *
     * Parsers whose name has a '1' suffix leave the TokenStream state
     * pointing to the token one past the end of the parsed fragment.  For a
     * number of the parsers this is convenient and avoids a lot of
     * unnecessary ungetting and regetting of tokens.
     *
     * Some parsers have two versions:  an always-inlined version (with an 'i'
     * suffix) and a never-inlined version (with an 'n' suffix).
     */
    Node functionStmt(uint32_t toStringStart,
                      YieldHandling yieldHandling, DefaultHandling defaultHandling,
                      FunctionAsyncKind asyncKind = SyncFunction);
    Node functionExpr(uint32_t toStringStart, InvokedPrediction invoked = PredictUninvoked,
                      FunctionAsyncKind asyncKind = SyncFunction);

    Node statementList(YieldHandling yieldHandling);
    Node statement(YieldHandling yieldHandling);
    bool maybeParseDirective(Node list, Node pn, bool* cont);

    Node blockStatement(YieldHandling yieldHandling,
                        unsigned errorNumber = JSMSG_CURLY_IN_COMPOUND);
    Node doWhileStatement(YieldHandling yieldHandling);
    Node whileStatement(YieldHandling yieldHandling);

    Node forStatement(YieldHandling yieldHandling);
    bool forHeadStart(YieldHandling yieldHandling,
                      IteratorKind iterKind,
                      ParseNodeKind* forHeadKind,
                      Node* forInitialPart,
                      mozilla::Maybe<ParseContext::Scope>& forLetImpliedScope,
                      Node* forInOrOfExpression);
    Node expressionAfterForInOrOf(ParseNodeKind forHeadKind, YieldHandling yieldHandling);

    Node switchStatement(YieldHandling yieldHandling);
    Node continueStatement(YieldHandling yieldHandling);
    Node breakStatement(YieldHandling yieldHandling);
    Node returnStatement(YieldHandling yieldHandling);
    Node withStatement(YieldHandling yieldHandling);
    Node throwStatement(YieldHandling yieldHandling);
    Node tryStatement(YieldHandling yieldHandling);
    Node catchBlockStatement(YieldHandling yieldHandling, ParseContext::Scope& catchParamScope);
    Node debuggerStatement();

    Node variableStatement(YieldHandling yieldHandling);

    Node labeledStatement(YieldHandling yieldHandling);
    Node labeledItem(YieldHandling yieldHandling);

    Node ifStatement(YieldHandling yieldHandling);
    Node consequentOrAlternative(YieldHandling yieldHandling);

    // While on a |let| TOK_NAME token, examine |next|.  Indicate whether
    // |next|, the next token already gotten with modifier TokenStream::None,
    // continues a LexicalDeclaration.
    bool nextTokenContinuesLetDeclaration(TokenKind next);

    Node lexicalDeclaration(YieldHandling yieldHandling, DeclarationKind kind);

    Node importDeclaration();

    bool processExport(Node node);
    bool processExportFrom(Node node);

    Node exportFrom(uint32_t begin, Node specList);
    Node exportBatch(uint32_t begin);
    bool checkLocalExportNames(Node node);
    Node exportClause(uint32_t begin);
    Node exportFunctionDeclaration(uint32_t begin, uint32_t toStringStart,
                                   FunctionAsyncKind asyncKind = SyncFunction);
    Node exportVariableStatement(uint32_t begin);
    Node exportClassDeclaration(uint32_t begin);
    Node exportLexicalDeclaration(uint32_t begin, DeclarationKind kind);
    Node exportDefaultFunctionDeclaration(uint32_t begin, uint32_t toStringStart,
                                          FunctionAsyncKind asyncKind = SyncFunction);
    Node exportDefaultClassDeclaration(uint32_t begin);
    Node exportDefaultAssignExpr(uint32_t begin);
    Node exportDefault(uint32_t begin);
    Node exportDeclaration();

    Node expressionStatement(YieldHandling yieldHandling,
                             InvokedPrediction invoked = PredictUninvoked);

    // Declaration parsing.  The main entrypoint is Parser::declarationList,
    // with sub-functionality split out into the remaining methods.

    // |blockScope| may be non-null only when |kind| corresponds to a lexical
    // declaration (that is, PNK_LET or PNK_CONST).
    //
    // The for* parameters, for normal declarations, should be null/ignored.
    // They should be non-null only when Parser::forHeadStart parses a
    // declaration at the start of a for-loop head.
    //
    // In this case, on success |*forHeadKind| is PNK_FORHEAD, PNK_FORIN, or
    // PNK_FOROF, corresponding to the three for-loop kinds.  The precise value
    // indicates what was parsed.
    //
    // If parsing recognized a for(;;) loop, the next token is the ';' within
    // the loop-head that separates the init/test parts.
    //
    // Otherwise, for for-in/of loops, the next token is the ')' ending the
    // loop-head.  Additionally, the expression that the loop iterates over was
    // parsed into |*forInOrOfExpression|.
    Node declarationList(YieldHandling yieldHandling,
                         ParseNodeKind kind,
                         ParseNodeKind* forHeadKind = nullptr,
                         Node* forInOrOfExpression = nullptr);

    // The items in a declaration list are either patterns or names, with or
    // without initializers.  These two methods parse a single pattern/name and
    // any associated initializer -- and if parsing an |initialDeclaration|
    // will, if parsing in a for-loop head (as specified by |forHeadKind| being
    // non-null), consume additional tokens up to the closing ')' in a
    // for-in/of loop head, returning the iterated expression in
    // |*forInOrOfExpression|.  (An "initial declaration" is the first
    // declaration in a declaration list: |a| but not |b| in |var a, b|, |{c}|
    // but not |d| in |let {c} = 3, d|.)
    Node declarationPattern(Node decl, DeclarationKind declKind, TokenKind tt,
                            bool initialDeclaration, YieldHandling yieldHandling,
                            ParseNodeKind* forHeadKind, Node* forInOrOfExpression);
    Node declarationName(Node decl, DeclarationKind declKind, TokenKind tt,
                         bool initialDeclaration, YieldHandling yieldHandling,
                         ParseNodeKind* forHeadKind, Node* forInOrOfExpression);

    // Having parsed a name (not found in a destructuring pattern) declared by
    // a declaration, with the current token being the '=' separating the name
    // from its initializer, parse and bind that initializer -- and possibly
    // consume trailing in/of and subsequent expression, if so directed by
    // |forHeadKind|.
    bool initializerInNameDeclaration(Node decl, Node binding, Handle<PropertyName*> name,
                                      DeclarationKind declKind, bool initialDeclaration,
                                      YieldHandling yieldHandling, ParseNodeKind* forHeadKind,
                                      Node* forInOrOfExpression);

    Node expr(InHandling inHandling, YieldHandling yieldHandling,
              TripledotHandling tripledotHandling, PossibleError* possibleError = nullptr,
              InvokedPrediction invoked = PredictUninvoked);
    Node assignExpr(InHandling inHandling, YieldHandling yieldHandling,
                    TripledotHandling tripledotHandling, PossibleError* possibleError = nullptr,
                    InvokedPrediction invoked = PredictUninvoked);
    Node assignExprWithoutYieldOrAwait(YieldHandling yieldHandling);
    Node yieldExpression(InHandling inHandling);
    Node condExpr1(InHandling inHandling, YieldHandling yieldHandling,
                   TripledotHandling tripledotHandling,
                   PossibleError* possibleError,
                   InvokedPrediction invoked = PredictUninvoked);
    Node orExpr1(InHandling inHandling, YieldHandling yieldHandling,
                 TripledotHandling tripledotHandling,
                 PossibleError* possibleError,
                 InvokedPrediction invoked = PredictUninvoked);
    Node unaryExpr(YieldHandling yieldHandling, TripledotHandling tripledotHandling,
                   PossibleError* possibleError = nullptr,
                   InvokedPrediction invoked = PredictUninvoked);
    Node memberExpr(YieldHandling yieldHandling, TripledotHandling tripledotHandling,
                    TokenKind tt, bool allowCallSyntax = true,
                    PossibleError* possibleError = nullptr,
                    InvokedPrediction invoked = PredictUninvoked);
    Node primaryExpr(YieldHandling yieldHandling, TripledotHandling tripledotHandling,
                     TokenKind tt, PossibleError* possibleError,
                     InvokedPrediction invoked = PredictUninvoked);
    Node exprInParens(InHandling inHandling, YieldHandling yieldHandling,
                      TripledotHandling tripledotHandling, PossibleError* possibleError = nullptr);

    bool tryNewTarget(Node& newTarget);
    bool checkAndMarkSuperScope();

    Node methodDefinition(uint32_t toStringStart, PropertyType propType, HandleAtom funName);

    /*
     * Additional JS parsers.
     */
    bool functionArguments(YieldHandling yieldHandling, FunctionSyntaxKind kind,
                           Node funcpn);

    Node functionDefinition(Node func, uint32_t toStringStart,
                            InHandling inHandling, YieldHandling yieldHandling,
                            HandleAtom name, FunctionSyntaxKind kind,
                            GeneratorKind generatorKind, FunctionAsyncKind asyncKind,
                            bool tryAnnexB = false);

    // Parse a function body.  Pass StatementListBody if the body is a list of
    // statements; pass ExpressionBody if the body is a single expression.
    enum FunctionBodyType { StatementListBody, ExpressionBody };
    Node functionBody(InHandling inHandling, YieldHandling yieldHandling, FunctionSyntaxKind kind,
                      FunctionBodyType type);

    Node unaryOpExpr(YieldHandling yieldHandling, ParseNodeKind kind, JSOp op, uint32_t begin);

    Node condition(InHandling inHandling, YieldHandling yieldHandling);

    /* comprehensions */
    Node generatorComprehensionLambda(unsigned begin);
    Node comprehensionFor(GeneratorKind comprehensionKind);
    Node comprehensionIf(GeneratorKind comprehensionKind);
    Node comprehensionTail(GeneratorKind comprehensionKind);
    Node comprehension(GeneratorKind comprehensionKind);
    Node arrayComprehension(uint32_t begin);
    Node generatorComprehension(uint32_t begin);

    bool argumentList(YieldHandling yieldHandling, Node listNode, bool* isSpread,
                      PossibleError* possibleError = nullptr);
    Node destructuringDeclaration(DeclarationKind kind, YieldHandling yieldHandling,
                                  TokenKind tt);
    Node destructuringDeclarationWithoutYieldOrAwait(DeclarationKind kind, YieldHandling yieldHandling,
                                                     TokenKind tt);

    bool namedImportsOrNamespaceImport(TokenKind tt, Node importSpecSet);
    bool checkExportedName(JSAtom* exportName);
    bool checkExportedNamesForDeclaration(Node node);
    bool checkExportedNameForClause(Node node);
    bool checkExportedNameForFunction(Node node);
    bool checkExportedNameForClass(Node node);

    enum ClassContext { ClassStatement, ClassExpression };
    Node classDefinition(YieldHandling yieldHandling, ClassContext classContext,
                         DefaultHandling defaultHandling);

    bool checkLabelOrIdentifierReference(PropertyName* ident,
                                         uint32_t offset,
                                         YieldHandling yieldHandling,
                                         TokenKind hint = TOK_LIMIT);

    bool checkLocalExportName(PropertyName* ident, uint32_t offset) {
        return checkLabelOrIdentifierReference(ident, offset, YieldIsName);
    }

    bool checkBindingIdentifier(PropertyName* ident,
                                uint32_t offset,
                                YieldHandling yieldHandling,
                                TokenKind hint = TOK_LIMIT);

    PropertyName* labelOrIdentifierReference(YieldHandling yieldHandling);

    PropertyName* labelIdentifier(YieldHandling yieldHandling) {
        return labelOrIdentifierReference(yieldHandling);
    }

    PropertyName* identifierReference(YieldHandling yieldHandling) {
        return labelOrIdentifierReference(yieldHandling);
    }

    PropertyName* importedBinding() {
        return bindingIdentifier(YieldIsName);
    }

    Node identifierReference(Handle<PropertyName*> name);

    bool matchLabel(YieldHandling yieldHandling, MutableHandle<PropertyName*> label);

    bool matchInOrOf(bool* isForInp, bool* isForOfp);

    bool hasUsedFunctionSpecialName(HandlePropertyName name);
    bool declareFunctionArgumentsObject();
    bool declareFunctionThis();
    Node newInternalDotName(HandlePropertyName name);
    Node newThisName();
    Node newDotGeneratorName();
    bool declareDotGeneratorName();

    bool skipLazyInnerFunction(Node pn, uint32_t toStringStart, FunctionSyntaxKind kind,
                               bool tryAnnexB);
    bool innerFunction(Node pn, ParseContext* outerpc, HandleFunction fun, uint32_t toStringStart,
                       InHandling inHandling, YieldHandling yieldHandling,
                       FunctionSyntaxKind kind,
                       GeneratorKind generatorKind, FunctionAsyncKind asyncKind, bool tryAnnexB,
                       Directives inheritedDirectives, Directives* newDirectives);
    bool trySyntaxParseInnerFunction(Node pn, HandleFunction fun, uint32_t toStringStart,
                                     InHandling inHandling, YieldHandling yieldHandling,
                                     FunctionSyntaxKind kind,
                                     GeneratorKind generatorKind, FunctionAsyncKind asyncKind,
                                     bool tryAnnexB,
                                     Directives inheritedDirectives, Directives* newDirectives);
    bool finishFunctionScopes(bool isStandaloneFunction);
    bool finishFunction(bool isStandaloneFunction = false);
    bool leaveInnerFunction(ParseContext* outerpc);

    bool matchOrInsertSemicolonHelper(TokenStream::Modifier modifier);
    bool matchOrInsertSemicolonAfterExpression();
    bool matchOrInsertSemicolonAfterNonExpression();

  public:
    enum FunctionCallBehavior {
        PermitAssignmentToFunctionCalls,
        ForbidAssignmentToFunctionCalls
    };

    bool isValidSimpleAssignmentTarget(Node node,
                                       FunctionCallBehavior behavior = ForbidAssignmentToFunctionCalls);

  private:
    bool checkIncDecOperand(Node operand, uint32_t operandOffset);
    bool checkStrictAssignment(Node lhs);

    void reportMissingClosing(unsigned errorNumber, unsigned noteNumber, uint32_t openedPos);

    void reportRedeclaration(HandlePropertyName name, DeclarationKind prevKind, TokenPos pos,
                             uint32_t prevPos);
    bool notePositionalFormalParameter(Node fn, HandlePropertyName name, uint32_t beginPos,
                                       bool disallowDuplicateParams, bool* duplicatedParam);
    bool noteDestructuredPositionalFormalParameter(Node fn, Node destruct);

    bool checkLexicalDeclarationDirectlyWithinBlock(ParseContext::Statement& stmt,
                                                    DeclarationKind kind, TokenPos pos);
    bool noteDeclaredName(HandlePropertyName name, DeclarationKind kind, TokenPos pos);
    bool noteUsedName(HandlePropertyName name);
    bool hasUsedName(HandlePropertyName name);

    // Required on Scope exit.
    bool propagateFreeNamesAndMarkClosedOverBindings(ParseContext::Scope& scope);

    Node finishLexicalScope(ParseContext::Scope& scope, Node body);

    Node propertyName(YieldHandling yieldHandling,
                      const mozilla::Maybe<DeclarationKind>& maybeDecl, Node propList,
                      PropertyType* propType, MutableHandleAtom propAtom);
    Node computedPropertyName(YieldHandling yieldHandling,
                              const mozilla::Maybe<DeclarationKind>& maybeDecl, Node literal);
    Node arrayInitializer(YieldHandling yieldHandling, PossibleError* possibleError);
    Node newRegExp();

    Node objectLiteral(YieldHandling yieldHandling, PossibleError* possibleError);

    Node bindingInitializer(Node lhs, DeclarationKind kind, YieldHandling yieldHandling);
    Node bindingIdentifier(DeclarationKind kind, YieldHandling yieldHandling);
    Node bindingIdentifierOrPattern(DeclarationKind kind, YieldHandling yieldHandling,
                                    TokenKind tt);
    Node objectBindingPattern(DeclarationKind kind, YieldHandling yieldHandling);
    Node arrayBindingPattern(DeclarationKind kind, YieldHandling yieldHandling);

    enum class TargetBehavior {
        PermitAssignmentPattern,
        ForbidAssignmentPattern
    };
    bool checkDestructuringAssignmentTarget(Node expr, TokenPos exprPos,
                                            PossibleError* exprPossibleError,
                                            PossibleError* possibleError,
                                            TargetBehavior behavior = TargetBehavior::PermitAssignmentPattern);
    void checkDestructuringAssignmentName(Node name, TokenPos namePos,
                                          PossibleError* possibleError);
    bool checkDestructuringAssignmentElement(Node expr, TokenPos exprPos,
                                             PossibleError* exprPossibleError,
                                             PossibleError* possibleError);

    Node newNumber(const Token& tok) {
        return handler.newNumber(tok.number(), tok.decimalPoint(), tok.pos);
    }

    static Node null() { return ParseHandler::null(); }

    JSAtom* prefixAccessorName(PropertyType propType, HandleAtom propAtom);

    bool asmJS(Node list);
};

template <class Parser>
class MOZ_STACK_CLASS AutoAwaitIsKeyword
{
  private:
    Parser* parser_;
    AwaitHandling oldAwaitHandling_;

  public:
    AutoAwaitIsKeyword(Parser* parser, AwaitHandling awaitHandling) {
        parser_ = parser;
        oldAwaitHandling_ = static_cast<AwaitHandling>(parser_->awaitHandling_);

        // 'await' is always a keyword in module contexts, so we don't modify
        // the state when the original handling is AwaitIsModuleKeyword.
        if (oldAwaitHandling_ != AwaitIsModuleKeyword)
            parser_->setAwaitHandling(awaitHandling);
    }

    ~AutoAwaitIsKeyword() {
        parser_->setAwaitHandling(oldAwaitHandling_);
    }
};

} /* namespace frontend */
} /* namespace js */

#endif /* frontend_Parser_h */
