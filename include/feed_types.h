// What the SkyScope Pi sends a Satellite (/api/satellite and /api/satellite/map),
// held in fixed-size structures so nothing fragments the heap over days of running.
#pragma once

#include <cstdint>

namespace feed {

constexpr int kMaxAircraft = 250;
constexpr int kMaxRings = 8;
constexpr int kListRows = 14;
constexpr int kMaxSegments = 8000;
constexpr int kMaxLabels = 160;

struct Aircraft {
  float x, y, vx, vy;  // ring units, ring units per second
  int16_t nose, track;
  uint8_t kind, flags;  // flags: 1 emergency, 2 watched, 4 nearest, 8 beyond the ring
  uint16_t color;       // RGB565
  char call[9], type[5], alt[10], emergency[20];
};

struct Row {
  char call[9], type[5], alt[10], dist[8], brg[4], gs[6];
  uint8_t flags;
};

struct Card {
  bool valid;
  char call[12], desc[44], cls[20], alt[48], pos[48], spd[48];
};

// One poll's worth of traffic and status.
struct Traffic {
  bool ok;                    // last poll succeeded
  unsigned long receivedMs;   // millis() when it arrived (for motion between polls)
  char mapKey[160];
  char range[12];
  float ringFrac[kMaxRings];
  char ringLabel[kMaxRings][10];
  int ringCount;
  bool sweepOn;
  float sweepPeriod, sweepTrail;
  char loc[50], count[48], src[20], err[64];
  float age;                  // seconds since the Pi's last data, -1 if none yet
  char units[3][6];           // altitude, distance, speed
  Aircraft ac[kMaxAircraft];
  int acCount;
  Card card;
  Row list[kListRows];
  int listCount;
};

struct Segment {
  uint8_t style;  // 1 runway, 2 centreline, 3/4/5 airspace B/C/D, 6 SID, 7 STAR
  int16_t x0, y0, x1, y1;  // ring units x 10000
};

struct Label {
  uint8_t style;  // 8 airport, 9 fix, 10 airspace shelf
  int16_t x, y;   // ring units x 10000
  char text[10];
};

struct Map {
  char key[160];
  Segment* seg;  // in PSRAM
  int segCount;
  Label label[kMaxLabels];
  int labelCount;
};

}  // namespace feed
