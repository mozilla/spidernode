// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PARSING_PARSER_H_
#define V8_PARSING_PARSER_H_

#include "src/ast/ast.h"
#include "src/ast/scopes.h"
#include "src/parsing/parser-base.h"
#include "src/parsing/preparse-data.h"
#include "src/parsing/preparse-data-format.h"
#include "src/parsing/preparser.h"
#include "src/pending-compilation-error-handler.h"

namespace v8 {

class ScriptCompiler;

namespace internal {

class ParseInfo;
class ScriptData;
class Target;

class FunctionEntry BASE_EMBEDDED {
 public:
  enum {
    kStartPositionIndex,
    kEndPositionIndex,
    kLiteralCountIndex,
    kPropertyCountIndex,
    kLanguageModeIndex,
    kUsesSuperPropertyIndex,
    kCallsEvalIndex,
    kSize
  };

  explicit FunctionEntry(Vector<unsigned> backing)
    : backing_(backing) { }

  FunctionEntry() : backing_() { }

  int start_pos() { return backing_[kStartPositionIndex]; }
  int end_pos() { return backing_[kEndPositionIndex]; }
  int literal_count() { return backing_[kLiteralCountIndex]; }
  int property_count() { return backing_[kPropertyCountIndex]; }
  LanguageMode language_mode() {
    DCHECK(is_valid_language_mode(backing_[kLanguageModeIndex]));
    return static_cast<LanguageMode>(backing_[kLanguageModeIndex]);
  }
  bool uses_super_property() { return backing_[kUsesSuperPropertyIndex]; }
  bool calls_eval() { return backing_[kCallsEvalIndex]; }

  bool is_valid() { return !backing_.is_empty(); }

 private:
  Vector<unsigned> backing_;
};


// Wrapper around ScriptData to provide parser-specific functionality.
class ParseData {
 public:
  static ParseData* FromCachedData(ScriptData* cached_data) {
    ParseData* pd = new ParseData(cached_data);
    if (pd->IsSane()) return pd;
    cached_data->Reject();
    delete pd;
    return NULL;
  }

  void Initialize();
  FunctionEntry GetFunctionEntry(int start);
  int FunctionCount();

  bool HasError();

  unsigned* Data() {  // Writable data as unsigned int array.
    return reinterpret_cast<unsigned*>(const_cast<byte*>(script_data_->data()));
  }

  void Reject() { script_data_->Reject(); }

  bool rejected() const { return script_data_->rejected(); }

 private:
  explicit ParseData(ScriptData* script_data) : script_data_(script_data) {}

  bool IsSane();
  unsigned Magic();
  unsigned Version();
  int FunctionsSize();
  int Length() const {
    // Script data length is already checked to be a multiple of unsigned size.
    return script_data_->length() / sizeof(unsigned);
  }

  ScriptData* script_data_;
  int function_index_;

  DISALLOW_COPY_AND_ASSIGN(ParseData);
};

// ----------------------------------------------------------------------------
// JAVASCRIPT PARSING

class Parser;
class SingletonLogger;


struct ParserFormalParameters : FormalParametersBase {
  struct Parameter {
    Parameter(const AstRawString* name, Expression* pattern,
              Expression* initializer, int initializer_end_position,
              bool is_rest)
        : name(name),
          pattern(pattern),
          initializer(initializer),
          initializer_end_position(initializer_end_position),
          is_rest(is_rest) {}
    const AstRawString* name;
    Expression* pattern;
    Expression* initializer;
    int initializer_end_position;
    bool is_rest;
    bool is_simple() const {
      return pattern->IsVariableProxy() && initializer == nullptr && !is_rest;
    }
  };

  explicit ParserFormalParameters(DeclarationScope* scope)
      : FormalParametersBase(scope), params(4, scope->zone()) {}
  ZoneList<Parameter> params;

  int Arity() const { return params.length(); }
  const Parameter& at(int i) const { return params[i]; }
};

template <>
class ParserBaseTraits<Parser> {
 public:
  typedef ParserBaseTraits<Parser> ParserTraits;

  struct Type {
    typedef Variable GeneratorVariable;

    typedef v8::internal::AstProperties AstProperties;

    typedef v8::internal::ExpressionClassifier<ParserTraits>
        ExpressionClassifier;

    // Return types for traversing functions.
    typedef const AstRawString* Identifier;
    typedef v8::internal::Expression* Expression;
    typedef Yield* YieldExpression;
    typedef v8::internal::FunctionLiteral* FunctionLiteral;
    typedef v8::internal::ClassLiteral* ClassLiteral;
    typedef v8::internal::Literal* Literal;
    typedef ObjectLiteral::Property* ObjectLiteralProperty;
    typedef ZoneList<v8::internal::Expression*>* ExpressionList;
    typedef ZoneList<ObjectLiteral::Property*>* PropertyList;
    typedef ParserFormalParameters::Parameter FormalParameter;
    typedef ParserFormalParameters FormalParameters;
    typedef ZoneList<v8::internal::Statement*>* StatementList;

    // For constructing objects returned by the traversing functions.
    typedef AstNodeFactory Factory;
  };

