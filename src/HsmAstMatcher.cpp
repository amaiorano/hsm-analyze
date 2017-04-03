#include "HsmAstMatcher.h"
#include "clang/ASTMatchers/ASTMatchers.h"

using namespace clang;
using namespace clang::ast_matchers;

namespace {
bool areSameVariable(const ValueDecl *First, const ValueDecl *Second) {
  return First && Second &&
         First->getCanonicalDecl() == Second->getCanonicalDecl();
}

bool areSameExpr(ASTContext *Context, const Expr *First, const Expr *Second) {
  if (!First || !Second)
    return false;
  llvm::FoldingSetNodeID FirstID, SecondID;
  First->Profile(FirstID, *Context, true);
  Second->Profile(SecondID, *Context, true);
  return FirstID == SecondID;
}

std::string getName(const CXXRecordDecl &RD) {
  std::string NameWithTemplateArgs;
  llvm::raw_string_ostream OS(NameWithTemplateArgs);
  RD.getNameForDiagnostic(OS, RD.getASTContext().getPrintingPolicy(), true);
  return OS.str();
}

std::string stringRemove(const std::string &S, const std::string &ToRemove) {
  std::string Result;
  size_t Index = 0, Off = 0;
  while ((Index = S.find(ToRemove, Off)) != -1) {
    Result += S.substr(Off, Index - Off);
    Off = Index + ToRemove.length();
  }
  Result += S.substr(Off, S.length() - Off);
  return Result;
}

std::string getName(const TemplateArgument &TA) {
  auto Name = TA.getAsType().getAsString();
  return stringRemove(stringRemove(Name, "struct "), "clang ");
}

inline TransitionType fuzzyNameToTransitionType(const StringRef &Name) {
  auto contains = [](auto stringRef, auto s) {
    return stringRef.find(s) != StringRef::npos;
  };

  if (contains(Name, "SiblingTransition"))
    return TransitionType::Sibling;
  if (contains(Name, "InnerTransition"))
    return TransitionType::Inner;
  if (contains(Name, "InnerEntryTransition"))
    return TransitionType::InnerEntry;
  assert(contains(Name, "No"));
  return TransitionType::No;
}

// Matches declaration of transition functions that actually cause a transition
// to occur (i.e. everything except NoTransition)
const auto TransitionFunctionDecl = hasDeclaration(eachOf(
    functionDecl(hasName("InnerTransition")).bind("trans_func_decl"),
    functionDecl(hasName("InnerEntryTransition")).bind("trans_func_decl"),
    functionDecl(hasName("SiblingTransition")).bind("trans_func_decl")));

// Matches declaration of state-derived classes
const auto StateDecl =
    cxxRecordDecl(decl().bind("state"), isDerivedFrom("hsm::State"));

// Matches transition function expressions (i.e. calls to transition functions)
// within member functions of states
const auto StateTransitionMatcher = callExpr(
    expr().bind("call_expr"), TransitionFunctionDecl,
    hasAncestor(cxxMethodDecl(ofClass(StateDecl))),
    anyOf(hasAncestor(
              callExpr(TransitionFunctionDecl).bind("arg_parent_call_expr")),
          anything()));

class StateTransitionMapper : public MatchFinder::MatchCallback {
  std::map<const CallExpr *, std::string> _CallExprToTargetState;
  StateTransitionMap &_StateTransitionMap;

public:
  StateTransitionMapper(StateTransitionMap &Map) : _StateTransitionMap(Map) {}

  virtual void run(const MatchFinder::MatchResult &Result) {
    const auto StateDecl = Result.Nodes.getNodeAs<CXXRecordDecl>("state");
    const auto TransCallExpr = Result.Nodes.getNodeAs<CallExpr>("call_expr");
    const auto TransitionFuncDecl =
        Result.Nodes.getNodeAs<FunctionDecl>("trans_func_decl");
    const auto ArgParentCallExpr =
        Result.Nodes.getNodeAs<CallExpr>("arg_parent_call_expr");

    assert(StateDecl);
    assert(TransCallExpr);
    assert(TransitionFuncDecl);

    const auto TransType = fuzzyNameToTransitionType(
        TransitionFuncDecl->getCanonicalDecl()->getName());

    auto SourceStateName = getName(*StateDecl);

    // We currently only support transition functions that accept target state
    // as a template parameter.
    // @TODO: support transition functions that accept StateFactory (e.g. state
    // overrides).
    const auto TSI = TransitionFuncDecl->getTemplateSpecializationInfo();
    if (!TSI)
      return;
    const TemplateArgument &TA = TSI->TemplateArguments->get(0);

    const auto TargetStateName = getName(TA);

    // If our transition is a state arg, we get the top-most transition's
    // target state and assume that this target state is the one that will
    // return the current transition. To do this, we track the top-most
    // CallExpr -> TargetStateName mapping.
    if (!ArgParentCallExpr) {
      // We're top-most, remember current target state
      assert(_CallExprToTargetState.find(TransCallExpr) ==
             _CallExprToTargetState.end());
      _CallExprToTargetState[TransCallExpr] = TargetStateName;
    } else {
      // Othwerise, use immediate parent CallExpr's target state as our
      // source state, and remember it for potential child CallExprs
      auto iter = _CallExprToTargetState.find(ArgParentCallExpr);
      assert(iter != _CallExprToTargetState.end());
      _CallExprToTargetState[TransCallExpr] = iter->second;

      // Override the source state name with the top-most CallExpr one
      SourceStateName = iter->second;
    }

    _StateTransitionMap.insert(std::make_pair(
        SourceStateName, std::make_tuple(TransType, TargetStateName)));
  }
};
} // namespace

namespace HsmAstMatcher {
void addMatchers(MatchFinder &Finder, StateTransitionMap &Map) {
  static StateTransitionMapper Mapper(Map);
  Finder.addMatcher(StateTransitionMatcher, &Mapper);
}
}
