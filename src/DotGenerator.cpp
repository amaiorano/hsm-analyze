#include "DotGenerator.h"
#include "StringHelpers.h"
#include <cassert>

namespace {
std::string getAttributesForTransition(TransitionType TransType) {
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
      R"([style="%s", weight="%d", color="%s"])", Style, Weight, Color);
  return Result;
};

std::string makeValidDotNodeName(std::string name) {
  size_t index = 0;
  while ((index = name.find(':', index)) != -1) {
    name[index] = '_';
  }
  return name;
};
}

namespace DotGenerator {
std::string generateDotFileContents(const StateTransitionMap &Map) {

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
}
