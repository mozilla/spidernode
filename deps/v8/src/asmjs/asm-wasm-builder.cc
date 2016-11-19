// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

// Required to get M_E etc. in MSVC.
#if defined(_WIN32)
#define _USE_MATH_DEFINES
#endif
#include <math.h>

#include "src/asmjs/asm-types.h"
#include "src/asmjs/asm-wasm-builder.h"
#include "src/wasm/switch-logic.h"
#include "src/wasm/wasm-macro-gen.h"
#include "src/wasm/wasm-opcodes.h"

#include "src/ast/ast.h"
#include "src/ast/scopes.h"
#include "src/codegen.h"

namespace v8 {
namespace internal {
namespace wasm {

#define RECURSE(call)               \
  do {                              \
    DCHECK(!HasStackOverflow());    \
    call;                           \
    if (HasStackOverflow()) return; \
  } while (false)

enum AsmScope { kModuleScope, kInitScope, kFuncScope, kExportScope };

struct ForeignVariable {
  Handle<Name> name;
  Variable* var;
  LocalType type;
};

class AsmWasmBuilderImpl final : public AstVisitor<AsmWasmBuilderImpl> {
 public:
  AsmWasmBuilderImpl(Isolate* isolate, Zone* zone, FunctionLiteral* literal,
                     AsmTyper* typer)
      : local_variables_(base::HashMap::PointersMatch,
                         ZoneHashMap::kDefaultHashMapCapacity,
                         ZoneAllocationPolicy(zone)),
        functions_(base::HashMap::PointersMatch,
                   ZoneHashMap::kDefaultHashMapCapacity,
                   ZoneAllocationPolicy(zone)),
        global_variables_(base::HashMap::PointersMatch,
                          ZoneHashMap::kDefaultHashMapCapacity,
                          ZoneAllocationPolicy(zone)),
        scope_(kModuleScope),
        builder_(new (zone) WasmModuleBuilder(zone)),
        current_function_builder_(nullptr),
        literal_(literal),
        isolate_(isolate),
        zone_(zone),
        typer_(typer),
        breakable_blocks_(zone),
        foreign_variables_(zone),
        init_function_index_(0),
        foreign_init_function_index_(0),
        next_table_index_(0),
        function_tables_(base::HashMap::PointersMatch,
                         ZoneHashMap::kDefaultHashMapCapacity,
                         ZoneAllocationPolicy(zone)),
        imported_function_table_(this) {
    InitializeAstVisitor(isolate);
  }

  void InitializeInitFunction() {
    init_function_index_ = builder_->AddFunction();
    FunctionSig::Builder b(zone(), 0, 0);
    current_function_builder_ = builder_->FunctionAt(init_function_index_);
    current_function_builder_->SetSignature(b.Build());
    builder_->MarkStartFunction(init_function_index_);
    current_function_builder_ = nullptr;
  }

  void BuildForeignInitFunction() {
    foreign_init_function_index_ = builder_->AddFunction();
    FunctionSig::Builder b(zone(), 0, foreign_variables_.size());
    for (auto i = foreign_variables_.begin(); i != foreign_variables_.end();
         ++i) {
      b.AddParam(i->type);
    }
    current_function_builder_ =
        builder_->FunctionAt(foreign_init_function_index_);
    current_function_builder_->SetExported();
    std::string raw_name = "__foreign_init__";
    current_function_builder_->SetName(raw_name.data(),
                                       static_cast<int>(raw_name.size()));
    current_function_builder_->SetSignature(b.Build());
    for (size_t pos = 0; pos < foreign_variables_.size(); ++pos) {
      current_function_builder_->EmitGetLocal(static_cast<uint32_t>(pos));
      ForeignVariable* fv = &foreign_variables_[pos];
      uint32_t index = LookupOrInsertGlobal(fv->var, fv->type);
      current_function_builder_->EmitWithVarInt(kExprSetGlobal, index);
    }
    current_function_builder_ = nullptr;
  }

  i::Handle<i::FixedArray> GetForeignArgs() {
    i::Handle<FixedArray> ret = isolate_->factory()->NewFixedArray(
        static_cast<int>(foreign_variables_.size()));
    for (size_t i = 0; i < foreign_variables_.size(); ++i) {
      ForeignVariable* fv = &foreign_variables_[i];
      ret->set(static_cast<int>(i), *fv->name);
    }
    return ret;
  }

  void Build() {
    InitializeInitFunction();
    RECURSE(VisitFunctionLiteral(literal_));
    BuildForeignInitFunction();
  }

  void VisitVariableDeclaration(VariableDeclaration* decl) {}

  void VisitFunctionDeclaration(FunctionDeclaration* decl) {
    DCHECK_EQ(kModuleScope, scope_);
    DCHECK_NULL(current_function_builder_);
    uint32_t index = LookupOrInsertFunction(decl->proxy()->var());
    current_function_builder_ = builder_->FunctionAt(index);
    scope_ = kFuncScope;
    RECURSE(Visit(decl->fun()));
    scope_ = kModuleScope;
    current_function_builder_ = nullptr;
    local_variables_.Clear();
  }

  void VisitStatements(ZoneList<Statement*>* stmts) {
    for (int i = 0; i < stmts->length(); ++i) {
      Statement* stmt = stmts->at(i);
      ExpressionStatement* e = stmt->AsExpressionStatement();
      if (e != nullptr && e->expression()->IsUndefinedLiteral()) {
        continue;
      }
      RECURSE(Visit(stmt));
      if (stmt->IsJump()) break;
    }
  }

  void VisitBlock(Block* stmt) {
    if (stmt->statements()->length() == 1) {
      ExpressionStatement* expr =
          stmt->statements()->at(0)->AsExpressionStatement();
      if (expr != nullptr) {
        if (expr->expression()->IsAssignment()) {
          RECURSE(VisitExpressionStatement(expr));
          return;
        }
      }
    }
    if (scope_ == kFuncScope) {
      BlockVisitor visitor(this, stmt->AsBreakableStatement(), kExprBlock,
                           false);
      RECURSE(VisitStatements(stmt->statements()));
    } else {
      RECURSE(VisitStatements(stmt->statements()));
    }
  }

  class BlockVisitor {
   private:
    AsmWasmBuilderImpl* builder_;

   public:
    BlockVisitor(AsmWasmBuilderImpl* builder, BreakableStatement* stmt,
                 WasmOpcode opcode, bool is_loop)
        : builder_(builder) {
      builder_->breakable_blocks_.push_back(std::make_pair(stmt, is_loop));
      builder_->current_function_builder_->Emit(opcode);
    }
    ~BlockVisitor() {
      builder_->current_function_builder_->Emit(kExprEnd);
      builder_->breakable_blocks_.pop_back();
    }
  };

  void VisitExpressionStatement(ExpressionStatement* stmt) {
    RECURSE(Visit(stmt->expression()));
  }

  void VisitEmptyStatement(EmptyStatement* stmt) {}

  void VisitEmptyParentheses(EmptyParentheses* paren) { UNREACHABLE(); }

  void VisitIfStatement(IfStatement* stmt) {
    DCHECK_EQ(kFuncScope, scope_);
    RECURSE(Visit(stmt->condition()));
    current_function_builder_->Emit(kExprIf);
    // WASM ifs come with implement blocks for both arms.
    breakable_blocks_.push_back(std::make_pair(nullptr, false));
    if (stmt->HasThenStatement()) {
      RECURSE(Visit(stmt->then_statement()));
    }
    if (stmt->HasElseStatement()) {
      current_function_builder_->Emit(kExprElse);
      RECURSE(Visit(stmt->else_statement()));
    }
    current_function_builder_->Emit(kExprEnd);
    breakable_blocks_.pop_back();
  }

  void VisitContinueStatement(ContinueStatement* stmt) {
    DCHECK_EQ(kFuncScope, scope_);
    DCHECK_NOT_NULL(stmt->target());
    int i = static_cast<int>(breakable_blocks_.size()) - 1;
    int block_distance = 0;
    for (; i >= 0; i--) {
      auto elem = breakable_blocks_.at(i);
      if (elem.first == stmt->target()) {
        DCHECK(elem.second);
        break;
      } else if (elem.second) {
        block_distance += 2;
      } else {
        block_distance += 1;
      }
    }
    DCHECK(i >= 0);
    current_function_builder_->EmitWithU8(kExprBr, ARITY_0);
    current_function_builder_->EmitVarInt(block_distance);
  }

