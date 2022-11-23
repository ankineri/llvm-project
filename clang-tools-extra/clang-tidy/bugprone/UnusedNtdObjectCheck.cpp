//===--- UnusedNtdObjectCheck.cpp - clang-tidy ----------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "UnusedNtdObjectCheck.h"
#include "../utils/OptionsUtils.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"

namespace clang {
namespace tidy {
namespace bugprone {

using namespace clang::ast_matchers;

namespace {
AST_MATCHER(VarDecl, isLocal) { return Node.isLocalVarDecl(); }
AST_MATCHER_P(DeclStmt, containsAnyDeclaration,
              ast_matchers::internal::Matcher<Decl>, InnerMatcher) {
  return ast_matchers::internal::matchesFirstInPointerRange(
             InnerMatcher, Node.decl_begin(), Node.decl_end(), Finder,
             Builder) != Node.decl_end();
}
} // namespace

UnusedNtdObjectCheck::UnusedNtdObjectCheck(StringRef Name,
                                           ClangTidyContext *Context)
    : ClangTidyCheck(Name, Context),
      CheckedTypes(Options.get("CheckedTypes", "::absl::Status")) {}

void UnusedNtdObjectCheck::storeOptions(ClangTidyOptions::OptionMap &Opts) {
  Options.store(Opts, "CheckedTypes", CheckedTypes);
}

void UnusedNtdObjectCheck::registerMatchers(MatchFinder *Finder) {
  auto TypesVec = utils::options::parseStringList(CheckedTypes);
  auto LocalValDecl =
      varDecl(allOf(isLocal(), hasType(recordDecl(hasAnyName(TypesVec)))));
  const auto FunctionScope = functionDecl(hasBody(
      compoundStmt(
          forEachDescendant(
              declStmt(containsAnyDeclaration(LocalValDecl.bind("local-value")),
                       unless(has(decompositionDecl())))
                  .bind("decl-stmt")))
          .bind("scope")));
  Finder->addMatcher(FunctionScope, this);
}

/// Traverses AST looking for variable reads after each write.
/// If at least once the variable has not been read IsUnused() returns true.
class UnusedVariableVisitor
    : public RecursiveASTVisitor<UnusedVariableVisitor> {
public:
  /// Initializes UnusedVariableVisitor
  /// \param VariableName the variable name to look for
  UnusedVariableVisitor(StringRef VariableName) : VariableName(VariableName) {}

  bool TraverseStmt(Stmt *Stmt) {
    if (Stmt == nullptr) {
      return true;
    }
    // If a class does not declare operator=, assignments will be
    // BinaryOperators.
    if (Stmt->getStmtClass() == Stmt::BinaryOperatorClass) {
      auto AsBinaryOperator = static_cast<BinaryOperator *>(Stmt);
      if (AsBinaryOperator->getOpcode() == clang::BO_Assign) {
        auto LHS = AsBinaryOperator->getLHS();
        auto RHS = AsBinaryOperator->getRHS();
        auto ProcessingResult = ProcessAssignmentOperator(LHS, RHS);
        if (ProcessingResult.has_value()) {
          return ProcessingResult.value();
        }
      }
    }

    // If a class does declare operator=, assignments will be
    // CXXOperatorCallExpr.
    if (Stmt->getStmtClass() == Stmt::CXXOperatorCallExprClass) {
      auto AsCxxOperator = (CXXOperatorCallExpr *)Stmt;
      if (AsCxxOperator->isAssignmentOp() && AsCxxOperator->getNumArgs() == 2) {
        auto LHS = AsCxxOperator->getArg(0);
        auto RHS = AsCxxOperator->getArg(1);
        auto ProcessingResult = ProcessAssignmentOperator(LHS, RHS);
        if (ProcessingResult.has_value()) {
          return ProcessingResult.value();
        }
      }
    }

    // If a value is used, we treat it as a read operation.
    if (Stmt->getStmtClass() == Stmt::DeclRefExprClass) {
      auto AsDeclRef = static_cast<DeclRefExpr *>(Stmt);
      auto Identifier = AsDeclRef->getDecl()->getIdentifier();
      if (Identifier != nullptr && Identifier->getName() == VariableName) {
        FoundUsage = true;
      }
    }
    RecursiveASTVisitor<UnusedVariableVisitor>::TraverseStmt(Stmt);

    return true;
  }

  /// After traversing the AST this returns whether VariableName was unused in
  /// AST \return true if the variable was unused
  bool IsUnused() { return UnusedInAssign || (!FoundUsage); }

private:
  StringRef VariableName;
  bool FoundUsage = false;

  // If we had at least one assignment before which the value was not read.
  bool UnusedInAssign = false;

  /// Processes an assignment operator. If lhs is the `varname` variable, it
  /// constitues a write operation, and the value must have been used before.
  /// \param LHS the left-hand side of the operator
  /// \param RHS the right-hand side of the operator
  /// \return false if an unused scenario was found; true if processing of this
  /// AST node is finished; nullopt if this node needs further processing.
  std::optional<bool> ProcessAssignmentOperator(Stmt *LHS, Stmt *RHS) {
    if (LHS->getStmtClass() == Stmt::DeclRefExprClass) {
      auto LHSAsDeclRef = static_cast<DeclRefExpr *>(LHS);
      if (LHSAsDeclRef->getDecl()->getIdentifier() != nullptr &&
          LHSAsDeclRef->getDecl()->getIdentifier()->getName() == VariableName) {
        if (!FoundUsage) {
          UnusedInAssign = true;
          return std::make_optional(false);
        }
        // Variable was assigned to, need to find another usage.
        FoundUsage = false;
      }
      RecursiveASTVisitor<UnusedVariableVisitor>::TraverseStmt(RHS);
      return std::make_optional(true);
    }
    return std::nullopt;
  }
};

void UnusedNtdObjectCheck::check(const MatchFinder::MatchResult &Result) {
  const auto *LocalScope = Result.Nodes.getNodeAs<CompoundStmt>("scope");
  const auto *Variable = Result.Nodes.getNodeAs<VarDecl>("local-value");

  // unused attribute suppresses the warning
  if (Variable->hasAttr<UnusedAttr>()) {
    return;
  }
  if (!Variable->getIdentifier()) {
    return;
  }

  UnusedVariableVisitor Visitor(Variable->getIdentifier()->getName().str());
  Visitor.TraverseCompoundStmt(const_cast<CompoundStmt *>(LocalScope), nullptr);
  if (!Visitor.IsUnused()) {
    return;
  }

  diag(Variable->getLocation(),
       "%0 is unlikely to be RAII and is potentially unused")
      << Variable;
}

} // namespace bugprone
} // namespace tidy
} // namespace clang