  // TODO(nikolaos): The traits methods should not need to call methods
  // of the implementation object.
  Parser* delegate() { return reinterpret_cast<Parser*>(this); }
  const Parser* delegate() const {
    return reinterpret_cast<const Parser*>(this);
  }

  // Helper functions for recursive descent.
  bool IsEval(const AstRawString* identifier) const;
  bool IsArguments(const AstRawString* identifier) const;
  bool IsEvalOrArguments(const AstRawString* identifier) const;
  bool IsUndefined(const AstRawString* identifier) const;
  V8_INLINE bool IsFutureStrictReserved(const AstRawString* identifier) const;

  // Returns true if the expression is of type "this.foo".
  static bool IsThisProperty(Expression* expression);

  static bool IsIdentifier(Expression* expression);

  static const AstRawString* AsIdentifier(Expression* expression) {
    DCHECK(IsIdentifier(expression));
    return expression->AsVariableProxy()->raw_name();
  }

  bool IsPrototype(const AstRawString* identifier) const;

  bool IsConstructor(const AstRawString* identifier) const;

  bool IsDirectEvalCall(Expression* expression) const {
    if (!expression->IsCall()) return false;
    expression = expression->AsCall()->expression();
    return IsIdentifier(expression) && IsEval(AsIdentifier(expression));
  }

  static bool IsBoilerplateProperty(ObjectLiteral::Property* property) {
    return ObjectLiteral::IsBoilerplateProperty(property);
  }

  static bool IsArrayIndex(const AstRawString* string, uint32_t* index) {
    return string->AsArrayIndex(index);
  }

  static Expression* GetPropertyValue(ObjectLiteral::Property* property) {
    return property->value();
  }

  // Functions for encapsulating the differences between parsing and preparsing;
  // operations interleaved with the recursive descent.
  static void PushLiteralName(FuncNameInferrer* fni, const AstRawString* id) {
    fni->PushLiteralName(id);
  }

  void PushPropertyName(FuncNameInferrer* fni, Expression* expression);

  static void InferFunctionName(FuncNameInferrer* fni,
                                FunctionLiteral* func_to_infer) {
    fni->AddFunction(func_to_infer);
  }

  // If we assign a function literal to a property we pretenure the
  // literal so it can be added as a constant function property.
  static void CheckAssigningFunctionLiteralToProperty(Expression* left,
                                                      Expression* right);

  // Determine if the expression is a variable proxy and mark it as being used
  // in an assignment or with a increment/decrement operator.
  static Expression* MarkExpressionAsAssigned(Expression* expression);

  // Returns true if we have a binary expression between two numeric
  // literals. In that case, *x will be changed to an expression which is the
  // computed value.
  bool ShortcutNumericLiteralBinaryExpression(Expression** x, Expression* y,
                                              Token::Value op, int pos,
                                              AstNodeFactory* factory);

  // Rewrites the following types of unary expressions:
  // not <literal> -> true / false
  // + <numeric literal> -> <numeric literal>
  // - <numeric literal> -> <numeric literal with value negated>
  // ! <literal> -> true / false
  // The following rewriting rules enable the collection of type feedback
  // without any special stub and the multiplication is removed later in
  // Crankshaft's canonicalization pass.
  // + foo -> foo * 1
  // - foo -> foo * (-1)
  // ~ foo -> foo ^(~0)
  Expression* BuildUnaryExpression(Expression* expression, Token::Value op,
                                   int pos, AstNodeFactory* factory);

  Expression* BuildIteratorResult(Expression* value, bool done);

  // Generate AST node that throws a ReferenceError with the given type.
  Expression* NewThrowReferenceError(MessageTemplate::Template message,
                                     int pos);

  // Generate AST node that throws a SyntaxError with the given
  // type. The first argument may be null (in the handle sense) in
  // which case no arguments are passed to the constructor.
  Expression* NewThrowSyntaxError(MessageTemplate::Template message,
                                  const AstRawString* arg, int pos);

  // Generate AST node that throws a TypeError with the given
  // type. Both arguments must be non-null (in the handle sense).
  Expression* NewThrowTypeError(MessageTemplate::Template message,
                                const AstRawString* arg, int pos);

  // Reporting errors.
  void ReportMessageAt(Scanner::Location source_location,
                       MessageTemplate::Template message,
                       const char* arg = NULL,
                       ParseErrorType error_type = kSyntaxError);
  void ReportMessageAt(Scanner::Location source_location,
                       MessageTemplate::Template message,
                       const AstRawString* arg,
                       ParseErrorType error_type = kSyntaxError);

  // "null" return type creators.
  static const AstRawString* EmptyIdentifier() { return nullptr; }
  static Expression* EmptyExpression() { return nullptr; }
  static Literal* EmptyLiteral() { return nullptr; }
  static ObjectLiteralProperty* EmptyObjectLiteralProperty() { return nullptr; }
  static FunctionLiteral* EmptyFunctionLiteral() { return nullptr; }

  // Used in error return values.
  static ZoneList<Expression*>* NullExpressionList() { return nullptr; }