  void VisitBreakStatement(BreakStatement* stmt) {
    DCHECK_EQ(kFuncScope, scope_);
    DCHECK_NOT_NULL(stmt->target());
    int i = static_cast<int>(breakable_blocks_.size()) - 1;
    int block_distance = 0;
    for (; i >= 0; i--) {
      auto elem = breakable_blocks_.at(i);
      if (elem.first == stmt->target()) {
        if (elem.second) {
          block_distance++;
        }
        break;
      } else if (elem.second) {
        block_distance += 2;
      } else {
        block_distance += 1;
      }
    }
    DCHECK(i >= 0);
    current_function_builder_->EmitWithU8(kExprBr, ARITY_0);
    current_function_builder_->EmitVarInt(block_distance);
  }

  void VisitReturnStatement(ReturnStatement* stmt) {
    if (scope_ == kModuleScope) {
      scope_ = kExportScope;
      RECURSE(Visit(stmt->expression()));
      scope_ = kModuleScope;
    } else if (scope_ == kFuncScope) {
      RECURSE(Visit(stmt->expression()));
      uint8_t arity =
          TypeOf(stmt->expression()) == kAstStmt ? ARITY_0 : ARITY_1;
      current_function_builder_->EmitWithU8(kExprReturn, arity);
    } else {
      UNREACHABLE();
    }
  }

  void VisitWithStatement(WithStatement* stmt) { UNREACHABLE(); }

  void HandleCase(CaseNode* node,
                  ZoneMap<int, unsigned int>& case_to_block,
                  VariableProxy* tag, int default_block, int if_depth) {
    int prev_if_depth = if_depth;
    if (node->left != nullptr) {
      VisitVariableProxy(tag);
      current_function_builder_->EmitI32Const(node->begin);
      current_function_builder_->Emit(kExprI32LtS);
      current_function_builder_->Emit(kExprIf);
      if_depth++;
      breakable_blocks_.push_back(std::make_pair(nullptr, false));
      HandleCase(node->left, case_to_block, tag, default_block, if_depth);
      current_function_builder_->Emit(kExprElse);
    }
    if (node->right != nullptr) {
      VisitVariableProxy(tag);
      current_function_builder_->EmitI32Const(node->end);
      current_function_builder_->Emit(kExprI32GtS);
      current_function_builder_->Emit(kExprIf);
      if_depth++;
      breakable_blocks_.push_back(std::make_pair(nullptr, false));
      HandleCase(node->right, case_to_block, tag, default_block, if_depth);
      current_function_builder_->Emit(kExprElse);
    }
    if (node->begin == node->end) {
      VisitVariableProxy(tag);
      current_function_builder_->EmitI32Const(node->begin);
      current_function_builder_->Emit(kExprI32Eq);
      current_function_builder_->Emit(kExprIf);
      DCHECK(case_to_block.find(node->begin) != case_to_block.end());
      current_function_builder_->EmitWithU8(kExprBr, ARITY_0);
      current_function_builder_->EmitVarInt(1 + if_depth +
                                            case_to_block[node->begin]);
      current_function_builder_->Emit(kExprEnd);
    } else {
      if (node->begin != 0) {
        VisitVariableProxy(tag);
        current_function_builder_->EmitI32Const(node->begin);
        current_function_builder_->Emit(kExprI32Sub);
      } else {
        VisitVariableProxy(tag);
      }
      current_function_builder_->EmitWithU8(kExprBrTable, ARITY_0);
      current_function_builder_->EmitVarInt(node->end - node->begin + 1);
      for (int v = node->begin; v <= node->end; v++) {
        if (case_to_block.find(v) != case_to_block.end()) {
          byte break_code[] = {BR_TARGET(if_depth + case_to_block[v])};
          current_function_builder_->EmitCode(break_code, sizeof(break_code));
        } else {
          byte break_code[] = {BR_TARGET(if_depth + default_block)};
          current_function_builder_->EmitCode(break_code, sizeof(break_code));
        }
        if (v == kMaxInt) {
          break;
        }
      }
      byte break_code[] = {BR_TARGET(if_depth + default_block)};
      current_function_builder_->EmitCode(break_code, sizeof(break_code));
    }

    while (if_depth-- != prev_if_depth) {
      breakable_blocks_.pop_back();
      current_function_builder_->Emit(kExprEnd);
    }
  }

  void VisitSwitchStatement(SwitchStatement* stmt) {
    VariableProxy* tag = stmt->tag()->AsVariableProxy();
    DCHECK_NOT_NULL(tag);
    ZoneList<CaseClause*>* clauses = stmt->cases();
    int case_count = clauses->length();
    if (case_count == 0) {
      return;
    }
    BlockVisitor visitor(this, stmt->AsBreakableStatement(), kExprBlock, false);
    ZoneVector<BlockVisitor*> blocks(zone_);
    ZoneVector<int32_t> cases(zone_);
    ZoneMap<int, unsigned int> case_to_block(zone_);
    bool has_default = false;
    for (int i = case_count - 1; i >= 0; i--) {
      CaseClause* clause = clauses->at(i);
      blocks.push_back(new BlockVisitor(this, nullptr, kExprBlock, false));
      if (!clause->is_default()) {
        Literal* label = clause->label()->AsLiteral();
        Handle<Object> value = label->value();
        int32_t label_value;
        bool label_is_i32 = value->ToInt32(&label_value);
        DCHECK(value->IsNumber() && label_is_i32);
        (void)label_is_i32;
        case_to_block[label_value] = i;
        cases.push_back(label_value);
      } else {
        DCHECK_EQ(i, case_count - 1);
        has_default = true;
      }
    }
    if (!has_default || case_count > 1) {
      int default_block = has_default ? case_count - 1 : case_count;
      BlockVisitor switch_logic_block(this, nullptr, kExprBlock, false);
      CaseNode* root = OrderCases(&cases, zone_);
      HandleCase(root, case_to_block, tag, default_block, 0);
      if (root->left != nullptr || root->right != nullptr ||
          root->begin == root->end) {
        current_function_builder_->EmitWithU8(kExprBr, ARITY_0);
        current_function_builder_->EmitVarInt(default_block);
      }
    }
    for (int i = 0; i < case_count; ++i) {
      CaseClause* clause = clauses->at(i);
      RECURSE(VisitStatements(clause->statements()));
      BlockVisitor* v = blocks.at(case_count - i - 1);
      blocks.pop_back();
      delete v;
    }
  }

  void VisitCaseClause(CaseClause* clause) { UNREACHABLE(); }

  void VisitDoWhileStatement(DoWhileStatement* stmt) {
    DCHECK_EQ(kFuncScope, scope_);
    BlockVisitor visitor(this, stmt->AsBreakableStatement(), kExprLoop, true);
    RECURSE(Visit(stmt->body()));
    RECURSE(Visit(stmt->cond()));
    current_function_builder_->Emit(kExprIf);
    current_function_builder_->EmitWithU8U8(kExprBr, ARITY_0, 1);
    current_function_builder_->Emit(kExprEnd);
  }

  void VisitWhileStatement(WhileStatement* stmt) {
    DCHECK_EQ(kFuncScope, scope_);
    BlockVisitor visitor(this, stmt->AsBreakableStatement(), kExprLoop, true);
    RECURSE(Visit(stmt->cond()));
    breakable_blocks_.push_back(std::make_pair(nullptr, false));
    current_function_builder_->Emit(kExprIf);
    RECURSE(Visit(stmt->body()));
    current_function_builder_->EmitWithU8U8(kExprBr, ARITY_0, 1);
    current_function_builder_->Emit(kExprEnd);
    breakable_blocks_.pop_back();
  }

  void VisitForStatement(ForStatement* stmt) {
    DCHECK_EQ(kFuncScope, scope_);
    if (stmt->init() != nullptr) {
      RECURSE(Visit(stmt->init()));
    }
    BlockVisitor visitor(this, stmt->AsBreakableStatement(), kExprLoop, true);
    if (stmt->cond() != nullptr) {
      RECURSE(Visit(stmt->cond()));
      current_function_builder_->Emit(kExprI32Eqz);
      current_function_builder_->Emit(kExprIf);
      current_function_builder_->Emit(kExprNop);
      current_function_builder_->EmitWithU8U8(kExprBr, ARITY_0, 2);
      current_function_builder_->Emit(kExprEnd);
    }
    if (stmt->body() != nullptr) {
      RECURSE(Visit(stmt->body()));
    }
    if (stmt->next() != nullptr) {
      RECURSE(Visit(stmt->next()));
    }
    current_function_builder_->Emit(kExprNop);
    current_function_builder_->EmitWithU8U8(kExprBr, ARITY_0, 0);
  }

