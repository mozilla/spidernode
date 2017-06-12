// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/asmjs/asm-scanner.h"
#include "src/objects.h"
#include "src/parsing/scanner-character-streams.h"
#include "src/parsing/scanner.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

#define TOK(t) AsmJsScanner::kToken_##t

class AsmJsScannerTest : public ::testing::Test {
 protected:
  void SetupSource(const char* source) {
    scanner.SetStream(ScannerStream::ForTesting(source));
  }

  void Skip(AsmJsScanner::token_t t) {
    CHECK_EQ(t, scanner.Token());
    scanner.Next();
  }

  void SkipGlobal() {
    CHECK(scanner.IsGlobal());
    scanner.Next();
  }

  void SkipLocal() {
    CHECK(scanner.IsLocal());
    scanner.Next();
  }

  void CheckForEnd() { CHECK(scanner.Token() == AsmJsScanner::kEndOfInput); }

  void CheckForParseError() {
    CHECK(scanner.Token() == AsmJsScanner::kParseError);
  }

  AsmJsScanner scanner;
};

TEST_F(AsmJsScannerTest, SimpleFunction) {
  SetupSource("function foo() { return; }");
  Skip(TOK(function));
  DCHECK_EQ("foo", scanner.GetIdentifierString());
  SkipGlobal();
  Skip('(');
  Skip(')');
  Skip('{');
  // clang-format off
  Skip(TOK(return));
  // clang-format on
  Skip(';');
  Skip('}');
  CheckForEnd();
}

TEST_F(AsmJsScannerTest, JSKeywords) {
  SetupSource(
      "arguments break case const continue\n"
      "default do else eval for function\n"
      "if new return switch var while\n");
  Skip(TOK(arguments));
  Skip(TOK(break));
  Skip(TOK(case));
  Skip(TOK(const));
  Skip(TOK(continue));
  Skip(TOK(default));
  Skip(TOK(do));
  Skip(TOK(else));
  Skip(TOK(eval));
  Skip(TOK(for));
  Skip(TOK(function));
  Skip(TOK(if));
  Skip(TOK(new));
  // clang-format off
  Skip(TOK(return));
  // clang-format on
  Skip(TOK(switch));
  Skip(TOK(var));
  Skip(TOK(while));
  CheckForEnd();
}

TEST_F(AsmJsScannerTest, JSOperatorsSpread) {
  SetupSource(
      "+ - * / % & | ^ ~ << >> >>>\n"
      "< > <= >= == !=\n");
  Skip('+');
  Skip('-');
  Skip('*');
  Skip('/');
  Skip('%');
  Skip('&');
  Skip('|');
  Skip('^');
  Skip('~');
  Skip(TOK(SHL));
  Skip(TOK(SAR));
  Skip(TOK(SHR));
  Skip('<');
  Skip('>');
  Skip(TOK(LE));
  Skip(TOK(GE));
  Skip(TOK(EQ));
  Skip(TOK(NE));
  CheckForEnd();
}

TEST_F(AsmJsScannerTest, JSOperatorsTight) {
  SetupSource(
      "+-*/%&|^~<<>> >>>\n"
      "<><=>= ==!=\n");
  Skip('+');
  Skip('-');
  Skip('*');
  Skip('/');
  Skip('%');
  Skip('&');
  Skip('|');
  Skip('^');
  Skip('~');
  Skip(TOK(SHL));
  Skip(TOK(SAR));
  Skip(TOK(SHR));
  Skip('<');
  Skip('>');
  Skip(TOK(LE));
  Skip(TOK(GE));
  Skip(TOK(EQ));
  Skip(TOK(NE));
  CheckForEnd();
}

TEST_F(AsmJsScannerTest, UsesOfAsm) {
  SetupSource("'use asm' \"use asm\"\n");
  Skip(TOK(UseAsm));
  Skip(TOK(UseAsm));
  CheckForEnd();
}