  // Non-NULL empty string.
  V8_INLINE const AstRawString* EmptyIdentifierString() const;

  // Odd-ball literal creators.
  Literal* GetLiteralTheHole(int position, AstNodeFactory* factory) const;

  // Producing data during the recursive descent.
  const AstRawString* GetSymbol(Scanner* scanner) const;
  const AstRawString* GetNextSymbol(Scanner* scanner) const;
  const AstRawString* GetNumberAsSymbol(Scanner* scanner) const;

  Expression* ThisExpression(int pos = kNoSourcePosition);
  Expression* NewSuperPropertyReference(AstNodeFactory* factory, int pos);
  Expression* NewSuperCallReference(AstNodeFactory* factory, int pos);
  Expression* NewTargetExpression(int pos);
  Expression* FunctionSentExpression(AstNodeFactory* factory, int pos) const;
  Literal* ExpressionFromLiteral(Token::Value token, int pos, Scanner* scanner,
                                 AstNodeFactory* factory) const;
  Expression* ExpressionFromIdentifier(const AstRawString* name,
                                       int start_position, int end_position,
                                       InferName = InferName::kYes);
  Expression* ExpressionFromString(int pos, Scanner* scanner,
                                   AstNodeFactory* factory) const;
  Expression* GetIterator(Expression* iterable, AstNodeFactory* factory,
                          int pos);
  ZoneList<v8::internal::Expression*>* NewExpressionList(int size,
                                                         Zone* zone) const {
    return new(zone) ZoneList<v8::internal::Expression*>(size, zone);
  }
  ZoneList<ObjectLiteral::Property*>* NewPropertyList(int size,
                                                      Zone* zone) const {
    return new(zone) ZoneList<ObjectLiteral::Property*>(size, zone);
  }
  ZoneList<v8::internal::Statement*>* NewStatementList(int size,
                                                       Zone* zone) const {
    return new(zone) ZoneList<v8::internal::Statement*>(size, zone);
  }

  V8_INLINE void AddParameterInitializationBlock(
      const ParserFormalParameters& parameters,
      ZoneList<v8::internal::Statement*>* body, bool is_async, bool* ok);

  V8_INLINE void AddFormalParameter(ParserFormalParameters* parameters,
                                    Expression* pattern,
                                    Expression* initializer,
                                    int initializer_end_position, bool is_rest);
  V8_INLINE void DeclareFormalParameter(
      DeclarationScope* scope,
      const ParserFormalParameters::Parameter& parameter,
      Type::ExpressionClassifier* classifier);
  void ParseArrowFunctionFormalParameterList(
      ParserFormalParameters* parameters, Expression* params,
      const Scanner::Location& params_loc, Scanner::Location* duplicate_loc,
      const Scope::Snapshot& scope_snapshot, bool* ok);

  void ReindexLiterals(const ParserFormalParameters& parameters);

  V8_INLINE Expression* NoTemplateTag() { return NULL; }
  V8_INLINE static bool IsTaggedTemplate(const Expression* tag) {
    return tag != NULL;
  }

  V8_INLINE void MaterializeUnspreadArgumentsLiterals(int count) {}

  Expression* ExpressionListToExpression(ZoneList<Expression*>* args);

  void SetFunctionNameFromPropertyName(ObjectLiteralProperty* property,
                                       const AstRawString* name);

  void SetFunctionNameFromIdentifierRef(Expression* value,
                                        Expression* identifier);

  V8_INLINE ZoneList<typename Type::ExpressionClassifier::Error>*
      GetReportedErrorList() const;
  V8_INLINE Zone* zone() const;

  V8_INLINE ZoneList<Expression*>* GetNonPatternList() const;
};

class Parser : public ParserBase<Parser> {
 public:
  explicit Parser(ParseInfo* info);
  ~Parser() {
    delete reusable_preparser_;
    reusable_preparser_ = NULL;
    delete cached_parse_data_;
    cached_parse_data_ = NULL;
  }

  // Parses the source code represented by the compilation info and sets its
  // function literal.  Returns false (and deallocates any allocated AST
  // nodes) if parsing failed.
  static bool ParseStatic(ParseInfo* info);
  bool Parse(ParseInfo* info);
  void ParseOnBackground(ParseInfo* info);

  void DeserializeScopeChain(ParseInfo* info, Handle<Context> context,
                             Scope::DeserializationMode deserialization_mode);

  // Handle errors detected during parsing, move statistics to Isolate,
  // internalize strings (move them to the heap).
  void Internalize(Isolate* isolate, Handle<Script> script, bool error);
  void HandleSourceURLComments(Isolate* isolate, Handle<Script> script);

 private:
  friend class ParserBase<Parser>;
  // TODO(nikolaos): This should not be necessary. It will be removed
  // when the traits object stops delegating to the implementation object.
  friend class ParserBaseTraits<Parser>;

  // Runtime encoding of different completion modes.
  enum CompletionKind {
    kNormalCompletion,
    kThrowCompletion,
    kAbruptCompletion
  };

  enum class FunctionBodyType { kNormal, kSingleExpression };

