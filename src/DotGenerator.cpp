#include "DotGenerator.h"
#include "StringHelpers.h"
#include <algorithm>
#include <cassert>
#include <functional>
#include <map>
#include <set>
#include <vector>

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

std::string makeFriendlyName(std::string name) {
  size_t index = name.rfind(':');
  if (index != -1) {
    return name.substr(index + 1);
  }
  return name;
}
}

namespace DotGenerator {
std::string generateDotFileContents(const StateTransitionMap &Map) {
  // We'd like GraphViz to generate a graph where states at the same depth are
  // placed at the same level (row or column, depending on rankdir). To do this,
  // we need to compute each state's maximal depth. We do this first by building
  // a graph containing the list of siblings and immediate inners for each
  // state. Then we traverse the graph and incrementally assign maximal depth to
  // each state.

  // Our graph is a map of state names to StateInfo
  struct StateInfo {
    int _Depth = 0;
    std::set<StateInfo *> Inners;
    std::set<StateInfo *> Siblings;
  };

  std::map<std::string, StateInfo> StateInfoMap;

  // Build graph
  for (auto &Kvp : Map) {
    auto SourceStateName = Kvp.first;
    auto TransType = std::get<0>(Kvp.second);
    auto TargetStateName = std::get<1>(Kvp.second);
    auto &SourceStateInfo = StateInfoMap[SourceStateName];
    auto &TargetStateInfo = StateInfoMap[TargetStateName];

    switch (TransType) {
    case TransitionType::Inner:
    case TransitionType::InnerEntry:
      SourceStateInfo.Inners.insert(&TargetStateInfo);
      break;

    case TransitionType::Sibling:
      SourceStateInfo.Siblings.insert(&TargetStateInfo);
      break;

    default:
      assert(false);
    }
  }

  // Traverse graph and compute depths
  std::function<void(StateInfo &)> computeDepths = [&](StateInfo &SI) {
    for (auto &Sibling : SI.Siblings) {
      // Sibling state depth is at least as deep as input's
      Sibling->_Depth = std::max(Sibling->_Depth, SI._Depth);
    }

    for (auto &Inner : SI.Inners) {
      // Immediate inner state depth is at least 1 deeper than input's
      Inner->_Depth = std::max(Inner->_Depth, SI._Depth + 1);
      computeDepths(*Inner); // Recurse
    }
  };

  for (auto &Kvp : StateInfoMap) {
    auto &SI = Kvp.second;
    computeDepths(SI);
  }

  // Write the dot file header
  std::string Result = "strict digraph G {\n"
                       "  fontname=Helvetica;\n"
                       "  nodesep=0.6;\n"
                       //"  node [shape=box]\n"
                       //"  rankdir=LR\n"
                       "";

  // Write all the graph edges
  for (auto &Kvp : Map) {
    auto SourceStateName = Kvp.first;
    auto TransType = std::get<0>(Kvp.second);
    auto TargetStateName = std::get<1>(Kvp.second);

    Result += FormatString<>("  %s -> %s %s\n",
                             makeValidDotNodeName(SourceStateName).c_str(),
                             makeValidDotNodeName(TargetStateName).c_str(),
                             getAttributesForTransition(TransType).c_str());
  }

  // Now write out subgraphs to set rank per state

  // Transform into a map of depth to state names
  std::map<int, std::set<std::string>> DepthToState;
  for (auto &Kvp : StateInfoMap) {
    const auto &StateName = Kvp.first;
    const auto &SI = Kvp.second;
    DepthToState[SI._Depth].emplace(StateName);
  }

  // Now we can write out a subgraph per depth with 'rank=same'. This results in
  // these states being laid out beside each other in the graph. We also label
  // the node with a friendlier name.
  for (const auto &Kvp : DepthToState) {
    const auto &Depth = Kvp.first;
    const auto &StateNames = Kvp.second;

    Result += FormatString<>("  subgraph {\n"
                             "    rank=same // depth=%d\n",
                             Depth);

    for (const auto &StateName : StateNames) {

      Result += FormatString<>("    %s [label=\"%s\"]\n",
                               makeValidDotNodeName(StateName).c_str(),
                               makeFriendlyName(StateName).c_str());
    }

    Result += "  }\n";
  }

  // Write footer
  Result += "}\n";

  return Result;
}
}
