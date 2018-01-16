#pragma once

#include "HsmTypes.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"

namespace HsmAstMatcher {
void addMatchers(clang::ast_matchers::MatchFinder &Finder,
                 StateTransitionMap &Map, StateMetadataMap &MetadataMap);
}
