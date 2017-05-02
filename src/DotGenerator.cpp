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
      R"(style="%s", weight="%d", color="%s")", Style, Weight, Color);
  return Result;
}

// Replaces all invalid characters with underscores
std::string makeValidDotNodeName(std::string name) {
  // TODO: make this cleaner and more efficient
  size_t Index = 0;
  while ((Index = name.find(':', Index)) != -1) {
    name[Index] = '_';
    ++Index;
  }
  Index = 0;
  while ((Index = name.find('<', Index)) != -1) {
    name[Index] = '_';
    ++Index;
  }
  Index = 0;
  while ((Index = name.find('>', Index)) != -1) {
    name[Index] = '_';
    ++Index;
  }
  return name;
}

// Removes namespaces and returns only the final type name
// e.g. A::B::C<D> returns C<D>
// e.g. A::B<C<D::E::F>>::G return B<C<D::E::F>>::G
std::string makeFriendlyName(std::string name) {
  // Basically we return the substring starting from the last ':' before the
  // first '<'
  size_t EndIndex = name.find('<');
  if (EndIndex == -1)
    EndIndex = name.size();
  size_t Index = name.rfind(':', EndIndex - 1);
  if (Index != -1)
    return name.substr(Index + 1);
  return name;
}

std::vector<std::string> splitString(std::string S, std::string Sep) {
  std::vector<std::string> Result;
  if (S.size() == 0)
    return Result;
  size_t Index = 0;
  size_t LastIndex = 0;
  while ((Index = S.find(Sep, Index)) != -1) {
    Result.push_back(S.substr(LastIndex, Index - LastIndex));
    LastIndex = Index + Sep.size();
    Index = LastIndex + 1;
  }
  Result.push_back(S.substr(LastIndex, std::string::npos));
  return Result;
}

template <typename T> T lerp(T V, T Min, T Max) {
  T Range = Max - Min;
  if (Range == 0) {
    assert(V == Min && V == Max);
    return 0;
  }
  return (V - Min) / (Max - Min);
}

template <typename T, typename U>
U remap(T SourceVal, T SourceMin, T SourceMax, U TargetMin, U TargetMax) {
  float Ratio = lerp<float>(SourceVal, SourceMin, SourceMax);
  float Result = TargetMin + Ratio * (TargetMax - TargetMin);
  return static_cast<U>(Result);
}

template <typename TargetType, typename SourceType>
TargetType hash(const SourceType &Val) {
  size_t Hash = std::hash<SourceType>()(Val);
  return Hash % std::numeric_limits<TargetType>::max();
}

