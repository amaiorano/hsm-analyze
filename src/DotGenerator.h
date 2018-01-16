#pragma once

#include "HsmTypes.h"

namespace DotGenerator {
std::string generateDotFileContents(const StateTransitionMap &Map,
                                    const StateMetadataMap &MetadataMap);
}