  DeclarationScope* GetDeclarationScope() const {
    return scope()->GetDeclarationScope();
  }
  DeclarationScope* GetClosureScope() const {
    return scope()->GetClosureScope();
  }
  Variable* NewTemporary(const AstRawString* name) {
    return scope()->NewTemporary(name);
  }

  // Limit the allowed number of local variables in a function. The hard limit
  // is that offsets computed by FullCodeGenerator::StackOperand and similar
  // functions are ints, and they should not overflow. In addition, accessing
  // local variables creates user-controlled constants in the generated code,
  // and we don't want too much user-controlled memory inside the code (this was
  // the reason why this limit was introduced in the first place; see
  // https://codereview.chromium.org/7003030/ ).
  static const int kMaxNumFunctionLocals = 4194303;  // 2^22-1

  // Returns NULL if parsing failed.
  FunctionLiteral* ParseProgram(Isolate* isolate, ParseInfo* info);

  FunctionLiteral* ParseLazy(Isolate* isolate, ParseInfo* info);
  FunctionLiteral* DoParseLazy(ParseInfo* info, const AstRawString* raw_name,
                               Utf16CharacterStream* source);

  // Called by ParseProgram after setting up the scanner.
  FunctionLiteral* DoParseProgram(ParseInfo* info);

  void SetCachedData(ParseInfo* info);

  ScriptCompiler::CompileOptions compile_options() const {
    return compile_options_;
  }
  bool consume_cached_parse_data() const {
    return compile_options_ == ScriptCompiler::kConsumeParserCache &&
           cached_parse_data_ != NULL;
  }
  bool produce_cached_parse_data() const {
    return compile_options_ == ScriptCompiler::kProduceParserCache;
  }

  // All ParseXXX functions take as the last argument an *ok parameter
  // which is set to false if parsing failed; it is unchanged otherwise.
  // By making the 'exception handling' explicit, we are forced to check
  // for failure at the call sites.
  void ParseStatementList(ZoneList<Statement*>* body, int end_token, bool* ok);
  Statement* ParseStatementListItem(bool* ok);
  void ParseModuleItemList(ZoneList<Statement*>* body, bool* ok);
  Statement* ParseModuleItem(bool* ok);
  const AstRawString* ParseModuleSpecifier(bool* ok);
  void ParseImportDeclaration(bool* ok);
  Statement* ParseExportDeclaration(bool* ok);
  Statement* ParseExportDefault(bool* ok);
  void ParseExportClause(ZoneList<const AstRawString*>* export_names,
                         ZoneList<Scanner::Location>* export_locations,
                         ZoneList<const AstRawString*>* local_names,
                         Scanner::Location* reserved_loc, bool* ok);
  struct NamedImport : public ZoneObject {
    const AstRawString* import_name;
    const AstRawString* local_name;
    const Scanner::Location location;
    NamedImport(const AstRawString* import_name, const AstRawString* local_name,
                Scanner::Location location)
        : import_name(import_name),
          local_name(local_name),
          location(location) {}
  };
  ZoneList<const NamedImport*>* ParseNamedImports(int pos, bool* ok);
  Statement* ParseStatement(ZoneList<const AstRawString*>* labels,
                            AllowLabelledFunctionStatement allow_function,
                            bool* ok);
  Statement* ParseSubStatement(ZoneList<const AstRawString*>* labels,
                               AllowLabelledFunctionStatement allow_function,
                               bool* ok);
  Statement* ParseStatementAsUnlabelled(ZoneList<const AstRawString*>* labels,
                                   bool* ok);
  Statement* ParseFunctionDeclaration(bool* ok);
  Statement* ParseHoistableDeclaration(ZoneList<const AstRawString*>* names,
                                       bool default_export, bool* ok);
  Statement* ParseHoistableDeclaration(int pos, ParseFunctionFlags flags,
                                       ZoneList<const AstRawString*>* names,
                                       bool default_export, bool* ok);
  Statement* ParseAsyncFunctionDeclaration(ZoneList<const AstRawString*>* names,
                                           bool default_export, bool* ok);
  Expression* ParseAsyncFunctionExpression(bool* ok);
  Statement* ParseClassDeclaration(ZoneList<const AstRawString*>* names,
                                   bool default_export, bool* ok);
  Statement* ParseNativeDeclaration(bool* ok);
  Block* ParseBlock(ZoneList<const AstRawString*>* labels, bool* ok);
  Block* ParseVariableStatement(VariableDeclarationContext var_context,
                                ZoneList<const AstRawString*>* names,
                                bool* ok);
  DoExpression* ParseDoExpression(bool* ok);
  Expression* ParseYieldStarExpression(bool* ok);

  struct DeclarationDescriptor {
    enum Kind { NORMAL, PARAMETER };
    Parser* parser;
    Scope* scope;
    Scope* hoist_scope;
    VariableMode mode;
    int declaration_pos;
    int initialization_pos;
    Kind declaration_kind;
  };

  struct DeclarationParsingResult {
    struct Declaration {
      Declaration(Expression* pattern, int initializer_position,
                  Expression* initializer)
          : pattern(pattern),
            initializer_position(initializer_position),
            initializer(initializer) {}

