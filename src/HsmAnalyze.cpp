// Declares clang::SyntaxOnlyAction.
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
// Declares llvm::cl::extrahelp.
#include "llvm/Support/CommandLine.h"

#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::tooling;
using namespace llvm;

// Apply a custom category to all command-line options so that they are the
// only ones displayed.
static llvm::cl::OptionCategory MyToolCategory("my-tool options");

// CommonOptionsParser declares HelpMessage with a description of the common
// command-line options related to the compilation database and input files.
// It's nice to have this help message in all tools.
static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);

// A help message for this specific tool can be added afterwards.
static cl::extrahelp MoreHelp("\nMore help text...");

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
} // namespace

enum class TransitionType {
  Sibling,
  Inner,
  InnerEntry,
  No,
};

static const char *TransitionTypeString[] = {"Sibling", "Inner", "InnerEntry",
                                             "No"};

static const char *TransitionTypeVisualString[] = {"--->", "==>>", "===>",
                                                   "XXXX"};

// Multimap of source state name to (transition, target state name) tuples
using StateTransitionMap =
    std::multimap<std::string, std::tuple<TransitionType, std::string>>;

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

// Utility for creating a temporary formatted string
template <int MaxLength = 1024> class FormatString {
  char _buffer[MaxLength];

public:
  FormatString(const char *format, ...) {
    va_list args;
    va_start(args, format);
    int Result = vsnprintf(_buffer, MaxLength, format, args);
    // If buffer too short, make sure to null terminate it
    if (Result < 0 || Result >= MaxLength) {
      _buffer[MaxLength - 1] = '\0';
    }
    va_end(args);
  }
  const char *value() const { return _buffer; }
  operator const char *() const { return value(); }
};

std::string generateDotFileContents(const StateTransitionMap &Map) {
  auto getAttributesForTransition = [](TransitionType TransType) {
    auto Weight = 1;
    auto Color = std::string("black");
    auto Style = std::string("solid");

    switch (TransType) {
    case TransitionType::Inner:
    case TransitionType::InnerEntry:
      if (TransType == TransitionType::InnerEntry)
        Style = "bold";
      break;

    case TransitionType::Sibling:
      Weight = 50;
      Style = "dotted";
      break;

    default:
      assert(false);
    }

    std::string Result = FormatString<>(
        R"([Style="%s", Weight="%d", Color="%s"])", Style, Weight, Color);
    return Result;
  };

  auto makeValidDotNodeName = [](std::string name) {
    size_t index = 0;
    while ((index = name.find(':', index)) != -1) {
      name[index] = '_';
    }
    return name;
  };

  std::string Result = "digraph G {\n"
                       "  nodesep=0.4;\n"
                       "  fontname=Helvetica;\n";

  for (auto &Kvp : Map) {
    auto SourceStateName = Kvp.first;
    auto TransType = std::get<0>(Kvp.second);
    auto TargetStateName = std::get<1>(Kvp.second);

    Result += FormatString<>("  %s -> %s %s\n",
                             makeValidDotNodeName(SourceStateName).c_str(),
                             makeValidDotNodeName(TargetStateName).c_str(),
                             getAttributesForTransition(TransType).c_str());
  }

  Result += "}\n";

  return Result;
}

int main(int argc, const char **argv) {
  CommonOptionsParser OptionsParser(argc, argv, MyToolCategory);
  ClangTool Tool(OptionsParser.getCompilations(),
                 OptionsParser.getSourcePathList());

  StateTransitionMap Map;

  StateTransitionMapper Mapper(Map);
  MatchFinder Finder;
  Finder.addMatcher(StateTransitionMatcher, &Mapper);
  auto Result = Tool.run(newFrontendActionFactory(&Finder).get());
  if (Result != 0) {
    return Result;
  }

  bool PrintMap = false;
  if (PrintMap) {
    for (auto &kvp : Map) {
      auto SourceStateName = kvp.first;
      auto TransType = std::get<0>(kvp.second);
      auto TargetStateName = std::get<1>(kvp.second);

      llvm::outs() << SourceStateName << " "
                   << TransitionTypeVisualString[static_cast<int>(TransType)]
                   << " " << TargetStateName << "\n";
    }
  }

  auto DotFileContents = generateDotFileContents(Map);
  llvm::outs() << DotFileContents;
}
