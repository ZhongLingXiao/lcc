/***********************************
 * File:     Parser.cc
 *
 * Author:   蔡鹏
 *
 * Email:    iiicp@outlook.com
 *
 * Date:     2022/10/17
 ***********************************/

#include "Parser.h"

namespace lcc::parser {

namespace {
auto TokenTypeToTypeKind = [](lexer::TokenType tokenType) -> TypeKind {
  switch (tokenType) {
  case lexer::kw_auto:
    return TypeKind::Auto;
  case lexer::kw_char:
    return TypeKind::Char;
  case lexer::kw_short:
    return TypeKind::Short;
  case lexer::kw_int:
    return TypeKind::Int;
  case lexer::kw_long:
    return TypeKind::Long;
  case lexer::kw_float:
    return TypeKind::Float;
  case lexer::kw_double:
    return TypeKind::Double;
  case lexer::kw_signed:
    return TypeKind::Signed;
  case lexer::kw_unsigned:
    return TypeKind::UnSigned;
  default:
    assert(0);
  }
};
}

std::unique_ptr<Program> Parser::Parse() {
  std::vector<std::unique_ptr<Function>> funcs;
  std::vector<std::unique_ptr<GlobalDecl>> decls;
  while (mTokCursor != mTokEnd) {
    if (IsFunction()) {
      funcs.push_back(std::move(ParseFunction()));
    } else {
      decls.push_back(std::move(ParseGlobalDecl()));
    }
  }
  return std::make_unique<Program>(std::move(funcs), std::move(decls));
}
std::unique_ptr<Function> Parser::ParseFunction() {
  assert(IsTypeName());
  auto retType = ParseType();
  assert(Expect(lexer::identifier));
  std::string funcName = std::get<std::string>(mTokCursor->GetTokenValue());
  Consume(lexer::identifier);
  assert(Match(lexer::l_paren));
  std::vector<std::pair<std::unique_ptr<Type>, std::string>> params;
  while (mTokCursor->GetTokenType() != lexer::r_paren) {
    auto ty = ParseType();
    std::string name;
    if (Match(lexer::comma)) {
      name = "";
    } else {
      assert(mTokCursor->GetTokenType() == lexer::identifier);
      name = std::get<std::string>(mTokCursor->GetTokenValue());
      Consume(lexer::identifier);
    }
    params.push_back({std::move(ty), name});
  }
  Consume(lexer::r_paren);
  if (mTokCursor->GetTokenType() == lexer::semi) {
    Consume(lexer::semi);
    return std::make_unique<Function>(std::move(retType), funcName,
                                      std::move(params));
  } else {
    return std::make_unique<Function>(std::move(retType), funcName,
                                      std::move(params), ParseBlockStmt());
  }
}
std::unique_ptr<GlobalDecl> Parser::ParseGlobalDecl() {
  assert(IsTypeName());
  auto ty = ParseType();
  assert(Expect(lexer::identifier));
  std::string varName = std::get<std::string>(mTokCursor->GetTokenValue());
  Consume(lexer::identifier);
  std::unique_ptr<GlobalDecl> globalDecl;
  if (Match(lexer::equal)) {
    globalDecl = std::make_unique<GlobalDecl>(std::move(ty), varName);
  } else {
    globalDecl = std::make_unique<GlobalDecl>(std::move(ty), varName,
                                              ParseConstantExpr());
  }
  assert(Match(lexer::semi));
  return std::move(globalDecl);
}

std::unique_ptr<ConstantExpr> Parser::ParseConstantExpr() {
  switch (mTokCursor->GetTokenType()) {
  case lexer::char_constant: {
    ConstantExpr::ConstantValue val =
        std::get<int32_t>(mTokCursor->GetTokenValue());
    return std::make_unique<ConstantExpr>(val);
  }
  case lexer::string_literal: {
    ConstantExpr::ConstantValue val =
        std::get<std::string>(mTokCursor->GetTokenValue());
    return std::make_unique<ConstantExpr>(val);
  }
  case lexer::numeric_constant: {
    auto val = std::visit(
        [](auto &&value) -> ConstantExpr::ConstantValue {
          using T = std::decay_t<decltype(value)>;
          if constexpr (!std::is_same_v<T, std::monostate>) {
            return value;
          } else {
            return "";
          }
        },
        mTokCursor->GetTokenValue());
    return std::make_unique<ConstantExpr>(val);
  }
  default:
    assert(0);
    break;
  }
}

std::unique_ptr<Type> Parser::ParseType() {
  std::vector<TypeKind> typeKinds;
  while (IsTypeName()) {
    typeKinds.push_back(TokenTypeToTypeKind(mTokCursor->GetTokenType()));
    ++mTokCursor;
  }
  assert(!typeKinds.empty());
  auto baseType = std::make_unique<PrimaryType>(std::move(typeKinds));
  return ParseType(std::move(baseType));
}

std::unique_ptr<Type> Parser::ParseType(std::unique_ptr<Type> &&baseType) {
  if (Match(lexer::star)) {
    return std::make_unique<PointerType>(ParseType(std::move(baseType)));
  } else {
    return std::move(baseType);
  }
}

std::unique_ptr<Stmt> Parser::ParseStmt() {
  std::unique_ptr<Stmt> stmt;
  if (Peek(lexer::kw_if)) {
    stmt = ParseIfStmt();
  }else if (Peek(lexer::kw_do)) {
    stmt = ParseDoWhileStmt();
  }else if (Peek(lexer::kw_while)) {
    stmt = ParseWhileStmt();
  }else if (Peek(lexer::kw_for)) {
    TokIter start = mTokCursor;
    Consume(lexer::kw_for);
    Match(lexer::l_paren);
    if (IsTypeName()) {
      mTokCursor = start;
      stmt = ParseForDeclStmt();
    }else {
      mTokCursor = start;
      stmt = ParseForStmt();
    }
  }else if (IsTypeName()) {
    stmt = ParseDeclStmt();
  }else if (Peek(lexer::kw_break)) {
    stmt = ParseBreakStmt();
  }else if (Peek(lexer::kw_continue)) {
    stmt = ParseContinueStmt();
  }else if (Peek(lexer::kw_return)) {
    stmt = ParseReturnStmt();
  }else if (Peek(lexer::l_brace)) {
    stmt = ParseBlockStmt();
  }else{
    Expect(lexer::identifier);
    stmt = ParseExprStmt();
  }
  return stmt;
}

std::unique_ptr<BlockStmt> Parser::ParseBlockStmt() {
    assert(Match(lexer::l_brace));
    std::vector<std::unique_ptr<Stmt>> stmts;
    while (mTokCursor->GetTokenType() != lexer::r_brace) {
      stmts.push_back(ParseStmt());
    }
    Consume(lexer::r_brace);
    return std::make_unique<BlockStmt>(std::move(stmts));
}

std::unique_ptr<IfStmt> Parser::ParseIfStmt() {
  Consume(lexer::kw_if);
  assert(Match(lexer::l_paren));
  std::unique_ptr<Expr> expr = ParseExpr();
  assert(Match(lexer::r_paren));
  std::unique_ptr<Stmt> thenStmt = ParseStmt();
  if (Match(lexer::kw_else)) {
    return std::make_unique<IfStmt>(std::move(expr), std::move(thenStmt), ParseStmt());
  }else {
    return std::make_unique<IfStmt>(std::move(expr), std::move(thenStmt));
  }
}

std::unique_ptr<WhileStmt> Parser::ParseWhileStmt() {
  Consume(lexer::kw_while);
  assert(Match(lexer::l_paren));
  auto expr = ParseExpr();
  assert(Match(lexer::r_paren));
  auto stmt = ParseStmt();
  return std::make_unique<WhileStmt>(std::move(expr), std::move(stmt));
}

std::unique_ptr<DoWhileStmt> Parser::ParseDoWhileStmt() {
  Consume(lexer::kw_do);
  auto stmt = ParseStmt();
  assert(Match(lexer::kw_while));
  assert(Match(lexer::l_paren));
  auto expr = ParseExpr();
  assert(Match(lexer::r_paren));
  assert(Match(lexer::semi));
  return std::make_unique<DoWhileStmt>(std::move(stmt), std::move(expr));
}

std::unique_ptr<ForStmt> Parser::ParseForStmt() {
  Consume(lexer::kw_for);
  assert(Match(lexer::l_paren));
  std::unique_ptr<Expr> initExpr=nullptr, controlExpr=nullptr, postExpr=nullptr;
  if (!Peek(lexer::semi)) {
    initExpr = ParseExpr();
  }
  assert(Match(lexer::semi));

  if (!Peek(lexer::semi)) {
    controlExpr = ParseExpr();
  }
  assert(Match(lexer::semi));

  if (!Peek(lexer::r_paren)) {
    postExpr = ParseExpr();
  }
  assert(Match(lexer::r_paren));
  auto stmt = ParseStmt();
  return std::make_unique<ForStmt>(std::move(initExpr),
                                   std::move(controlExpr),
                                   std::move(postExpr), std::move(stmt));
}

std::unique_ptr<ForDeclarationStmt> Parser::ParseForDeclStmt() {
  Consume(lexer::kw_for);
  assert(Match(lexer::l_paren));
  assert(IsTypeName());
  std::unique_ptr<Declaration> initDecl = ParseDeclStmt();
  std::unique_ptr<Expr> controlExpr = nullptr, postExpr = nullptr;
  if (!Peek(lexer::semi)) {
    controlExpr = ParseExpr();
  }
  assert(Match(lexer::semi));
  if (!Peek(lexer::r_paren)) {
    postExpr = ParseExpr();
  }
  assert(Match(lexer::r_paren));
  auto stmt = ParseStmt();
  return std::make_unique<ForDeclarationStmt>(std::move(initDecl),
                                   std::move(controlExpr),
                                   std::move(postExpr), std::move(stmt));
}

std::unique_ptr<Declaration> Parser::ParseDeclStmt() {
  assert(IsTypeName());
  auto ty = ParseType();
  Expect(lexer::identifier);
  std::string name = std::get<std::string>(mTokCursor->GetTokenValue());
  if (Peek(lexer::equal)) {
    Consume(lexer::equal);
    auto expr = ParseExpr();
    assert(Match(lexer::semi));
    return std::make_unique<Declaration>(std::move(ty), name, std::move(expr));
  }else {
    assert(Match(lexer::semi));
    return std::make_unique<Declaration>(std::move(ty), name);
  }
}

std::unique_ptr<BreakStmt> Parser::ParseBreakStmt() {
    assert(Match(lexer::kw_break));
    return std::make_unique<BreakStmt>();
}

std::unique_ptr<ContinueStmt> Parser::ParseContinueStmt() {
  assert(Match(lexer::kw_continue));
  return std::make_unique<ContinueStmt>();
}

std::unique_ptr<ReturnStmt> Parser::ParseReturnStmt() {
  assert(Match(lexer::kw_return));
  if (Peek(lexer::semi)) {
    return std::make_unique<ReturnStmt>();
  }

  auto expr = ParseExpr();
  assert(Match(lexer::semi));
  return std::make_unique<ReturnStmt>(std::move(expr));
}

std::unique_ptr<ExprStmt> Parser::ParseExprStmt() {
  if (Peek(lexer::semi)) {
    return std::make_unique<ExprStmt>();
  }
  auto expr = ParseExpr();
  assert(Match(lexer::semi));
  return std::make_unique<ExprStmt>(std::move(expr));
}

std::unique_ptr<Expr> Parser::ParseExpr() {
  auto assign = ParseAssignExpr();
  std::vector<std::unique_ptr<AssignExpr>> assignExps;
  while (Peek(lexer::comma)) {
    Consume(lexer::comma);
    assignExps.push_back(ParseAssignExpr());
  }
  return std::make_unique<Expr>(std::move(assign), std::move(assignExps));
}

std::unique_ptr<AssignExpr> Parser::ParseAssignExpr() {
  auto conditionExpr = ParseConditionalExpr();
  lexer::TokenType tokenType;
  switch (mTokCursor->GetTokenType()) {
  case lexer::equal:
  case lexer::plus_equal:
  case lexer::star_equal:
  case lexer::minus_equal:
  case lexer::slash_equal:
  case lexer::percent_equal:
  case lexer::less_less_equal:
  case lexer::greater_greater_equal:
  case lexer::pipe_equal:
  case lexer::amp_equal:
  case lexer::caret_equal: {
    tokenType = mTokCursor->GetTokenType();
    break;
  }
  default:
    assert(0);
  }
  return std::make_unique<AssignExpr>(std::move(conditionExpr), tokenType, ParseAssignExpr());
}

std::unique_ptr<ConditionalExpr> Parser::ParseConditionalExpr() {
  return nullptr;
}

bool Parser::IsFunction() {
  TokIter start = mTokCursor;
  assert(IsTypeName());

  while (IsTypeName()) {
    ++mTokCursor;
  }
  assert(Match(lexer::identifier));

  bool isFunc = false;
  if (Match(lexer::l_paren)) {
    isFunc = true;
  }
  mTokCursor = start;
  return isFunc;
}

bool Parser::IsTypeName() {
  lexer::TokenType type = mTokCursor->GetTokenType();
  return type == lexer::kw_auto | type == lexer::kw_char |
         type == lexer::kw_short | type == lexer::kw_int |
         type == lexer::kw_long | type == lexer::kw_float |
         type == lexer::kw_double | type == lexer::kw_signed |
         type == lexer::kw_unsigned;
}

bool Parser::Match(lexer::TokenType tokenType) {
  if (mTokCursor->GetTokenType() == tokenType) {
    ++mTokCursor;
    return true;
  }
  return false;
}

bool Parser::Expect(lexer::TokenType tokenType) {
  if (mTokCursor->GetTokenType() == tokenType)
    return true;
  assert(0);
  return false;
}

bool Parser::Consume(lexer::TokenType tokenType) {
  if (mTokCursor->GetTokenType() == tokenType) {
    ++mTokCursor;
    return true;
  } else {
    assert(0);
    return false;
  }
}
bool Parser::Peek(lexer::TokenType tokenType) {
  return mTokCursor->GetTokenType() == tokenType;
}
} // namespace lcc::parser