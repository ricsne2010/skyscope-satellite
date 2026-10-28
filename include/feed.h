// The connection to the SkyScope Pi: a task that polls it and keeps the latest
// traffic and map (see feed_types.h).
#pragma once

#include <Arduino.h>

#include "feed_types.h"

namespace feed {

// Connection to the Pi. Runs its own task; read with lock()/unlock().
void begin(const char* host, uint16_t port);
void setTarget(const char* host, uint16_t port);
void lock();
void unlock();
Traffic& traffic();        // only while locked
Map& map();                // only while locked
bool takeMapChanged();     // true once after a new map arrives
String status();           // human-readable connection state

}  // namespace feed
