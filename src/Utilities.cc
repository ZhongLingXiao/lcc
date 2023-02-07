/***********************************
 * File:     Utilities.cc
 *
 * Author:   caipeng
 *
 * Email:    iiicp@outlook.com
 *
 * Date:     2023/1/15
 *
 * Sign:     enjoy life
 ***********************************/

#include "Utilities.h"
#include "llvm/Support/raw_ostream.h"

namespace lcc {
void LOGE(uint32_t row, uint32_t col, const std::string &msg) {
  llvm::errs() << row << ":" << col << ", " << msg << "\n";
  LCC_ASSERT(0);
}

void LOGE(const Token& tok, const std::string &msg) {
  llvm::errs() << tok.getLine() << ":" << tok.getColumn() << ", " << msg << "\n";
  LCC_ASSERT(0);
}


std::string_view getDeclaratorName(const Syntax::Declarator& declarator) {
  auto visitor = overload{
    [](auto&&, const Syntax::DirectDeclaratorIdent& name) -> std::string_view {
        return name.getIdent();
      },
    [](auto&& self, const Syntax::DirectDeclaratorParent& declarator) -> std::string_view {
        return std::visit([&self](auto &&value) -> std::string_view {
          return self(value);
        }, declarator.getDeclarator()->getDirectDeclarator());
      },
    [](auto&& self, const Syntax::DirectDeclaratorParentParamTypeList& paramTypeList) -> std::string_view {
      return std::visit([&self](auto &&value) -> std::string_view {
          return self(value);
        }, *paramTypeList.getDirectDeclarator());
      },
    [](auto&& self, const Syntax::DirectDeclaratorAssignExpr& assignExpr) -> std::string_view {
      return std::visit([&self](auto &&value) -> std::string_view { return self(value); },
      *assignExpr.getDirectDeclarator());
    }};
  return std::visit(YComb{visitor}, declarator.getDirectDeclarator());
}

const Syntax::DirectDeclaratorParentParamTypeList *getFuncDeclarator
    (const Syntax::Declarator &declarator) {
 const Syntax::DirectDeclaratorParentParamTypeList * paramTypeList_ = nullptr;
  auto visitor = overload{
      [](auto&&, const Syntax::DirectDeclaratorIdent& name) -> std::string_view {
        return name.getIdent();
      },
      [](auto&& self, const Syntax::DirectDeclaratorParent& declarator) -> std::string_view {
        return std::visit([&self](auto &&value) -> std::string_view {
          return self(value);
        }, declarator.getDeclarator()->getDirectDeclarator());
      },
      [&paramTypeList_](auto&& self, const Syntax::DirectDeclaratorParentParamTypeList& paramTypeList) -> std::string_view {
        paramTypeList_ = &paramTypeList;
        return std::visit([&self](auto &&value) -> std::string_view {
          return self(value);
        }, *paramTypeList.getDirectDeclarator());
      },
      [](auto&& self, const Syntax::DirectDeclaratorAssignExpr& assignExpr) -> std::string_view {
        return std::visit([&self](auto &&value) -> std::string_view { return self(value); },
                          *assignExpr.getDirectDeclarator());
      }};
  std::visit(YComb{visitor}, declarator.getDirectDeclarator());
  return paramTypeList_;
}

} // namespace lcc