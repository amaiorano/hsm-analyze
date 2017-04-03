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
