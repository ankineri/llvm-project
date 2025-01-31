//===--- UnusedNtdObjectCheck.h - clang-tidy --------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_BUGPRONE_UNUSEDNTDOBJECTCHECK_H
#define LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_BUGPRONE_UNUSEDNTDOBJECTCHECK_H

#include "../ClangTidyCheck.h"
#include <clang/Analysis/Analyses/ExprMutationAnalyzer.h>

namespace clang {
namespace tidy {
namespace bugprone {

/// Checks for unused objects of non-trivially-destructible (ntd) types
/// which are unlikely to be used for RAII. Trivially destructible objects are
/// covered with -Wunused, but ntd objects don't cause this warning due to
/// destructor side-effects.
/// One important ntd type is absl::Status.
///
/// For the user-facing documentation see:
/// http://clang.llvm.org/extra/clang-tidy/checks/bugprone/unused-ntd-object.html
class UnusedNtdObjectCheck : public ClangTidyCheck {
public:
  UnusedNtdObjectCheck(StringRef Name, ClangTidyContext *Context);
  void registerMatchers(ast_matchers::MatchFinder *Finder) override;
  void check(const ast_matchers::MatchFinder::MatchResult &Result) override;
  void storeOptions(ClangTidyOptions::OptionMap &Opts) override;

private:
  std::string CheckedTypes;
};

} // namespace bugprone
} // namespace tidy
} // namespace clang

#endif // LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_BUGPRONE_UNUSEDNTDOBJECTCHECK_H