  void VisitForInStatement(ForInStatement* stmt) { UNREACHABLE(); }

  void VisitForOfStatement(ForOfStatement* stmt) { UNREACHABLE(); }

  void VisitTryCatchStatement(TryCatchStatement* stmt) { UNREACHABLE(); }

  void VisitTryFinallyStatement(TryFinallyStatement* stmt) { UNREACHABLE(); }

  void VisitDebuggerStatement(DebuggerStatement* stmt) { UNREACHABLE(); }

  void VisitFunctionLiteral(FunctionLiteral* expr) {
    DeclarationScope* scope = expr->scope();
    if (scope_ == kFuncScope) {
      if (auto* func_type = typer_->TypeOf(expr)->AsFunctionType()) {
        // Build the signature for the function.
        LocalType return_type = TypeFrom(func_type->ReturnType());
        const auto& arguments = func_type->Arguments();
        FunctionSig::Builder b(zone(), return_type == kAstStmt ? 0 : 1,
                               arguments.size());
        if (return_type != kAstStmt) b.AddReturn(return_type);
        for (int i = 0; i < expr->parameter_count(); ++i) {
          LocalType type = TypeFrom(arguments[i]);
          DCHECK_NE(kAstStmt, type);
          b.AddParam(type);
          InsertParameter(scope->parameter(i), type, i);
        }
        current_function_builder_->SetSignature(b.Build());
      } else {
        UNREACHABLE();
      }
    }
    RECURSE(VisitStatements(expr->body()));
    RECURSE(VisitDeclarations(scope->declarations()));
  }

  void VisitNativeFunctionLiteral(NativeFunctionLiteral* expr) {
    UNREACHABLE();
  }

  void VisitConditional(Conditional* expr) {
    DCHECK_EQ(kFuncScope, scope_);
    RECURSE(Visit(expr->condition()));
    // WASM ifs come with implicit blocks for both arms.
    breakable_blocks_.push_back(std::make_pair(nullptr, false));
    current_function_builder_->Emit(kExprIf);
    RECURSE(Visit(expr->then_expression()));
    current_function_builder_->Emit(kExprElse);
    RECURSE(Visit(expr->else_expression()));
    current_function_builder_->Emit(kExprEnd);
    breakable_blocks_.pop_back();
  }

  bool VisitStdlibConstant(Variable* var) {
    AsmTyper::StandardMember standard_object =
        typer_->VariableAsStandardMember(var);
    double value;
    switch (standard_object) {
      case AsmTyper::kInfinity: {
        value = std::numeric_limits<double>::infinity();
        break;
      }
      case AsmTyper::kNaN: {
        value = std::numeric_limits<double>::quiet_NaN();
        break;
      }
      case AsmTyper::kMathE: {
        value = M_E;
        break;
      }
      case AsmTyper::kMathLN10: {
        value = M_LN10;
        break;
      }
      case AsmTyper::kMathLN2: {
        value = M_LN2;
        break;
      }
      case AsmTyper::kMathLOG10E: {
        value = M_LOG10E;
        break;
      }
      case AsmTyper::kMathLOG2E: {
        value = M_LOG2E;
        break;
      }
      case AsmTyper::kMathPI: {
        value = M_PI;
        break;
      }
      case AsmTyper::kMathSQRT1_2: {
        value = M_SQRT1_2;
        break;
      }
      case AsmTyper::kMathSQRT2: {
        value = M_SQRT2;
        break;
      }
      default: { return false; }
    }
    byte code[] = {WASM_F64(value)};
    current_function_builder_->EmitCode(code, sizeof(code));
    return true;
  }

  void VisitVariableProxy(VariableProxy* expr) {
    if (scope_ == kFuncScope || scope_ == kInitScope) {
      Variable* var = expr->var();
      if (VisitStdlibConstant(var)) {
        return;
      }
      LocalType var_type = TypeOf(expr);
      DCHECK_NE(kAstStmt, var_type);
      if (var->IsContextSlot()) {
        current_function_builder_->EmitWithVarInt(
            kExprGetGlobal, LookupOrInsertGlobal(var, var_type));
      } else {
        current_function_builder_->EmitGetLocal(
            LookupOrInsertLocal(var, var_type));
      }
    }
  }

  void VisitLiteral(Literal* expr) {
    Handle<Object> value = expr->value();
    if (!value->IsNumber() || (scope_ != kFuncScope && scope_ != kInitScope)) {
      return;
    }
    AsmType* type = typer_->TypeOf(expr);
    DCHECK_NE(type, AsmType::None());

    if (type->IsA(AsmType::Signed())) {
      int32_t i = 0;
      if (!value->ToInt32(&i)) {
        UNREACHABLE();
      }
      byte code[] = {WASM_I32V(i)};
      current_function_builder_->EmitCode(code, sizeof(code));
    } else if (type->IsA(AsmType::Unsigned()) || type->IsA(AsmType::FixNum())) {
      uint32_t u = 0;
      if (!value->ToUint32(&u)) {
        UNREACHABLE();
      }
      int32_t i = static_cast<int32_t>(u);
      byte code[] = {WASM_I32V(i)};
      current_function_builder_->EmitCode(code, sizeof(code));
    } else if (type->IsA(AsmType::Double())) {
      double val = expr->raw_value()->AsNumber();
      byte code[] = {WASM_F64(val)};
      current_function_builder_->EmitCode(code, sizeof(code));
    } else {
      UNREACHABLE();
    }
  }

  void VisitRegExpLiteral(RegExpLiteral* expr) { UNREACHABLE(); }

  void VisitObjectLiteral(ObjectLiteral* expr) {
    ZoneList<ObjectLiteralProperty*>* props = expr->properties();
    for (int i = 0; i < props->length(); ++i) {
      ObjectLiteralProperty* prop = props->at(i);
      DCHECK_EQ(kExportScope, scope_);
      VariableProxy* expr = prop->value()->AsVariableProxy();
      DCHECK_NOT_NULL(expr);
      Variable* var = expr->var();
      Literal* name = prop->key()->AsLiteral();
      DCHECK_NOT_NULL(name);
      DCHECK(name->IsPropertyName());
      const AstRawString* raw_name = name->AsRawPropertyName();
      if (var->is_function()) {
        uint32_t index = LookupOrInsertFunction(var);
        builder_->FunctionAt(index)->SetExported();
        builder_->FunctionAt(index)->SetName(
            reinterpret_cast<const char*>(raw_name->raw_data()),
            raw_name->length());
      }
    }
  }

  void VisitArrayLiteral(ArrayLiteral* expr) { UNREACHABLE(); }

  void LoadInitFunction() {
    current_function_builder_ = builder_->FunctionAt(init_function_index_);
    scope_ = kInitScope;
  }

  void UnLoadInitFunction() {
    scope_ = kModuleScope;
    current_function_builder_ = nullptr;
  }

  void AddFunctionTable(VariableProxy* table, ArrayLiteral* funcs) {
    auto* func_tbl_type = typer_->TypeOf(funcs)->AsFunctionTableType();
    DCHECK_NOT_NULL(func_tbl_type);
    auto* func_type = func_tbl_type->signature()->AsFunctionType();
    const auto& arguments = func_type->Arguments();
    LocalType return_type = TypeFrom(func_type->ReturnType());
    FunctionSig::Builder sig(zone(), return_type == kAstStmt ? 0 : 1,
                             arguments.size());
    if (return_type != kAstStmt) {
      sig.AddReturn(return_type);
    }
    for (auto* arg : arguments) {
      sig.AddParam(TypeFrom(arg));
    }
    uint32_t signature_index = builder_->AddSignature(sig.Build());
    InsertFunctionTable(table->var(), next_table_index_, signature_index);
    next_table_index_ += funcs->values()->length();
    for (int i = 0; i < funcs->values()->length(); ++i) {
      VariableProxy* func = funcs->values()->at(i)->AsVariableProxy();
      DCHECK_NOT_NULL(func);
      builder_->AddIndirectFunction(LookupOrInsertFunction(func->var()));
    }
  }

  struct FunctionTableIndices : public ZoneObject {
    uint32_t start_index;
    uint32_t signature_index;
  };

