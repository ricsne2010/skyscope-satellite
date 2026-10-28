#include "render.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "logo.h"

namespace render {

namespace {

uint16_t segmentColor(uint8_t style) {
  switch (style) {
    case 1: return scope::color::runway;
    case 2: return scope::color::centerline;
    case 3: case 5: return scope::color::airspaceB;
    case 4: return scope::color::airspaceC;
    case 6: return scope::color::sid;
    case 7: return scope::color::star;
    default: return scope::color::grid;
  }
}

}  // namespace

// The static layer: grid and rings from the latest poll, then the Pi's map.
void Screen::buildBase(const feed::Traffic& t, const feed::Map& m) {
  LGFX_Sprite& base = base_;
  const char* labels[feed::kMaxRings];
  for (int i = 0; i < t.ringCount; ++i) labels[i] = t.ringLabel[i];
  fixedLabels_.clear();
  scope::drawGrid(base_, t.ringFrac, labels, t.ringCount, fixedLabels_);
  const float R = scope::geo.R;
  for (int i = 0; i < m.segCount; ++i) {
    const feed::Segment& s = m.seg[i];
    scope::line(base, scope::geo.cx + s.x0 / 10000.0f * R, scope::geo.cy + s.y0 / 10000.0f * R,
                scope::geo.cx + s.x1 / 10000.0f * R, scope::geo.cy + s.y1 / 10000.0f * R, segmentColor(s.style),
                s.style == 5 || s.style == 6 || s.style == 7, false);
  }
  scope::Placed mapLabels = fixedLabels_;  // map labels avoid each other and the grid labels
  base.setFont(&fonts::Font2);
  base.setTextDatum(textdatum_t::middle_left);
  for (int i = 0; i < m.labelCount; ++i) {
    const feed::Label& l = m.label[i];
    const int x = scope::geo.cx + l.x / 10000.0f * R, y = scope::geo.cy + l.y / 10000.0f * R;
    const int w = base.textWidth(l.text);
    const scope::Rect r{static_cast<int16_t>(x + 4), static_cast<int16_t>(y - 7), static_cast<int16_t>(w), 14};
    if (mapLabels.hits(r)) continue;
    mapLabels.add(r);
    const uint16_t c = l.style == 8 ? scope::color::runwayLabel : l.style == 10 ? scope::color::airspaceB
                                                                                 : (l.style == 9 ? scope::color::dim : scope::color::label);
    base.setTextColor(c);
    base.drawString(l.text, x + 4, y);
  }
}

void Screen::drawPanel(const feed::Traffic& t, const char* status, const char* footer) {
  LGFX_Sprite& frame = frame_;
  const bool reachable = t.ok;
  const int x0 = scope::geo.panelX;
  frame.fillRect(x0, 0, board::kWidth - x0, board::kHeight, scope::color::panel);
  frame.setClipRect(x0, 0, board::kWidth - x0 - 6, board::kHeight);  // long names stop short of the edge
  frame.drawFastVLine(x0, 0, board::kHeight, scope::color::grid);
  size_t len;
  const uint8_t* jpg = logo::smallJpg(&len);
  frame.drawJpg(jpg, len, x0 + 12, 10, 48, 48, 0, 0, 0.5f);
  frame.setTextDatum(textdatum_t::top_left);
  frame.setFont(&fonts::FreeSansBold12pt7b);
  frame.setTextColor(scope::color::sweep);
  frame.drawString("SKYSCOPE", x0 + 68, 12);
  frame.setFont(&fonts::Font2);
  frame.setTextColor(scope::color::dim);
  frame.drawString("SATELLITE", x0 + 68, 40);
  frame.setTextDatum(textdatum_t::top_right);
  frame.setFont(&fonts::FreeSansBold9pt7b);
  frame.setTextColor(scope::color::sweep);
  frame.drawString(t.range, board::kWidth - 10, 14);

  int y = 66;
  frame.setTextDatum(textdatum_t::top_left);
  frame.setFont(&fonts::Font2);
  frame.setTextColor(scope::color::dim);
  frame.drawString(t.loc, x0 + 12, y);
  y += 18;
  if (!reachable) {
    frame.setTextColor(scope::color::alert);
    frame.drawString(status, x0 + 12, y);
  } else {
    char line[96];
    snprintf(line, sizeof line, "%s  -  %s  -  %.0fs ago", t.count, t.src, t.age < 0 ? 0.0f : t.age);
    frame.setTextColor(t.err[0] ? scope::color::alert : scope::color::label);
    frame.drawString(t.age < 0 ? "Waiting for data..." : line, x0 + 12, y);
  }
  y += 22;
  frame.drawFastHLine(x0 + 12, y, board::kWidth - x0 - 24, scope::color::grid);
  y += 8;
  if (t.card.valid) {
    frame.setTextColor(scope::color::highlight);
    char eyebrow[40];
    snprintf(eyebrow, sizeof eyebrow, "NEAREST - %s", t.card.cls);
    frame.drawString(eyebrow, x0 + 12, y);
    y += 16;
    frame.setFont(&fonts::FreeSansBold18pt7b);
    frame.setTextColor(scope::color::label);
    frame.drawString(t.card.call, x0 + 12, y);
    y += 34;
    frame.setFont(&fonts::Font2);
    frame.setTextColor(scope::color::type);
    frame.drawString(t.card.desc, x0 + 12, y);
    y += 17;
    frame.setTextColor(scope::color::alt);
    frame.drawString(t.card.alt, x0 + 12, y);
    y += 17;
    frame.setTextColor(scope::color::label);
    frame.drawString(t.card.pos, x0 + 12, y);
    y += 17;
    frame.drawString(t.card.spd, x0 + 12, y);
    y += 22;
    frame.drawFastHLine(x0 + 12, y, board::kWidth - x0 - 24, scope::color::grid);
    y += 8;
  }
  // The list: callsign, type, altitude, distance, ground speed.
  const int cx[5] = {x0 + 12, x0 + 92, x0 + 186, x0 + 238, x0 + 300};
  const char* head[5] = {"CALLSIGN", "TYPE", "ALT", "DIST", "GS"};
  frame.setFont(&fonts::Font2);
  frame.setTextColor(scope::color::dim);
  for (int i = 0; i < 5; ++i) {
    frame.setTextDatum(i < 2 ? textdatum_t::top_left : textdatum_t::top_right);
    char h[20];
    if (i >= 2) snprintf(h, sizeof h, "%s %s", head[i], t.units[i == 2 ? 0 : i == 3 ? 1 : 2]);
    else snprintf(h, sizeof h, "%s", head[i]);
    frame.drawString(h, cx[i], y);
  }
  y += 18;
  for (int r = 0; r < t.listCount && y < board::kHeight - 30; ++r, y += 17) {
    const feed::Row& row = t.list[r];
    const char* cells[5] = {row.call, row.type, row.alt, row.dist, row.gs};
    const uint16_t cols[5] = {
        static_cast<uint16_t>(row.flags & 1 ? scope::color::alert : row.flags & 2 ? scope::color::watch : scope::color::label),
        scope::color::type, scope::color::alt, scope::color::label, scope::color::label};
    for (int i = 0; i < 5; ++i) {
      frame.setTextDatum(i < 2 ? textdatum_t::top_left : textdatum_t::top_right);
      frame.setTextColor(cols[i]);
      frame.drawString(cells[i], cx[i], y);
    }
  }
  frame.setTextDatum(textdatum_t::bottom_left);
  frame.setTextColor(scope::color::dim);
  frame.drawString(footer, x0 + 12, board::kHeight - 8);
  frame.clearClipRect();
}

void Screen::draw(const feed::Traffic& t, const feed::Map& m, bool mapChanged, unsigned long now,
                  const char* status, const char* footer) {
  // Rebuild the static layer when the rings or the map change.
  char ringKey[sizeof ringKey_];
  snprintf(ringKey, sizeof ringKey, "%s/%d", t.range, t.ringCount);
  if (mapChanged || strcmp(ringKey, ringKey_) != 0) {
    memcpy(ringKey_, ringKey, sizeof ringKey_);
    buildBase(t, m);
  }
  LGFX_Sprite& frame = frame_;
  base_.pushSprite(&frame, 0, 0);
  if (t.sweepOn && t.ringCount) {
    const float period = t.sweepPeriod > 0.5f ? t.sweepPeriod : 4.0f;
    sweep_.apply(frame, board::kWidth, std::fmod(now / 1000.0f / period * 360.0f, 360.0f), t.sweepTrail);
  }

  // Aircraft, moved along their tracks since the poll.
  const float dt = (now - t.receivedMs) / 1000.0f;
  const float R = scope::geo.R;
  static float xs[feed::kMaxAircraft], ys[feed::kMaxAircraft];
  const bool flashOn = (now / 500) % 2;
  for (int i = 0; i < t.acCount; ++i) {
    const feed::Aircraft& a = t.ac[i];
    const float rx = a.x + a.vx * dt, ry = a.y + a.vy * dt;
    xs[i] = scope::geo.cx + rx * R;
    ys[i] = scope::geo.cy + ry * R;
    const uint16_t c = (a.flags & 1) ? (flashOn ? scope::color::label : scope::color::alert) : a.color;
    if (a.flags & 8) {  // beyond the ring: a dot on the rim in its direction
      const float d = std::hypot(rx, ry);
      frame.fillCircle(scope::geo.cx + rx / d * scope::geo.dotR, scope::geo.cy + ry / d * scope::geo.dotR, 4, c);
      continue;
    }
    if (std::hypot(rx, ry) > 1.0f) continue;
    const float v = std::hypot(a.vx, a.vy) * 60 * R;  // one minute of travel, px
    scope::drawSpeedLine(frame, a.kind, xs[i], ys[i], a.nose, a.track, std::min(40.0f, std::max(5.0f, v)),
                         a.flags & 1 ? scope::color::alert : scope::color::track);
    scope::drawSymbol(frame, a.kind, xs[i], ys[i], a.nose, c);
    const float sr = scope::symbolRadius(a.kind);
    if (a.flags & 2) frame.drawCircle(xs[i], ys[i], sr + 6, scope::color::watch);
    if (a.flags & 1) frame.drawCircle(xs[i], ys[i], sr + 6 + (now / 70) % 10, scope::color::alert);
    if (a.flags & 4) frame.drawCircle(xs[i], ys[i], sr + 5, scope::color::highlight);
  }
  // Labels: the Pi already sorted alerts first, then nearest first.
  scope::Placed& placed = placed_;
  placed = fixedLabels_;
  for (int i = 0; i < t.acCount; ++i) {
    const float r = scope::symbolRadius(t.ac[i].kind);
    placed.add({static_cast<int16_t>(xs[i] - r), static_cast<int16_t>(ys[i] - r), static_cast<int16_t>(2 * r),
                static_cast<int16_t>(2 * r)});
  }
  for (int i = 0; i < t.acCount; ++i) {
    const feed::Aircraft& a = t.ac[i];
    if (a.flags & 8) continue;
    const char* lines[3] = {a.flags & 1 ? a.emergency : a.call, a.flags & 1 ? a.call : a.type, a.alt};
    const uint16_t cols[3] = {a.flags & 1 ? scope::color::alert : (a.flags & 2 ? scope::color::watch : scope::color::label),
                              a.flags & 1 ? scope::color::label : scope::color::type, scope::color::alt};
    scope::drawTag(frame, xs[i], ys[i], scope::symbolRadius(a.kind), lines, cols, placed);
  }
  drawPanel(t, status, footer);
}

}  // namespace render