template <typename T> constexpr T maxValue(T &V) {
  return std::numeric_limits<T>::max();
}
} // namespace

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

  // Recursive function that computes depths across siblings and inners of input
  // state
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

  for (int i = 0; i < 100; ++i) { // TODO: Run until no depths change anymore
    for (auto &Kvp : StateInfoMap) {
      auto &SI = Kvp.second;
      computeDepths(SI);
    }
  }

  // Returns true if State1 and State2 both making sibling transitons to each
  // other
  auto arePingPongSiblings = [&StateInfoMap](std::string State1,
                                             std::string State2) {
    auto &SI1 = StateInfoMap[State1];
    auto &SI2 = StateInfoMap[State2];

    return SI1.Siblings.find(&SI2) != SI1.Siblings.end() &&
           SI2.Siblings.find(&SI1) != SI2.Siblings.end();
  };

  // Write the dot file header
  std::string Result = "strict digraph G {\n"
                       "  fontname=Helvetica;\n"
                       "  nodesep=0.6;\n"
                       //"  node [shape=box]\n"
                       //"  rankdir=LR\n"
                       "";

  // Write all the graph edges

  std::set<std::string> PingPongStatesWritten;

  for (auto &Kvp : Map) {
    auto SourceStateName = Kvp.first;
    auto TransType = std::get<0>(Kvp.second);
    auto TargetStateName = std::get<1>(Kvp.second);

    std::string Attributes = getAttributesForTransition(TransType);

    // If source and target are ping-pong siblings, only write the first edge
    // with attribute dir="both", which instructs GraphViz to make a
    // double-ended edge between the two nodes
    if (arePingPongSiblings(SourceStateName, TargetStateName)) {
      if (PingPongStatesWritten.find(TargetStateName) !=
          PingPongStatesWritten.end())
        continue;

      PingPongStatesWritten.insert(SourceStateName);

      Attributes += R"(, dir="both")";
    }

    Result += FormatString<>(
        "  %s -> %s [%s]\n", makeValidDotNodeName(SourceStateName).c_str(),
        makeValidDotNodeName(TargetStateName).c_str(), Attributes.c_str());
  }
  Result += '\n';

  // Now write out subgraphs to set rank per state

  // We want to create clusters by namespace. The dot file format is quite
  // flexible when it comes to how it processes clusters: we can repeat the same
  // cluster hierarchy declarations, the only thing we need to do is make sure
  // that nodes of the same rank appear together with "rank=same".

  auto getStateNamespace = [](std::string StateName) -> std::string {
    int Index = StateName.rfind("::");
    if (Index != -1) {
      return StateName.substr(0, Index);
    }
    return "";
  };

  while (!StateInfoMap.empty()) {

    std::string CurrNamespace = getStateNamespace(StateInfoMap.begin()->first);

    // Collect all StateInfoMap entries for the current namespace, moving them
    // into StateInfoMap2. The outer loop will end once all entries have been
    // moved and processed.
    decltype(StateInfoMap) StateInfoMap2;
    for (auto iter = StateInfoMap.begin(); iter != StateInfoMap.end();) {
      if (getStateNamespace(iter->first) == CurrNamespace) {
        StateInfoMap2.insert(*iter);
        iter = StateInfoMap.erase(iter);
      } else {
        ++iter;
      }
    }

    // Create a map of depth to state names from StateInfoMap2
    std::map<int, std::set<std::string>> DepthToState;
    for (auto &Kvp : StateInfoMap2) {
      const auto &StateName = Kvp.first;
      const auto &SI = Kvp.second;
      DepthToState[SI._Depth].emplace(StateName);
    }

    // Determine min/max depth
    auto MinMaxDepth =
        std::minmax_element(StateInfoMap2.begin(), StateInfoMap2.end(),
                            [](const auto &P1, const auto &P2) {
                              return P1.second._Depth < P2.second._Depth;
                            });
    const int MinDepth = MinMaxDepth.first->second._Depth;
    const int MaxDepth = MinMaxDepth.second->second._Depth;

    // Finally, write out subgraphs per depth with 'rank=same', each within its
    // own namespace cluster. This results in these states being laid out beside
    // each other in the graph. We also label the node with a friendlier name.
    for (const auto &Kvp : DepthToState) {
      const auto &Depth = Kvp.first;
      const auto &StateNames = Kvp.second;

      // Open up a cluster per namespace part
      // TODO: indent output for readability
      auto NamespaceParts = splitString(CurrNamespace, "::");
      for (auto Part : NamespaceParts) {
        Result += FormatString<>(
            "  subgraph cluster_%s { label = \"%s\"; labeljust=left;\n",
            makeValidDotNodeName(Part).c_str(), Part.c_str());
      }

      // Write subgraphs for states of same depth
      Result += FormatString<>("    subgraph {\n"
                               "      rank=same // depth=%d\n",
                               Depth);

      for (const auto &StateName : StateNames) {
        std::string StateAttributes = FormatString<>(
            R"(label="%s")", makeFriendlyName(StateName).c_str());

        static bool EnableColor = true;
        if (EnableColor) {
          // Hue
          float MinH = 0.f;
          float MaxH = 1.f;
          auto Hash = hash<uint32_t>(CurrNamespace);
          float H = remap(Hash, 0u, maxValue(Hash), MinH, MaxH);

          // Saturation
          float S = 0.5f;

          // Value (aka Brightness)
          float MinV = 0.25f;
          float MaxV = 0.7f;
          float V = remap(Depth, MinDepth, MaxDepth, MinV, MaxV);

          StateAttributes += FormatString<>(
              R"(fontcolor=white, style=filled, color="%f %f %f")", H, S, V);
        }

        Result += FormatString<>("      %s [%s]\n",
                                 makeValidDotNodeName(StateName).c_str(),
                                 StateAttributes.c_str());
      }
      Result += "    }\n";

      // Close clusters
      Result += "  ";
      for (auto Part : NamespaceParts)
        Result += "}";
      Result += "\n\n";
    }
  }

  // Write footer
  Result += "}\n";

  return Result;
}
} // namespace DotGenerator