TEST_F(AsmJsScannerTest, DefaultGlobalScope) {
  SetupSource("var x = x + x;");
  Skip(TOK(var));
  CHECK_EQ("x", scanner.GetIdentifierString());
  AsmJsScanner::token_t x = scanner.Token();
  SkipGlobal();
  Skip('=');
  Skip(x);
  Skip('+');
  Skip(x);
  Skip(';');
  CheckForEnd();
}

TEST_F(AsmJsScannerTest, GlobalScope) {
  SetupSource("var x = x + x;");
  scanner.EnterGlobalScope();
  Skip(TOK(var));
  CHECK_EQ("x", scanner.GetIdentifierString());
  AsmJsScanner::token_t x = scanner.Token();
  SkipGlobal();
  Skip('=');
  Skip(x);
  Skip('+');
  Skip(x);
  Skip(';');
  CheckForEnd();
}

TEST_F(AsmJsScannerTest, LocalScope) {
  SetupSource("var x = x + x;");
  scanner.EnterLocalScope();
  Skip(TOK(var));
  CHECK_EQ("x", scanner.GetIdentifierString());
  AsmJsScanner::token_t x = scanner.Token();
  SkipLocal();
  Skip('=');
  Skip(x);
  Skip('+');
  Skip(x);
  Skip(';');
  CheckForEnd();
}

TEST_F(AsmJsScannerTest, Numbers) {
  SetupSource("1 1.2 0x1f 1.e3");

  CHECK(scanner.IsUnsigned());
  CHECK_EQ(1, scanner.AsUnsigned());
  scanner.Next();

  CHECK(scanner.IsDouble());
  CHECK_EQ(1.2, scanner.AsDouble());
  scanner.Next();

  CHECK(scanner.IsUnsigned());
  CHECK_EQ(31, scanner.AsUnsigned());
  scanner.Next();

  CHECK(scanner.IsDouble());
  CHECK_EQ(1.0e3, scanner.AsDouble());
  scanner.Next();

  CheckForEnd();
}

TEST_F(AsmJsScannerTest, BadNumber) {
  SetupSource(".123fe");
  Skip('.');
  CheckForParseError();
}

TEST_F(AsmJsScannerTest, Rewind1) {
  SetupSource("+ - * /");
  Skip('+');
  scanner.Rewind();
  Skip('+');
  Skip('-');
  scanner.Rewind();
  Skip('-');
  Skip('*');
  scanner.Rewind();
  Skip('*');
  Skip('/');
  scanner.Rewind();
  Skip('/');
  CheckForEnd();
}

TEST_F(AsmJsScannerTest, Comments) {
  SetupSource(
      "var // This is a test /* */ eval\n"
      "var /* test *** test */ eval\n"
      "function /* this */ ^");
  Skip(TOK(var));
  Skip(TOK(var));
  Skip(TOK(eval));
  Skip(TOK(function));
  Skip('^');
  CheckForEnd();
}

TEST_F(AsmJsScannerTest, TrailingCComment) {
  SetupSource("var /* test\n");
  Skip(TOK(var));
  CheckForParseError();
}

TEST_F(AsmJsScannerTest, Seeking) {
  SetupSource("var eval do arguments function break\n");
  Skip(TOK(var));
  int old_pos = scanner.GetPosition();
  Skip(TOK(eval));
  Skip(TOK(do));
  Skip(TOK(arguments));
  scanner.Rewind();
  Skip(TOK(arguments));
  scanner.Rewind();
  scanner.Seek(old_pos);
  Skip(TOK(do));
  Skip(TOK(arguments));
  Skip(TOK(function));
  Skip(TOK(break));
  CheckForEnd();
}

TEST_F(AsmJsScannerTest, Newlines) {
  SetupSource(
      "var x = 1\n"
      "var y = 2\n");
  Skip(TOK(var));
  scanner.Next();
  Skip('=');
  scanner.Next();
  CHECK(scanner.IsPrecededByNewline());
  Skip(TOK(var));
  scanner.Next();
  Skip('=');
  scanner.Next();
  CHECK(scanner.IsPrecededByNewline());
  CheckForEnd();
}

}  // namespace internal
}  // namespace v8
