#pragma once

#include <map>

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

// Map of state name to state metadata
struct StateMetadata {
  unsigned int SourceLineNumber{};
};
using StateMetadataMap = std::map<std::string, StateMetadata>;
