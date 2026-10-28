// Turning the Pi's JSON into the feed structures. No networking here, so the
// desktop preview (tools/preview) uses the same code.
#pragma once

#include <ArduinoJson.h>

#include "feed_types.h"

namespace feed {

void parseTraffic(JsonVariantConst doc, Traffic& t);  // /api/satellite
void parseMap(JsonVariantConst doc, Map& m);          // /api/satellite/map (m.seg must be allocated)

}  // namespace feed
