#include "feed_parse.h"

#include <cstddef>

#include "scope.h"

namespace feed {
namespace {

// The Pi's text is UTF-8; the board's fonts are plain ASCII. Copies v into dst
// (bounded), spelling the few symbols SkyScope uses in ASCII and dropping
// anything else that isn't.
void copy(char* dst, size_t n, JsonVariantConst v) {
  const char* s = v.as<const char*>();
  size_t o = 0;
  for (const unsigned char* p = reinterpret_cast<const unsigned char*>(s ? s : ""); *p && o + 1 < n;) {
    if (*p < 0x80) {
      dst[o++] = static_cast<char>(*p++);
      continue;
    }
    unsigned cp = 0;
    int more = *p >= 0xF0 ? 3 : *p >= 0xE0 ? 2 : 1;
    cp = *p++ & (0x3F >> more);
    for (; more && (*p & 0xC0) == 0x80; --more) cp = (cp << 6) | (*p++ & 0x3F);
    const char* sub = cp == 0xB7 || cp == 0x2014 || cp == 0x2013 ? "-"   // middle dot, dashes
                      : cp == 0x2192 ? ">"                              // arrow
                      : cp == 0xB1 ? "+/-"
                      : "";                                             // degree sign and the rest
    for (; *sub && o + 1 < n; ++sub) dst[o++] = *sub;
  }
  dst[o] = 0;
}

}  // namespace

void parseTraffic(JsonVariantConst doc, Traffic& t) {
  copy(t.mapKey, sizeof t.mapKey, doc["map"]);
  copy(t.range, sizeof t.range, doc["range"]);
  t.ringCount = 0;
  for (JsonArrayConst r : doc["rings"].as<JsonArrayConst>()) {
    if (t.ringCount >= kMaxRings) break;
    t.ringFrac[t.ringCount] = r[0];
    copy(t.ringLabel[t.ringCount], sizeof t.ringLabel[0], r[1]);
    ++t.ringCount;
  }
  t.sweepOn = doc["sweep"][0] | true;
  t.sweepPeriod = doc["sweep"][1] | 4.0f;
  t.sweepTrail = doc["sweep"][2] | 90.0f;
  copy(t.loc, sizeof t.loc, doc["loc"]);
  copy(t.count, sizeof t.count, doc["count"]);
  copy(t.src, sizeof t.src, doc["src"]);
  copy(t.err, sizeof t.err, doc["err"]);
  t.age = doc["age"].isNull() ? -1.0f : doc["age"].as<float>();
  for (int i = 0; i < 3; ++i) copy(t.units[i], sizeof t.units[0], doc["units"][i]);
  t.acCount = 0;
  for (JsonArrayConst a : doc["ac"].as<JsonArrayConst>()) {
    if (t.acCount >= kMaxAircraft) break;
    Aircraft& p = t.ac[t.acCount++];
    p.x = a[1];
    p.y = a[2];
    p.vx = a[3];
    p.vy = a[4];
    p.nose = a[5];
    p.track = a[6];
    p.kind = a[7];
    p.color = scope::rgb(a[8].as<uint32_t>());
    copy(p.call, sizeof p.call, a[9]);
    copy(p.type, sizeof p.type, a[10]);
    copy(p.alt, sizeof p.alt, a[11]);
    p.flags = a[12];
    copy(p.emergency, sizeof p.emergency, a[13]);
  }
  JsonObjectConst c = doc["card"].as<JsonObjectConst>();
  t.card.valid = !c.isNull();
  if (t.card.valid) {
    copy(t.card.call, sizeof t.card.call, c["call"]);
    copy(t.card.desc, sizeof t.card.desc, c["desc"]);
    copy(t.card.cls, sizeof t.card.cls, c["cls"]);
    copy(t.card.alt, sizeof t.card.alt, c["alt"]);
    copy(t.card.pos, sizeof t.card.pos, c["pos"]);
    copy(t.card.spd, sizeof t.card.spd, c["spd"]);
  }
  t.listCount = 0;
  for (JsonArrayConst r : doc["list"].as<JsonArrayConst>()) {
    if (t.listCount >= kListRows) break;
    Row& row = t.list[t.listCount++];
    copy(row.call, sizeof row.call, r[0]);
    copy(row.type, sizeof row.type, r[1]);
    copy(row.alt, sizeof row.alt, r[2]);
    copy(row.dist, sizeof row.dist, r[3]);
    copy(row.brg, sizeof row.brg, r[4]);
    copy(row.gs, sizeof row.gs, r[5]);
    row.flags = r[6];
  }
}

void parseMap(JsonVariantConst doc, Map& m) {
  copy(m.key, sizeof m.key, doc["key"]);
  m.segCount = 0;
  for (JsonArrayConst l : doc["lines"].as<JsonArrayConst>()) {
    if (m.segCount >= kMaxSegments) break;
    Segment& s = m.seg[m.segCount++];
    s.style = l[0];
    s.x0 = static_cast<int16_t>(l[1].as<float>() * 10000);
    s.y0 = static_cast<int16_t>(l[2].as<float>() * 10000);
    s.x1 = static_cast<int16_t>(l[3].as<float>() * 10000);
    s.y1 = static_cast<int16_t>(l[4].as<float>() * 10000);
  }
  m.labelCount = 0;
  for (JsonArrayConst l : doc["labels"].as<JsonArrayConst>()) {
    if (m.labelCount >= kMaxLabels) break;
    Label& lab = m.label[m.labelCount++];
    lab.style = l[0];
    lab.x = static_cast<int16_t>(l[1].as<float>() * 10000);
    lab.y = static_cast<int16_t>(l[2].as<float>() * 10000);
    copy(lab.text, sizeof lab.text, l[3]);
  }
}

}  // namespace feed
