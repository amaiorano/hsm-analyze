#pragma once

#include "HsmTypes.h"

namespace DotGenerator {
struct Options {
  bool LeftRightOrdering = false; // Dot orders top to bottom by default
};

std::string generateDotFileContents(const StateTransitionMap &Map,
                                    const Options &Options = {});
} // namespace DotGenerator