      Expression* pattern;
      int initializer_position;
      Expression* initializer;
    };

    DeclarationParsingResult()
        : declarations(4),
          first_initializer_loc(Scanner::Location::invalid()),
          bindings_loc(Scanner::Location::invalid()) {}

    Block* BuildInitializationBlock(ZoneList<const AstRawString*>* names,
                                    bool* ok);

    DeclarationDescriptor descriptor;
    List<Declaration> declarations;
    Scanner::Location first_initializer_loc;
    Scanner::Location bindings_loc;
  };

  class PatternRewriter final : public AstVisitor<PatternRewriter> {
   public:
    static void DeclareAndInitializeVariables(
        Block* block, const DeclarationDescriptor* declaration_descriptor,
        const DeclarationParsingResult::Declaration* declaration,
        ZoneList<const AstRawString*>* names, bool* ok);

    static void RewriteDestructuringAssignment(Parser* parser,
                                               RewritableExpression* expr,
                                               Scope* Scope);

    static Expression* RewriteDestructuringAssignment(Parser* parser,
                                                      Assignment* assignment,
                                                      Scope* scope);

   private:
    PatternRewriter() {}

#define DECLARE_VISIT(type) void Visit##type(v8::internal::type* node);
    // Visiting functions for AST nodes make this an AstVisitor.
    AST_NODE_LIST(DECLARE_VISIT)
#undef DECLARE_VISIT

    enum PatternContext {
      BINDING,
      INITIALIZER,
      ASSIGNMENT,
      ASSIGNMENT_INITIALIZER
    };

    PatternContext context() const { return context_; }
    void set_context(PatternContext context) { context_ = context; }

    void RecurseIntoSubpattern(AstNode* pattern, Expression* value) {
      Expression* old_value = current_value_;
      current_value_ = value;
      recursion_level_++;
      Visit(pattern);
      recursion_level_--;
      current_value_ = old_value;
    }

    void VisitObjectLiteral(ObjectLiteral* node, Variable** temp_var);
    void VisitArrayLiteral(ArrayLiteral* node, Variable** temp_var);

    bool IsBindingContext() const { return IsBindingContext(context_); }
    bool IsInitializerContext() const { return context_ != ASSIGNMENT; }
    bool IsAssignmentContext() const { return IsAssignmentContext(context_); }
    bool IsAssignmentContext(PatternContext c) const;
    bool IsBindingContext(PatternContext c) const;
    bool IsSubPattern() const { return recursion_level_ > 1; }
    PatternContext SetAssignmentContextIfNeeded(Expression* node);
    PatternContext SetInitializerContextIfNeeded(Expression* node);

    void RewriteParameterScopes(Expression* expr);

    Variable* CreateTempVar(Expression* value = nullptr);

    AstNodeFactory* factory() const { return parser_->factory(); }
    AstValueFactory* ast_value_factory() const {
      return parser_->ast_value_factory();
    }
    Zone* zone() const { return parser_->zone(); }
    Scope* scope() const { return scope_; }

    Scope* scope_;
    Parser* parser_;
    PatternContext context_;
    Expression* pattern_;
    int initializer_position_;
    Block* block_;
    const DeclarationDescriptor* descriptor_;
    ZoneList<const AstRawString*>* names_;
    Expression* current_value_;
    int recursion_level_;
    bool* ok_;

    DEFINE_AST_VISITOR_MEMBERS_WITHOUT_STACKOVERFLOW()
  };

  Block* ParseVariableDeclarations(VariableDeclarationContext var_context,
                                   DeclarationParsingResult* parsing_result,
                                   ZoneList<const AstRawString*>* names,
                                   bool* ok);
  Statement* ParseExpressionOrLabelledStatement(
      ZoneList<const AstRawString*>* labels,
      AllowLabelledFunctionStatement allow_function, bool* ok);
  IfStatement* ParseIfStatement(ZoneList<const AstRawString*>* labels,
                                bool* ok);
  Statement* ParseContinueStatement(bool* ok);
  Statement* ParseBreakStatement(ZoneList<const AstRawString*>* labels,
                                 bool* ok);
  Statement* ParseReturnStatement(bool* ok);
  Statement* ParseWithStatement(ZoneList<const AstRawString*>* labels,
                                bool* ok);
  CaseClause* ParseCaseClause(bool* default_seen_ptr, bool* ok);
  Statement* ParseSwitchStatement(ZoneList<const AstRawString*>* labels,
                                  bool* ok);
  DoWhileStatement* ParseDoWhileStatement(ZoneList<const AstRawString*>* labels,
                                          bool* ok);
  WhileStatement* ParseWhileStatement(ZoneList<const AstRawString*>* labels,
                                      bool* ok);
  Statement* ParseForStatement(ZoneList<const AstRawString*>* labels, bool* ok);
  Statement* ParseThrowStatement(bool* ok);
  Expression* MakeCatchContext(Handle<String> id, VariableProxy* value);
  TryStatement* ParseTryStatement(bool* ok);
  DebuggerStatement* ParseDebuggerStatement(bool* ok);
  // Parse a SubStatement in strict mode, or with an extra block scope in
  // sloppy mode to handle
  // ES#sec-functiondeclarations-in-ifstatement-statement-clauses
  // The legacy parameter indicates whether function declarations are
  // banned by the ES2015 specification in this location, and they are being
  // permitted here to match previous V8 behavior.
  Statement* ParseScopedStatement(ZoneList<const AstRawString*>* labels,
                                  bool legacy, bool* ok);