  void InsertFunctionTable(Variable* v, uint32_t start_index,
                           uint32_t signature_index) {
    FunctionTableIndices* container = new (zone()) FunctionTableIndices();
    container->start_index = start_index;
    container->signature_index = signature_index;
    ZoneHashMap::Entry* entry = function_tables_.LookupOrInsert(
        v, ComputePointerHash(v), ZoneAllocationPolicy(zone()));
    entry->value = container;
  }

  FunctionTableIndices* LookupFunctionTable(Variable* v) {
    ZoneHashMap::Entry* entry =
        function_tables_.Lookup(v, ComputePointerHash(v));
    DCHECK_NOT_NULL(entry);
    return reinterpret_cast<FunctionTableIndices*>(entry->value);
  }

  class ImportedFunctionTable {
   private:
    class ImportedFunctionIndices : public ZoneObject {
     public:
      const char* name_;
      int name_length_;
      WasmModuleBuilder::SignatureMap signature_to_index_;

      ImportedFunctionIndices(const char* name, int name_length, Zone* zone)
          : name_(name), name_length_(name_length), signature_to_index_(zone) {}
    };
    ZoneHashMap table_;
    AsmWasmBuilderImpl* builder_;

   public:
    explicit ImportedFunctionTable(AsmWasmBuilderImpl* builder)
        : table_(base::HashMap::PointersMatch,
                 ZoneHashMap::kDefaultHashMapCapacity,
                 ZoneAllocationPolicy(builder->zone())),
          builder_(builder) {}

    void AddImport(Variable* v, const char* name, int name_length) {
      ImportedFunctionIndices* indices = new (builder_->zone())
          ImportedFunctionIndices(name, name_length, builder_->zone());
      ZoneHashMap::Entry* entry = table_.LookupOrInsert(
          v, ComputePointerHash(v), ZoneAllocationPolicy(builder_->zone()));
      entry->value = indices;
    }

    uint32_t GetFunctionIndex(Variable* v, FunctionSig* sig) {
      ZoneHashMap::Entry* entry = table_.Lookup(v, ComputePointerHash(v));
      DCHECK_NOT_NULL(entry);
      ImportedFunctionIndices* indices =
          reinterpret_cast<ImportedFunctionIndices*>(entry->value);
      WasmModuleBuilder::SignatureMap::iterator pos =
          indices->signature_to_index_.find(sig);
      if (pos != indices->signature_to_index_.end()) {
        return pos->second;
      } else {
        uint32_t index = builder_->builder_->AddImport(
            indices->name_, indices->name_length_, sig);
        indices->signature_to_index_[sig] = index;
        return index;
      }
    }
  };

  void EmitAssignmentLhs(Expression* target, MachineType* mtype) {
    // Match the left hand side of the assignment.
    VariableProxy* target_var = target->AsVariableProxy();
    if (target_var != nullptr) {
      // Left hand side is a local or a global variable, no code on LHS.
      return;
    }

    Property* target_prop = target->AsProperty();
    if (target_prop != nullptr) {
      // Left hand side is a property access, i.e. the asm.js heap.
      VisitPropertyAndEmitIndex(target_prop, mtype);
      return;
    }

    if (target_var == nullptr && target_prop == nullptr) {
      UNREACHABLE();  // invalid assignment.
    }
  }

  void EmitAssignmentRhs(Expression* target, Expression* value, bool* is_nop) {
    BinaryOperation* binop = value->AsBinaryOperation();
    if (binop != nullptr) {
      if (scope_ == kInitScope) {
        // Handle foreign variables in the initialization scope.
        Property* prop = binop->left()->AsProperty();
        if (binop->op() == Token::MUL) {
          DCHECK(binop->right()->IsLiteral());
          DCHECK_EQ(1.0, binop->right()->AsLiteral()->raw_value()->AsNumber());
          DCHECK(binop->right()->AsLiteral()->raw_value()->ContainsDot());
          DCHECK(target->IsVariableProxy());
          VisitForeignVariable(true, target->AsVariableProxy()->var(), prop);
          *is_nop = true;
          return;
        } else if (binop->op() == Token::BIT_OR) {
          DCHECK(binop->right()->IsLiteral());
          DCHECK_EQ(0.0, binop->right()->AsLiteral()->raw_value()->AsNumber());
          DCHECK(!binop->right()->AsLiteral()->raw_value()->ContainsDot());
          DCHECK(target->IsVariableProxy());
          VisitForeignVariable(false, target->AsVariableProxy()->var(), prop);
          *is_nop = true;
          return;
        } else {
          UNREACHABLE();
        }
      }
      if (MatchBinaryOperation(binop) == kAsIs) {
        VariableProxy* target_var = target->AsVariableProxy();
        VariableProxy* effective_value_var = GetLeft(binop)->AsVariableProxy();
        if (target_var != nullptr && effective_value_var != nullptr &&
            target_var->var() == effective_value_var->var()) {
          *is_nop = true;
          return;
        }
      }
    }
    RECURSE(Visit(value));
  }

  void EmitAssignment(Assignment* expr, MachineType type) {
    // Match the left hand side of the assignment.
    VariableProxy* target_var = expr->target()->AsVariableProxy();
    if (target_var != nullptr) {
      // Left hand side is a local or a global variable.
      Variable* var = target_var->var();
      LocalType var_type = TypeOf(expr);
      DCHECK_NE(kAstStmt, var_type);
      if (var->IsContextSlot()) {
        current_function_builder_->EmitWithVarInt(
            kExprSetGlobal, LookupOrInsertGlobal(var, var_type));
      } else {
        current_function_builder_->EmitSetLocal(
            LookupOrInsertLocal(var, var_type));
      }
    }

    Property* target_prop = expr->target()->AsProperty();
    if (target_prop != nullptr) {
      // Left hand side is a property access, i.e. the asm.js heap.
      if (TypeOf(expr->value()) == kAstF64 && expr->target()->IsProperty() &&
          typer_->TypeOf(expr->target()->AsProperty()->obj())
              ->IsA(AsmType::Float32Array())) {
        current_function_builder_->Emit(kExprF32ConvertF64);
      }
      WasmOpcode opcode;
      if (type == MachineType::Int8()) {
        opcode = kExprI32AsmjsStoreMem8;
      } else if (type == MachineType::Uint8()) {
        opcode = kExprI32AsmjsStoreMem8;
      } else if (type == MachineType::Int16()) {
        opcode = kExprI32AsmjsStoreMem16;
      } else if (type == MachineType::Uint16()) {
        opcode = kExprI32AsmjsStoreMem16;
      } else if (type == MachineType::Int32()) {
        opcode = kExprI32AsmjsStoreMem;
      } else if (type == MachineType::Uint32()) {
        opcode = kExprI32AsmjsStoreMem;
      } else if (type == MachineType::Float32()) {
        opcode = kExprF32AsmjsStoreMem;
      } else if (type == MachineType::Float64()) {
        opcode = kExprF64AsmjsStoreMem;
      } else {
        UNREACHABLE();
      }
      current_function_builder_->Emit(opcode);
    }

    if (target_var == nullptr && target_prop == nullptr) {
      UNREACHABLE();  // invalid assignment.
    }
  }

  void VisitAssignment(Assignment* expr) {
    bool as_init = false;
    if (scope_ == kModuleScope) {
      // Skip extra assignment inserted by the parser when in this form:
      // (function Module(a, b, c) {... })
      if (expr->target()->IsVariableProxy() &&
          expr->target()->AsVariableProxy()->var()->mode() == CONST_LEGACY) {
        return;
      }
      Property* prop = expr->value()->AsProperty();
      if (prop != nullptr) {
        VariableProxy* vp = prop->obj()->AsVariableProxy();
        if (vp != nullptr && vp->var()->IsParameter() &&
            vp->var()->index() == 1) {
          VariableProxy* target = expr->target()->AsVariableProxy();
          if (typer_->TypeOf(target)->AsFFIType() != nullptr) {
            const AstRawString* name =
                prop->key()->AsLiteral()->AsRawPropertyName();
            imported_function_table_.AddImport(
                target->var(), reinterpret_cast<const char*>(name->raw_data()),
                name->length());
          }
        }
        // Property values in module scope don't emit code, so return.
        return;
      }
      ArrayLiteral* funcs = expr->value()->AsArrayLiteral();
      if (funcs != nullptr &&
          typer_->TypeOf(funcs)
              ->AsFunctionTableType()
              ->signature()
              ->AsFunctionType()) {
        VariableProxy* target = expr->target()->AsVariableProxy();
        DCHECK_NOT_NULL(target);
        AddFunctionTable(target, funcs);
        // Only add to the function table. No init needed.
        return;
      }
      if (expr->value()->IsCallNew()) {
        // No init code to emit for CallNew nodes.
        return;
      }
      as_init = true;
    }

    if (as_init) LoadInitFunction();
    MachineType mtype;
    bool is_nop = false;
    EmitAssignmentLhs(expr->target(), &mtype);
    EmitAssignmentRhs(expr->target(), expr->value(), &is_nop);
    if (!is_nop) {
      EmitAssignment(expr, mtype);
    }
    if (as_init) UnLoadInitFunction();
  }

