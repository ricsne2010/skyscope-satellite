// Desktop preview: draws one Satellite frame from saved /api/satellite and
// /api/satellite/map responses with the firmware's own drawing code, and
// writes it as a PPM image. See tools/preview/build.sh.
//
//   preview traffic.json map.json logo.jpg out.ppm [sweep-seconds]

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "feed_parse.h"
#include "logo.h"
#include "render.h"

namespace {
std::vector<uint8_t> g_logo;

std::string slurp(const char* path) {
  std::ifstream f(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(f), {});
}
}  // namespace

namespace logo {
const uint8_t* splashJpg(size_t* len) { return smallJpg(len); }
const uint8_t* smallJpg(size_t* len) {
  *len = g_logo.size();
  return g_logo.data();
}
}  // namespace logo

int main(int argc, char** argv) {
  if (argc < 5) {
    fprintf(stderr, "usage: %s traffic.json map.json logo.jpg out.ppm [sweep-seconds]\n", argv[0]);
    return 2;
  }
  const std::string logoBytes = slurp(argv[3]);
  g_logo.assign(logoBytes.begin(), logoBytes.end());
  const unsigned long now = argc > 5 ? static_cast<unsigned long>(atof(argv[5]) * 1000) : 1000;

  static feed::Traffic t;
  static feed::Map m;
  m.seg = static_cast<feed::Segment*>(calloc(feed::kMaxSegments, sizeof(feed::Segment)));
  JsonDocument doc;
  if (deserializeJson(doc, slurp(argv[1]))) return fprintf(stderr, "bad traffic JSON\n"), 1;
  feed::parseTraffic(doc, t);
  t.ok = true;
  t.receivedMs = now;
  if (deserializeJson(doc, slurp(argv[2]))) return fprintf(stderr, "bad map JSON\n"), 1;
  feed::parseMap(doc, m);
  printf("%d aircraft, %d list rows, %d segments, %d labels, range %s\n", t.acCount, t.listCount, m.segCount,
         m.labelCount, t.range);

  scope::color::init();
  LGFX_Sprite frame, base;
  frame.setColorDepth(16);
  base.setColorDepth(16);
  if (!frame.createSprite(board::kWidth, board::kHeight) || !base.createSprite(scope::geo.panelX, board::kHeight)) {
    return fprintf(stderr, "no memory\n"), 1;
  }
  render::Screen screen(frame, base);
  screen.init();
  screen.draw(t, m, true, now, "", "skyscope-sat-1a2b.local  -  192.168.1.50");

  FILE* out = fopen(argv[4], "wb");
  fprintf(out, "P6\n%d %d\n255\n", board::kWidth, board::kHeight);
  for (int y = 0; y < board::kHeight; ++y) {
    for (int x = 0; x < board::kWidth; ++x) {
      const auto c = frame.readPixelRGB(x, y);
      const uint8_t px[3] = {c.R8(), c.G8(), c.B8()};
      fwrite(px, 1, 3, out);
    }
  }
  fclose(out);
  return 0;
}