  // !%_IsJSReceiver(result = iterator.next()) &&
  //     %ThrowIteratorResultNotAnObject(result)
  Expression* BuildIteratorNextResult(Expression* iterator, Variable* result,
                                      int pos);


  // Initialize the components of a for-in / for-of statement.
  Statement* InitializeForEachStatement(ForEachStatement* stmt,
                                        Expression* each, Expression* subject,
                                        Statement* body, int each_keyword_pos);
  Statement* InitializeForOfStatement(ForOfStatement* stmt, Expression* each,
                                      Expression* iterable, Statement* body,
                                      bool finalize,
                                      int next_result_pos = kNoSourcePosition);
  Statement* DesugarLexicalBindingsInForStatement(
      Scope* inner_scope, VariableMode mode,
      ZoneList<const AstRawString*>* names, ForStatement* loop, Statement* init,
      Expression* cond, Statement* next, Statement* body, bool* ok);

  void DesugarAsyncFunctionBody(const AstRawString* function_name, Scope* scope,
                                ZoneList<Statement*>* body,
                                Type::ExpressionClassifier* classifier,
                                FunctionKind kind, FunctionBodyType type,
                                bool accept_IN, int pos, bool* ok);

  void RewriteDoExpression(Expression* expr, bool* ok);

  FunctionLiteral* ParseFunctionLiteral(
      const AstRawString* name, Scanner::Location function_name_location,
      FunctionNameValidity function_name_validity, FunctionKind kind,
      int function_token_position, FunctionLiteral::FunctionType type,
      LanguageMode language_mode, bool* ok);

  Expression* ParseClassLiteral(ExpressionClassifier* classifier,
                                const AstRawString* name,
                                Scanner::Location class_name_location,
                                bool name_is_strict_reserved, int pos,
                                bool* ok);

  // Magical syntax support.
  Expression* ParseV8Intrinsic(bool* ok);

  // Get odd-ball literals.
  Literal* GetLiteralUndefined(int position);

  // Check if the scope has conflicting var/let declarations from different
  // scopes. This covers for example
  //
  // function f() { { { var x; } let x; } }
  // function g() { { var x; let x; } }
  //
  // The var declarations are hoisted to the function scope, but originate from
  // a scope where the name has also been let bound or the var declaration is
  // hoisted over such a scope.
  void CheckConflictingVarDeclarations(Scope* scope, bool* ok);

  // Insert initializer statements for var-bindings shadowing parameter bindings
  // from a non-simple parameter list.
  void InsertShadowingVarBindingInitializers(Block* block);

  // Implement sloppy block-scoped functions, ES2015 Annex B 3.3
  void InsertSloppyBlockFunctionVarBindings(DeclarationScope* scope,
                                            Scope* complex_params_scope,
                                            bool* ok);

  static InitializationFlag DefaultInitializationFlag(VariableMode mode);
  VariableProxy* NewUnresolved(const AstRawString* name, int begin_pos,
                               int end_pos = kNoSourcePosition,
                               Variable::Kind kind = Variable::NORMAL);
  VariableProxy* NewUnresolved(const AstRawString* name);
  Variable* Declare(Declaration* declaration,
                    DeclarationDescriptor::Kind declaration_kind,
                    VariableMode mode, InitializationFlag init, bool* ok,
                    Scope* declaration_scope = nullptr);
  Declaration* DeclareVariable(const AstRawString* name, VariableMode mode,
                               int pos, bool* ok);
  Declaration* DeclareVariable(const AstRawString* name, VariableMode mode,
                               InitializationFlag init, int pos, bool* ok);

  bool TargetStackContainsLabel(const AstRawString* label);
  BreakableStatement* LookupBreakTarget(const AstRawString* label, bool* ok);
  IterationStatement* LookupContinueTarget(const AstRawString* label, bool* ok);

  Statement* BuildAssertIsCoercible(Variable* var);

  // Factory methods.
  FunctionLiteral* DefaultConstructor(const AstRawString* name, bool call_super,
                                      int pos, int end_pos,
                                      LanguageMode language_mode);

  // Skip over a lazy function, either using cached data if we have it, or
  // by parsing the function with PreParser. Consumes the ending }.
  //
  // If bookmark is set, the (pre-)parser may decide to abort skipping
  // in order to force the function to be eagerly parsed, after all.
  // In this case, it'll reset the scanner using the bookmark.
  void SkipLazyFunctionBody(int* materialized_literal_count,
                            int* expected_property_count, bool* ok,
                            Scanner::BookmarkScope* bookmark = nullptr);