  void VisitYield(Yield* expr) { UNREACHABLE(); }

  void VisitThrow(Throw* expr) { UNREACHABLE(); }

  void VisitForeignVariable(bool is_float, Variable* var, Property* expr) {
    DCHECK(expr->obj()->AsVariableProxy());
    DCHECK(VariableLocation::PARAMETER ==
           expr->obj()->AsVariableProxy()->var()->location());
    DCHECK_EQ(1, expr->obj()->AsVariableProxy()->var()->index());
    Literal* key_literal = expr->key()->AsLiteral();
    DCHECK_NOT_NULL(key_literal);
    if (!key_literal->value().is_null()) {
      Handle<Name> name =
          i::Object::ToName(isolate_, key_literal->value()).ToHandleChecked();
      LocalType type = is_float ? kAstF64 : kAstI32;
      foreign_variables_.push_back({name, var, type});
    }
  }

  void VisitPropertyAndEmitIndex(Property* expr, MachineType* mtype) {
    Expression* obj = expr->obj();
    AsmType* type = typer_->TypeOf(obj);
    int size;
    if (type->IsA(AsmType::Uint8Array())) {
      *mtype = MachineType::Uint8();
      size = 1;
    } else if (type->IsA(AsmType::Int8Array())) {
      *mtype = MachineType::Int8();
      size = 1;
    } else if (type->IsA(AsmType::Uint16Array())) {
      *mtype = MachineType::Uint16();
      size = 2;
    } else if (type->IsA(AsmType::Int16Array())) {
      *mtype = MachineType::Int16();
      size = 2;
    } else if (type->IsA(AsmType::Uint32Array())) {
      *mtype = MachineType::Uint32();
      size = 4;
    } else if (type->IsA(AsmType::Int32Array())) {
      *mtype = MachineType::Int32();
      size = 4;
    } else if (type->IsA(AsmType::Uint32Array())) {
      *mtype = MachineType::Uint32();
      size = 4;
    } else if (type->IsA(AsmType::Float32Array())) {
      *mtype = MachineType::Float32();
      size = 4;
    } else if (type->IsA(AsmType::Float64Array())) {
      *mtype = MachineType::Float64();
      size = 8;
    } else {
      UNREACHABLE();
    }
    if (size == 1) {
      // Allow more general expression in byte arrays than the spec
      // strictly permits.
      // Early versions of Emscripten emit HEAP8[HEAP32[..]|0] in
      // places that strictly should be HEAP8[HEAP32[..]>>0].
      RECURSE(Visit(expr->key()));
      return;
    }

    Literal* value = expr->key()->AsLiteral();
    if (value) {
      DCHECK(value->raw_value()->IsNumber());
      DCHECK_EQ(kAstI32, TypeOf(value));
      int32_t val = static_cast<int32_t>(value->raw_value()->AsNumber());
      // TODO(titzer): handle overflow here.
      current_function_builder_->EmitI32Const(val * size);
      return;
    }
    BinaryOperation* binop = expr->key()->AsBinaryOperation();
    if (binop) {
      DCHECK_EQ(Token::SAR, binop->op());
      DCHECK(binop->right()->AsLiteral()->raw_value()->IsNumber());
      DCHECK(kAstI32 == TypeOf(binop->right()->AsLiteral()));
      DCHECK_EQ(size,
                1 << static_cast<int>(
                    binop->right()->AsLiteral()->raw_value()->AsNumber()));
      // Mask bottom bits to match asm.js behavior.
      byte mask = static_cast<byte>(~(size - 1));
      RECURSE(Visit(binop->left()));
      current_function_builder_->EmitWithU8(kExprI8Const, mask);
      current_function_builder_->Emit(kExprI32And);
      return;
    }
    UNREACHABLE();
  }

  void VisitProperty(Property* expr) {
    MachineType type;
    VisitPropertyAndEmitIndex(expr, &type);
    WasmOpcode opcode;
    if (type == MachineType::Int8()) {
      opcode = kExprI32AsmjsLoadMem8S;
    } else if (type == MachineType::Uint8()) {
      opcode = kExprI32AsmjsLoadMem8U;
    } else if (type == MachineType::Int16()) {
      opcode = kExprI32AsmjsLoadMem16S;
    } else if (type == MachineType::Uint16()) {
      opcode = kExprI32AsmjsLoadMem16U;
    } else if (type == MachineType::Int32()) {
      opcode = kExprI32AsmjsLoadMem;
    } else if (type == MachineType::Uint32()) {
      opcode = kExprI32AsmjsLoadMem;
    } else if (type == MachineType::Float32()) {
      opcode = kExprF32AsmjsLoadMem;
    } else if (type == MachineType::Float64()) {
      opcode = kExprF64AsmjsLoadMem;
    } else {
      UNREACHABLE();
    }

    current_function_builder_->Emit(opcode);
  }

