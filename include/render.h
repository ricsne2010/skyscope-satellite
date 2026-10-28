// Composing the Satellite's screen: the scope (static layer, sweep, aircraft,
// labels) and the panel (card, list, status). No Arduino calls here, so the
// desktop preview (tools/preview) draws exactly what the board draws.
#pragma once

#include "feed_types.h"
#include "scope.h"

namespace render {

class Screen {
 public:
  // frame: the whole 800x480 screen; base: a 480x480 cache of the static scope.
  Screen(LGFX_Sprite& frame, LGFX_Sprite& base) : frame_(frame), base_(base) {}
  bool init() { return sweep_.init(); }

  // now: milliseconds on the clock that stamped t.receivedMs (aircraft move on
  // from their polled positions). status: why the Pi can't be reached, shown
  // when !t.ok. footer: this display's address.
  void draw(const feed::Traffic& t, const feed::Map& m, bool mapChanged, unsigned long now, const char* status,
            const char* footer);

 private:
  void buildBase(const feed::Traffic& t, const feed::Map& m);
  void drawPanel(const feed::Traffic& t, const char* status, const char* footer);

  LGFX_Sprite& frame_;
  LGFX_Sprite& base_;
  scope::Sweep sweep_;
  scope::Placed fixedLabels_;  // grid labels that aircraft labels avoid
  scope::Placed placed_;       // this frame's labels and symbols
  char ringKey_[24] = "";      // rebuild the static layer when the rings change
};

}  // namespace render