  PreParser::PreParseResult ParseLazyFunctionBodyWithPreParser(
      SingletonLogger* logger, Scanner::BookmarkScope* bookmark = nullptr);

  Block* BuildParameterInitializationBlock(
      const ParserFormalParameters& parameters, bool* ok);
  Block* BuildRejectPromiseOnException(Block* block);

  // Consumes the ending }.
  ZoneList<Statement*>* ParseEagerFunctionBody(
      const AstRawString* function_name, int pos,
      const ParserFormalParameters& parameters, FunctionKind kind,
      FunctionLiteral::FunctionType function_type, bool* ok);

  void ThrowPendingError(Isolate* isolate, Handle<Script> script);

  class TemplateLiteral : public ZoneObject {
   public:
    TemplateLiteral(Zone* zone, int pos)
        : cooked_(8, zone), raw_(8, zone), expressions_(8, zone), pos_(pos) {}

    const ZoneList<Expression*>* cooked() const { return &cooked_; }
    const ZoneList<Expression*>* raw() const { return &raw_; }
    const ZoneList<Expression*>* expressions() const { return &expressions_; }
    int position() const { return pos_; }

    void AddTemplateSpan(Literal* cooked, Literal* raw, int end, Zone* zone) {
      DCHECK_NOT_NULL(cooked);
      DCHECK_NOT_NULL(raw);
      cooked_.Add(cooked, zone);
      raw_.Add(raw, zone);
    }

    void AddExpression(Expression* expression, Zone* zone) {
      DCHECK_NOT_NULL(expression);
      expressions_.Add(expression, zone);
    }

   private:
    ZoneList<Expression*> cooked_;
    ZoneList<Expression*> raw_;
    ZoneList<Expression*> expressions_;
    int pos_;
  };

  typedef TemplateLiteral* TemplateLiteralState;

  TemplateLiteralState OpenTemplateLiteral(int pos);
  void AddTemplateSpan(TemplateLiteralState* state, bool tail);
  void AddTemplateExpression(TemplateLiteralState* state,
                             Expression* expression);
  Expression* CloseTemplateLiteral(TemplateLiteralState* state, int start,
                                   Expression* tag);
  uint32_t ComputeTemplateLiteralHash(const TemplateLiteral* lit);

  void ParseAsyncArrowSingleExpressionBody(ZoneList<Statement*>* body,
                                           bool accept_IN,
                                           ExpressionClassifier* classifier,
                                           int pos, bool* ok) {
    DesugarAsyncFunctionBody(ast_value_factory()->empty_string(), scope(), body,
                             classifier, kAsyncArrowFunction,
                             FunctionBodyType::kSingleExpression, accept_IN,
                             pos, ok);
  }

  ZoneList<v8::internal::Expression*>* PrepareSpreadArguments(
      ZoneList<v8::internal::Expression*>* list);
  Expression* SpreadCall(Expression* function,
                         ZoneList<v8::internal::Expression*>* args, int pos);
  Expression* SpreadCallNew(Expression* function,
                            ZoneList<v8::internal::Expression*>* args, int pos);

  void SetLanguageMode(Scope* scope, LanguageMode mode);
  void RaiseLanguageMode(LanguageMode mode);

  V8_INLINE void MarkCollectedTailCallExpressions();
  V8_INLINE void MarkTailPosition(Expression* expression);

  // Rewrite all DestructuringAssignments in the current FunctionState.
  V8_INLINE void RewriteDestructuringAssignments();

  V8_INLINE Expression* RewriteExponentiation(Expression* left,
                                              Expression* right, int pos);
  V8_INLINE Expression* RewriteAssignExponentiation(Expression* left,
                                                    Expression* right, int pos);

  friend class NonPatternRewriter;
  V8_INLINE Expression* RewriteSpreads(ArrayLiteral* lit);

  // Rewrite expressions that are not used as patterns
  V8_INLINE void RewriteNonPattern(ExpressionClassifier* classifier, bool* ok);

  V8_INLINE void QueueDestructuringAssignmentForRewriting(
      Expression* assignment);
  V8_INLINE void QueueNonPatternForRewriting(Expression* expr, bool* ok);

  friend class InitializerRewriter;
  void RewriteParameterInitializer(Expression* expr, Scope* scope);

  Expression* BuildCreateJSGeneratorObject(int pos, FunctionKind kind);
  Expression* BuildPromiseResolve(Expression* value, int pos);
  Expression* BuildPromiseReject(Expression* value, int pos);

  // Generic AST generator for throwing errors from compiled code.
  Expression* NewThrowError(Runtime::FunctionId function_id,
                            MessageTemplate::Template message,
                            const AstRawString* arg, int pos);

  void FinalizeIteratorUse(Variable* completion, Expression* condition,
                           Variable* iter, Block* iterator_use, Block* result);

  Statement* FinalizeForOfStatement(ForOfStatement* loop, Variable* completion,
                                    int pos);
  void BuildIteratorClose(ZoneList<Statement*>* statements, Variable* iterator,
                          Variable* input, Variable* output);
  void BuildIteratorCloseForCompletion(ZoneList<Statement*>* statements,
                                       Variable* iterator,
                                       Expression* completion);
  Statement* CheckCallable(Variable* var, Expression* error, int pos);