  bool VisitStdlibFunction(Call* call, VariableProxy* expr) {
    Variable* var = expr->var();
    AsmTyper::StandardMember standard_object =
        typer_->VariableAsStandardMember(var);
    ZoneList<Expression*>* args = call->arguments();
    LocalType call_type = TypeOf(call);

    switch (standard_object) {
      case AsmTyper::kNone: {
        return false;
      }
      case AsmTyper::kMathAcos: {
        VisitCallArgs(call);
        DCHECK_EQ(kAstF64, call_type);
        current_function_builder_->Emit(kExprF64Acos);
        break;
      }
      case AsmTyper::kMathAsin: {
        VisitCallArgs(call);
        DCHECK_EQ(kAstF64, call_type);
        current_function_builder_->Emit(kExprF64Asin);
        break;
      }
      case AsmTyper::kMathAtan: {
        VisitCallArgs(call);
        DCHECK_EQ(kAstF64, call_type);
        current_function_builder_->Emit(kExprF64Atan);
        break;
      }
      case AsmTyper::kMathCos: {
        VisitCallArgs(call);
        DCHECK_EQ(kAstF64, call_type);
        current_function_builder_->Emit(kExprF64Cos);
        break;
      }
      case AsmTyper::kMathSin: {
        VisitCallArgs(call);
        DCHECK_EQ(kAstF64, call_type);
        current_function_builder_->Emit(kExprF64Sin);
        break;
      }
      case AsmTyper::kMathTan: {
        VisitCallArgs(call);
        DCHECK_EQ(kAstF64, call_type);
        current_function_builder_->Emit(kExprF64Tan);
        break;
      }
      case AsmTyper::kMathExp: {
        VisitCallArgs(call);
        DCHECK_EQ(kAstF64, call_type);
        current_function_builder_->Emit(kExprF64Exp);
        break;
      }
      case AsmTyper::kMathLog: {
        VisitCallArgs(call);
        DCHECK_EQ(kAstF64, call_type);
        current_function_builder_->Emit(kExprF64Log);
        break;
      }
      case AsmTyper::kMathCeil: {
        VisitCallArgs(call);
        if (call_type == kAstF32) {
          current_function_builder_->Emit(kExprF32Ceil);
        } else if (call_type == kAstF64) {
          current_function_builder_->Emit(kExprF64Ceil);
        } else {
          UNREACHABLE();
        }
        break;
      }
      case AsmTyper::kMathFloor: {
        VisitCallArgs(call);
        if (call_type == kAstF32) {
          current_function_builder_->Emit(kExprF32Floor);
        } else if (call_type == kAstF64) {
          current_function_builder_->Emit(kExprF64Floor);
        } else {
          UNREACHABLE();
        }
        break;
      }
      case AsmTyper::kMathSqrt: {
        VisitCallArgs(call);
        if (call_type == kAstF32) {
          current_function_builder_->Emit(kExprF32Sqrt);
        } else if (call_type == kAstF64) {
          current_function_builder_->Emit(kExprF64Sqrt);
        } else {
          UNREACHABLE();
        }
        break;
      }
      case AsmTyper::kMathClz32: {
        VisitCallArgs(call);
        DCHECK(call_type == kAstI32);
        current_function_builder_->Emit(kExprI32Clz);
        break;
      }
      case AsmTyper::kMathAbs: {
        if (call_type == kAstI32) {
          uint32_t tmp = current_function_builder_->AddLocal(kAstI32);

          // if set_local(tmp, x) < 0
          Visit(call->arguments()->at(0));
          current_function_builder_->EmitSetLocal(tmp);
          byte code[] = {WASM_I8(0)};
          current_function_builder_->EmitCode(code, sizeof(code));
          current_function_builder_->Emit(kExprI32LtS);
          current_function_builder_->Emit(kExprIf);

          // then (0 - tmp)
          current_function_builder_->EmitCode(code, sizeof(code));
          current_function_builder_->EmitGetLocal(tmp);
          current_function_builder_->Emit(kExprI32Sub);

          // else tmp
          current_function_builder_->Emit(kExprElse);
          current_function_builder_->EmitGetLocal(tmp);
          // end
          current_function_builder_->Emit(kExprEnd);

        } else if (call_type == kAstF32) {
          VisitCallArgs(call);
          current_function_builder_->Emit(kExprF32Abs);
        } else if (call_type == kAstF64) {
          VisitCallArgs(call);
          current_function_builder_->Emit(kExprF64Abs);
        } else {
          UNREACHABLE();
        }
        break;
      }
      case AsmTyper::kMathMin: {
        // TODO(bradnelson): Change wasm to match Math.min in asm.js mode.
        if (call_type == kAstI32) {
          uint32_t tmp_x = current_function_builder_->AddLocal(kAstI32);
          uint32_t tmp_y = current_function_builder_->AddLocal(kAstI32);

          // if set_local(tmp_x, x) < set_local(tmp_y, y)
          Visit(call->arguments()->at(0));
          current_function_builder_->EmitSetLocal(tmp_x);

          Visit(call->arguments()->at(1));
          current_function_builder_->EmitSetLocal(tmp_y);

          current_function_builder_->Emit(kExprI32LeS);
          current_function_builder_->Emit(kExprIf);

          // then tmp_x
          current_function_builder_->EmitGetLocal(tmp_x);

          // else tmp_y
          current_function_builder_->Emit(kExprElse);
          current_function_builder_->EmitGetLocal(tmp_y);
          current_function_builder_->Emit(kExprEnd);

        } else if (call_type == kAstF32) {
          VisitCallArgs(call);
          current_function_builder_->Emit(kExprF32Min);
        } else if (call_type == kAstF64) {
          VisitCallArgs(call);
          current_function_builder_->Emit(kExprF64Min);
        } else {
          UNREACHABLE();
        }
        break;
      }
      case AsmTyper::kMathMax: {
        // TODO(bradnelson): Change wasm to match Math.max in asm.js mode.
        if (call_type == kAstI32) {
          uint32_t tmp_x = current_function_builder_->AddLocal(kAstI32);
          uint32_t tmp_y = current_function_builder_->AddLocal(kAstI32);

          // if set_local(tmp_x, x) < set_local(tmp_y, y)
          Visit(call->arguments()->at(0));

          current_function_builder_->EmitSetLocal(tmp_x);

          Visit(call->arguments()->at(1));
          current_function_builder_->EmitSetLocal(tmp_y);

          current_function_builder_->Emit(kExprI32LeS);
          current_function_builder_->Emit(kExprIf);

          // then tmp_y
          current_function_builder_->EmitGetLocal(tmp_y);

          // else tmp_x
          current_function_builder_->Emit(kExprElse);
          current_function_builder_->EmitGetLocal(tmp_x);
          current_function_builder_->Emit(kExprEnd);

        } else if (call_type == kAstF32) {
          VisitCallArgs(call);
          current_function_builder_->Emit(kExprF32Max);
        } else if (call_type == kAstF64) {
          VisitCallArgs(call);
          current_function_builder_->Emit(kExprF64Max);
        } else {
          UNREACHABLE();
        }
        break;
      }
      case AsmTyper::kMathAtan2: {
        VisitCallArgs(call);
        DCHECK_EQ(kAstF64, call_type);
        current_function_builder_->Emit(kExprF64Atan2);
        break;
      }
      case AsmTyper::kMathPow: {
        VisitCallArgs(call);
        DCHECK_EQ(kAstF64, call_type);
        current_function_builder_->Emit(kExprF64Pow);
        break;
      }
      case AsmTyper::kMathImul: {
        VisitCallArgs(call);
        current_function_builder_->Emit(kExprI32Mul);
        break;
      }
      case AsmTyper::kMathFround: {
        DCHECK(args->length() == 1);
        Literal* literal = args->at(0)->AsLiteral();
        if (literal != nullptr) {
          // constant fold Math.fround(#const);
          if (literal->raw_value()->IsNumber()) {
            float val = static_cast<float>(literal->raw_value()->AsNumber());
            byte code[] = {WASM_F32(val)};
            current_function_builder_->EmitCode(code, sizeof(code));
            return true;
          }
        }
        VisitCallArgs(call);
        static const bool kDontIgnoreSign = false;
        switch (TypeIndexOf(args->at(0), kDontIgnoreSign)) {
          case kInt32:
          case kFixnum:
            current_function_builder_->Emit(kExprF32SConvertI32);
            break;
          case kUint32:
            current_function_builder_->Emit(kExprF32UConvertI32);
            break;
          case kFloat32:
            break;
          case kFloat64:
            current_function_builder_->Emit(kExprF32ConvertF64);
            break;
          default:
            UNREACHABLE();
        }
        break;
      }
      default: {
        UNREACHABLE();
        break;
      }
    }
    return true;
  }

  void VisitCallArgs(Call* expr) {
    ZoneList<Expression*>* args = expr->arguments();
    for (int i = 0; i < args->length(); ++i) {
      Expression* arg = args->at(i);
      RECURSE(Visit(arg));
    }
  }

  void VisitCall(Call* expr) {
    Call::CallType call_type = expr->GetCallType();
    switch (call_type) {
      case Call::OTHER_CALL: {
        DCHECK_EQ(kFuncScope, scope_);
        VariableProxy* proxy = expr->expression()->AsVariableProxy();
        if (proxy != nullptr) {
          if (VisitStdlibFunction(expr, proxy)) {
            return;
          }
        }
        uint32_t index;
        VariableProxy* vp = expr->expression()->AsVariableProxy();
        DCHECK_NOT_NULL(vp);
        if (typer_->TypeOf(vp)->AsFFIType() != nullptr) {
          LocalType return_type = TypeOf(expr);
          ZoneList<Expression*>* args = expr->arguments();
          FunctionSig::Builder sig(zone(), return_type == kAstStmt ? 0 : 1,
                                   args->length());
          if (return_type != kAstStmt) {
            sig.AddReturn(return_type);
          }
          for (int i = 0; i < args->length(); ++i) {
            sig.AddParam(TypeOf(args->at(i)));
          }
          index =
              imported_function_table_.GetFunctionIndex(vp->var(), sig.Build());
          VisitCallArgs(expr);
          current_function_builder_->Emit(kExprCallImport);
          current_function_builder_->EmitVarInt(expr->arguments()->length());
          current_function_builder_->EmitVarInt(index);
        } else {
          index = LookupOrInsertFunction(vp->var());
          VisitCallArgs(expr);
          current_function_builder_->Emit(kExprCallFunction);
          current_function_builder_->EmitVarInt(expr->arguments()->length());
          current_function_builder_->EmitVarInt(index);
        }
        break;
      }
      case Call::KEYED_PROPERTY_CALL: {
        DCHECK_EQ(kFuncScope, scope_);
        Property* p = expr->expression()->AsProperty();
        DCHECK_NOT_NULL(p);
        VariableProxy* var = p->obj()->AsVariableProxy();
        DCHECK_NOT_NULL(var);
        FunctionTableIndices* indices = LookupFunctionTable(var->var());
        RECURSE(Visit(p->key()));
        current_function_builder_->EmitI32Const(indices->start_index);
        current_function_builder_->Emit(kExprI32Add);
        VisitCallArgs(expr);
        current_function_builder_->Emit(kExprCallIndirect);
        current_function_builder_->EmitVarInt(expr->arguments()->length());
        current_function_builder_->EmitVarInt(indices->signature_index);
        break;
      }
      default:
        UNREACHABLE();
    }
  }

  void VisitCallNew(CallNew* expr) { UNREACHABLE(); }

  void VisitCallRuntime(CallRuntime* expr) { UNREACHABLE(); }

  void VisitUnaryOperation(UnaryOperation* expr) {
    RECURSE(Visit(expr->expression()));
    switch (expr->op()) {
      case Token::NOT: {
        DCHECK_EQ(kAstI32, TypeOf(expr->expression()));
        current_function_builder_->Emit(kExprI32Eqz);
        break;
      }
      default:
        UNREACHABLE();
    }
  }

  void VisitCountOperation(CountOperation* expr) { UNREACHABLE(); }

  bool MatchIntBinaryOperation(BinaryOperation* expr, Token::Value op,
                               int32_t val) {
    DCHECK_NOT_NULL(expr->right());
    if (expr->op() == op && expr->right()->IsLiteral() &&
        TypeOf(expr) == kAstI32) {
      Literal* right = expr->right()->AsLiteral();
      DCHECK(right->raw_value()->IsNumber());
      if (static_cast<int32_t>(right->raw_value()->AsNumber()) == val) {
        return true;
      }
    }
    return false;
  }

  bool MatchDoubleBinaryOperation(BinaryOperation* expr, Token::Value op,
                                  double val) {
    DCHECK_NOT_NULL(expr->right());
    if (expr->op() == op && expr->right()->IsLiteral() &&
        TypeOf(expr) == kAstF64) {
      Literal* right = expr->right()->AsLiteral();
      DCHECK(right->raw_value()->IsNumber());
      if (right->raw_value()->AsNumber() == val) {
        return true;
      }
    }
    return false;
  }

  enum ConvertOperation { kNone, kAsIs, kToInt, kToDouble };

  ConvertOperation MatchOr(BinaryOperation* expr) {
    if (MatchIntBinaryOperation(expr, Token::BIT_OR, 0) &&
        (TypeOf(expr->left()) == kAstI32)) {
      return kAsIs;
    } else {
      return kNone;
    }
  }

  ConvertOperation MatchShr(BinaryOperation* expr) {
    if (MatchIntBinaryOperation(expr, Token::SHR, 0)) {
      // TODO(titzer): this probably needs to be kToUint
      return (TypeOf(expr->left()) == kAstI32) ? kAsIs : kToInt;
    } else {
      return kNone;
    }
  }

  ConvertOperation MatchXor(BinaryOperation* expr) {
    if (MatchIntBinaryOperation(expr, Token::BIT_XOR, 0xffffffff)) {
      DCHECK_EQ(kAstI32, TypeOf(expr->left()));
      DCHECK_EQ(kAstI32, TypeOf(expr->right()));
      BinaryOperation* op = expr->left()->AsBinaryOperation();
      if (op != nullptr) {
        if (MatchIntBinaryOperation(op, Token::BIT_XOR, 0xffffffff)) {
          DCHECK_EQ(kAstI32, TypeOf(op->right()));
          if (TypeOf(op->left()) != kAstI32) {
            return kToInt;
          } else {
            return kAsIs;
          }
        }
      }
    }
    return kNone;
  }

  ConvertOperation MatchMul(BinaryOperation* expr) {
    if (MatchDoubleBinaryOperation(expr, Token::MUL, 1.0)) {
      DCHECK_EQ(kAstF64, TypeOf(expr->right()));
      if (TypeOf(expr->left()) != kAstF64) {
        return kToDouble;
      } else {
        return kAsIs;
      }
    } else {
      return kNone;
    }
  }