  V8_INLINE Expression* RewriteAwaitExpression(Expression* value, int pos);

  Expression* RewriteYieldStar(Expression* generator, Expression* expression,
                               int pos);

  void ParseArrowFunctionFormalParameters(ParserFormalParameters* parameters,
                                          Expression* params, int end_pos,
                                          bool* ok);
  void SetFunctionName(Expression* value, const AstRawString* name);

  Scanner scanner_;
  PreParser* reusable_preparser_;
  Scope* original_scope_;  // for ES5 function declarations in sloppy eval
  Target* target_stack_;  // for break, continue statements
  ScriptCompiler::CompileOptions compile_options_;
  ParseData* cached_parse_data_;

  PendingCompilationErrorHandler pending_error_handler_;

  // Other information which will be stored in Parser and moved to Isolate after
  // parsing.
  int use_counts_[v8::Isolate::kUseCounterFeatureCount];
  int total_preparse_skipped_;
  HistogramTimer* pre_parse_timer_;

  bool parsing_on_main_thread_;

#ifdef DEBUG
  void Print(AstNode* node);
#endif  // DEBUG
};

bool ParserBaseTraits<Parser>::IsFutureStrictReserved(
    const AstRawString* identifier) const {
  return delegate()->scanner()->IdentifierIsFutureStrictReserved(identifier);
}

const AstRawString* ParserBaseTraits<Parser>::EmptyIdentifierString() const {
  return delegate()->ast_value_factory()->empty_string();
}


// Support for handling complex values (array and object literals) that
// can be fully handled at compile time.
class CompileTimeValue: public AllStatic {
 public:
  enum LiteralType {
    OBJECT_LITERAL_FAST_ELEMENTS,
    OBJECT_LITERAL_SLOW_ELEMENTS,
    ARRAY_LITERAL
  };

  static bool IsCompileTimeValue(Expression* expression);

  // Get the value as a compile time value.
  static Handle<FixedArray> GetValue(Isolate* isolate, Expression* expression);

  // Get the type of a compile time value returned by GetValue().
  static LiteralType GetLiteralType(Handle<FixedArray> value);

  // Get the elements array of a compile time value returned by GetValue().
  static Handle<FixedArray> GetElements(Handle<FixedArray> value);

 private:
  static const int kLiteralTypeSlot = 0;
  static const int kElementsSlot = 1;

  DISALLOW_IMPLICIT_CONSTRUCTORS(CompileTimeValue);
};

void ParserBaseTraits<Parser>::AddFormalParameter(
    ParserFormalParameters* parameters, Expression* pattern,
    Expression* initializer, int initializer_end_position, bool is_rest) {
  bool is_simple = pattern->IsVariableProxy() && initializer == nullptr;
  const AstRawString* name =
      is_simple ? pattern->AsVariableProxy()->raw_name()
                : delegate()->ast_value_factory()->empty_string();
  parameters->params.Add(
      ParserFormalParameters::Parameter(name, pattern, initializer,
                                        initializer_end_position, is_rest),
      parameters->scope->zone());
}

void ParserBaseTraits<Parser>::DeclareFormalParameter(
    DeclarationScope* scope, const ParserFormalParameters::Parameter& parameter,
    Type::ExpressionClassifier* classifier) {
  bool is_duplicate = false;
  bool is_simple = classifier->is_simple_parameter_list();
  auto name = is_simple || parameter.is_rest
                  ? parameter.name
                  : delegate()->ast_value_factory()->empty_string();
  auto mode = is_simple || parameter.is_rest ? VAR : TEMPORARY;
  if (!is_simple) scope->SetHasNonSimpleParameters();
  bool is_optional = parameter.initializer != nullptr;
  Variable* var =
      scope->DeclareParameter(name, mode, is_optional, parameter.is_rest,
                              &is_duplicate, delegate()->ast_value_factory());
  if (is_duplicate) {
    classifier->RecordDuplicateFormalParameterError(
        delegate()->scanner()->location());
  }
  if (is_sloppy(scope->language_mode())) {
    // TODO(sigurds) Mark every parameter as maybe assigned. This is a
    // conservative approximation necessary to account for parameters
    // that are assigned via the arguments array.
    var->set_maybe_assigned();
  }
}

void ParserBaseTraits<Parser>::AddParameterInitializationBlock(
    const ParserFormalParameters& parameters,
    ZoneList<v8::internal::Statement*>* body, bool is_async, bool* ok) {
  if (!parameters.is_simple) {
    auto* init_block =
        delegate()->BuildParameterInitializationBlock(parameters, ok);
    if (!*ok) return;

    if (is_async) {
      init_block = delegate()->BuildRejectPromiseOnException(init_block);
    }

    if (init_block != nullptr) {
      body->Add(init_block, delegate()->zone());
    }
  }
}

}  // namespace internal
}  // namespace v8

#endif  // V8_PARSING_PARSER_H_