  ConvertOperation MatchBinaryOperation(BinaryOperation* expr) {
    switch (expr->op()) {
      case Token::BIT_OR:
        return MatchOr(expr);
      case Token::SHR:
        return MatchShr(expr);
      case Token::BIT_XOR:
        return MatchXor(expr);
      case Token::MUL:
        return MatchMul(expr);
      default:
        return kNone;
    }
  }

// Work around Mul + Div being defined in PPC assembler.
#ifdef Mul
#undef Mul
#endif

#define NON_SIGNED_BINOP(op)      \
  static WasmOpcode opcodes[] = { \
    kExprI32##op,                 \
    kExprI32##op,                 \
    kExprF32##op,                 \
    kExprF64##op                  \
  }

#define SIGNED_BINOP(op)          \
  static WasmOpcode opcodes[] = { \
    kExprI32##op##S,              \
    kExprI32##op##U,              \
    kExprF32##op,                 \
    kExprF64##op                  \
  }

#define NON_SIGNED_INT_BINOP(op) \
  static WasmOpcode opcodes[] = { kExprI32##op, kExprI32##op }

#define BINOP_CASE(token, op, V, ignore_sign)                         \
  case token: {                                                       \
    V(op);                                                            \
    int type = TypeIndexOf(expr->left(), expr->right(), ignore_sign); \
    current_function_builder_->Emit(opcodes[type]);                   \
    break;                                                            \
  }

  Expression* GetLeft(BinaryOperation* expr) {
    if (expr->op() == Token::BIT_XOR) {
      return expr->left()->AsBinaryOperation()->left();
    } else {
      return expr->left();
    }
  }

  void VisitBinaryOperation(BinaryOperation* expr) {
    ConvertOperation convertOperation = MatchBinaryOperation(expr);
    static const bool kDontIgnoreSign = false;
    if (convertOperation == kToDouble) {
      RECURSE(Visit(expr->left()));
      TypeIndex type = TypeIndexOf(expr->left(), kDontIgnoreSign);
      if (type == kInt32 || type == kFixnum) {
        current_function_builder_->Emit(kExprF64SConvertI32);
      } else if (type == kUint32) {
        current_function_builder_->Emit(kExprF64UConvertI32);
      } else if (type == kFloat32) {
        current_function_builder_->Emit(kExprF64ConvertF32);
      } else {
        UNREACHABLE();
      }
    } else if (convertOperation == kToInt) {
      RECURSE(Visit(GetLeft(expr)));
      TypeIndex type = TypeIndexOf(GetLeft(expr), kDontIgnoreSign);
      if (type == kFloat32) {
        current_function_builder_->Emit(kExprI32AsmjsSConvertF32);
      } else if (type == kFloat64) {
        current_function_builder_->Emit(kExprI32AsmjsSConvertF64);
      } else {
        UNREACHABLE();
      }
    } else if (convertOperation == kAsIs) {
      RECURSE(Visit(GetLeft(expr)));
    } else {
      if (expr->op() == Token::COMMA) {
        current_function_builder_->Emit(kExprBlock);
      }

      RECURSE(Visit(expr->left()));
      RECURSE(Visit(expr->right()));

      if (expr->op() == Token::COMMA) {
        current_function_builder_->Emit(kExprEnd);
      }

      switch (expr->op()) {
        BINOP_CASE(Token::ADD, Add, NON_SIGNED_BINOP, true);
        BINOP_CASE(Token::SUB, Sub, NON_SIGNED_BINOP, true);
        BINOP_CASE(Token::MUL, Mul, NON_SIGNED_BINOP, true);
        BINOP_CASE(Token::BIT_OR, Ior, NON_SIGNED_INT_BINOP, true);
        BINOP_CASE(Token::BIT_AND, And, NON_SIGNED_INT_BINOP, true);
        BINOP_CASE(Token::BIT_XOR, Xor, NON_SIGNED_INT_BINOP, true);
        BINOP_CASE(Token::SHL, Shl, NON_SIGNED_INT_BINOP, true);
        BINOP_CASE(Token::SAR, ShrS, NON_SIGNED_INT_BINOP, true);
        BINOP_CASE(Token::SHR, ShrU, NON_SIGNED_INT_BINOP, true);
        case Token::DIV: {
          static WasmOpcode opcodes[] = {kExprI32AsmjsDivS, kExprI32AsmjsDivU,
                                         kExprF32Div, kExprF64Div};
          int type = TypeIndexOf(expr->left(), expr->right(), false);
          current_function_builder_->Emit(opcodes[type]);
          break;
        }
        case Token::MOD: {
          TypeIndex type = TypeIndexOf(expr->left(), expr->right(), false);
          if (type == kInt32) {
            current_function_builder_->Emit(kExprI32AsmjsRemS);
          } else if (type == kUint32) {
            current_function_builder_->Emit(kExprI32AsmjsRemU);
          } else if (type == kFloat64) {
            current_function_builder_->Emit(kExprF64Mod);
            return;
          } else {
            UNREACHABLE();
          }
          break;
        }
        case Token::COMMA: {
          break;
        }
        default:
          UNREACHABLE();
      }
    }
  }

  void VisitCompareOperation(CompareOperation* expr) {
    RECURSE(Visit(expr->left()));
    RECURSE(Visit(expr->right()));
    switch (expr->op()) {
      BINOP_CASE(Token::EQ, Eq, NON_SIGNED_BINOP, false);
      BINOP_CASE(Token::LT, Lt, SIGNED_BINOP, false);
      BINOP_CASE(Token::LTE, Le, SIGNED_BINOP, false);
      BINOP_CASE(Token::GT, Gt, SIGNED_BINOP, false);
      BINOP_CASE(Token::GTE, Ge, SIGNED_BINOP, false);
      default:
        UNREACHABLE();
    }
  }

#undef BINOP_CASE
#undef NON_SIGNED_INT_BINOP
#undef SIGNED_BINOP
#undef NON_SIGNED_BINOP

  enum TypeIndex {
    kInt32 = 0,
    kUint32 = 1,
    kFloat32 = 2,
    kFloat64 = 3,
    kFixnum = 4
  };

  TypeIndex TypeIndexOf(Expression* left, Expression* right, bool ignore_sign) {
    TypeIndex left_index = TypeIndexOf(left, ignore_sign);
    TypeIndex right_index = TypeIndexOf(right, ignore_sign);
    if (left_index == kFixnum) {
      left_index = right_index;
    }
    if (right_index == kFixnum) {
      right_index = left_index;
    }
    if (left_index == kFixnum && right_index == kFixnum) {
      left_index = kInt32;
      right_index = kInt32;
    }
    if (left_index != right_index) {
      DCHECK(ignore_sign && (left_index <= 1) && (right_index <= 1));
    }
    return left_index;
  }

  TypeIndex TypeIndexOf(Expression* expr, bool ignore_sign) {
    AsmType* type = typer_->TypeOf(expr);
    if (type->IsA(AsmType::FixNum())) {
      return kFixnum;
    }

    if (type->IsA(AsmType::Signed())) {
      return kInt32;
    }

    if (type->IsA(AsmType::Unsigned())) {
      return kUint32;
    }

    if (type->IsA(AsmType::Intish())) {
      if (!ignore_sign) {
        // TODO(jpp): log a warning and move on.
      }
      return kInt32;
    }

    if (type->IsA(AsmType::Floatish())) {
      return kFloat32;
    }

    if (type->IsA(AsmType::DoubleQ())) {
      return kFloat64;
    }

    UNREACHABLE();
    return kInt32;
  }

#undef CASE
#undef NON_SIGNED_INT
#undef SIGNED
#undef NON_SIGNED

  void VisitThisFunction(ThisFunction* expr) { UNREACHABLE(); }

  void VisitDeclarations(ZoneList<Declaration*>* decls) {
    for (int i = 0; i < decls->length(); ++i) {
      Declaration* decl = decls->at(i);
      RECURSE(Visit(decl));
    }
  }

  void VisitClassLiteral(ClassLiteral* expr) { UNREACHABLE(); }

  void VisitSpread(Spread* expr) { UNREACHABLE(); }

  void VisitSuperPropertyReference(SuperPropertyReference* expr) {
    UNREACHABLE();
  }

  void VisitSuperCallReference(SuperCallReference* expr) { UNREACHABLE(); }

  void VisitSloppyBlockFunctionStatement(SloppyBlockFunctionStatement* expr) {
    UNREACHABLE();
  }

  void VisitDoExpression(DoExpression* expr) { UNREACHABLE(); }

  void VisitRewritableExpression(RewritableExpression* expr) { UNREACHABLE(); }

  struct IndexContainer : public ZoneObject {
    uint32_t index;
  };

  uint32_t LookupOrInsertLocal(Variable* v, LocalType type) {
    DCHECK_NOT_NULL(current_function_builder_);
    ZoneHashMap::Entry* entry =
        local_variables_.Lookup(v, ComputePointerHash(v));
    if (entry == nullptr) {
      uint32_t index;
      DCHECK(!v->IsParameter());
      index = current_function_builder_->AddLocal(type);
      IndexContainer* container = new (zone()) IndexContainer();
      container->index = index;
      entry = local_variables_.LookupOrInsert(v, ComputePointerHash(v),
                                              ZoneAllocationPolicy(zone()));
      entry->value = container;
    }
    return (reinterpret_cast<IndexContainer*>(entry->value))->index;
  }

  void InsertParameter(Variable* v, LocalType type, uint32_t index) {
    DCHECK(v->IsParameter());
    DCHECK_NOT_NULL(current_function_builder_);
    ZoneHashMap::Entry* entry =
        local_variables_.Lookup(v, ComputePointerHash(v));
    DCHECK_NULL(entry);
    IndexContainer* container = new (zone()) IndexContainer();
    container->index = index;
    entry = local_variables_.LookupOrInsert(v, ComputePointerHash(v),
                                            ZoneAllocationPolicy(zone()));
    entry->value = container;
  }

  uint32_t LookupOrInsertGlobal(Variable* v, LocalType type) {
    ZoneHashMap::Entry* entry =
        global_variables_.Lookup(v, ComputePointerHash(v));
    if (entry == nullptr) {
      uint32_t index = builder_->AddGlobal(type, 0);
      IndexContainer* container = new (zone()) IndexContainer();
      container->index = index;
      entry = global_variables_.LookupOrInsert(v, ComputePointerHash(v),
                                               ZoneAllocationPolicy(zone()));
      entry->value = container;
    }
    return (reinterpret_cast<IndexContainer*>(entry->value))->index;
  }

  uint32_t LookupOrInsertFunction(Variable* v) {
    DCHECK_NOT_NULL(builder_);
    ZoneHashMap::Entry* entry = functions_.Lookup(v, ComputePointerHash(v));
    if (entry == nullptr) {
      uint32_t index = builder_->AddFunction();
      IndexContainer* container = new (zone()) IndexContainer();
      container->index = index;
      entry = functions_.LookupOrInsert(v, ComputePointerHash(v),
                                        ZoneAllocationPolicy(zone()));
      entry->value = container;
    }
    return (reinterpret_cast<IndexContainer*>(entry->value))->index;
  }

  LocalType TypeOf(Expression* expr) { return TypeFrom(typer_->TypeOf(expr)); }

  LocalType TypeFrom(AsmType* type) {
    if (type->IsA(AsmType::Intish())) {
      return kAstI32;
    }

    if (type->IsA(AsmType::Floatish())) {
      return kAstF32;
    }

    if (type->IsA(AsmType::DoubleQ())) {
      return kAstF64;
    }

    return kAstStmt;
  }

  Zone* zone() { return zone_; }

  ZoneHashMap local_variables_;
  ZoneHashMap functions_;
  ZoneHashMap global_variables_;
  AsmScope scope_;
  WasmModuleBuilder* builder_;
  WasmFunctionBuilder* current_function_builder_;
  FunctionLiteral* literal_;
  Isolate* isolate_;
  Zone* zone_;
  AsmTyper* typer_;
  ZoneVector<std::pair<BreakableStatement*, bool>> breakable_blocks_;
  ZoneVector<ForeignVariable> foreign_variables_;
  uint32_t init_function_index_;
  uint32_t foreign_init_function_index_;
  uint32_t next_table_index_;
  ZoneHashMap function_tables_;
  ImportedFunctionTable imported_function_table_;

  DEFINE_AST_VISITOR_SUBCLASS_MEMBERS();

 private:
  DISALLOW_COPY_AND_ASSIGN(AsmWasmBuilderImpl);
};

AsmWasmBuilder::AsmWasmBuilder(Isolate* isolate, Zone* zone,
                               FunctionLiteral* literal, AsmTyper* typer)
    : isolate_(isolate), zone_(zone), literal_(literal), typer_(typer) {}

// TODO(aseemgarg): probably should take zone (to write wasm to) as input so
// that zone in constructor may be thrown away once wasm module is written.
ZoneBuffer* AsmWasmBuilder::Run(i::Handle<i::FixedArray>* foreign_args) {
  AsmWasmBuilderImpl impl(isolate_, zone_, literal_, typer_);
  impl.Build();
  *foreign_args = impl.GetForeignArgs();
  ZoneBuffer* buffer = new (zone_) ZoneBuffer(zone_);
  impl.builder_->WriteTo(*buffer);
  return buffer;
}
}  // namespace wasm
}  // namespace internal
}  // namespace v8
