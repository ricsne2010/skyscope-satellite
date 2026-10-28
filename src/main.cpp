// SkyScope Satellite: a small display that mirrors a SkyScope Pi's radar.
// The Pi does the work (data, filters, map); this draws what it sends.
// Board: Sunton ESP32-8048S043 (ESP32-S3, 4.3" 800x480). A Project Moon Pi project.

#include <Arduino.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiManager.h>

#include <cmath>
#include <cstring>

#include "board.h"
#include "device.h"
#include "feed.h"
#include "logo.h"
#include "render.h"

#define SKYSCOPE_SAT_VERSION "1.0.0"

namespace {

constexpr char kProduct[] = "SkyScope Satellite";
constexpr char kDefaultHost[] = "skyscope.local";
constexpr uint16_t kDefaultPort = 7700;
constexpr unsigned long kFrameMs = 100;  // ~10 frames a second

LGFX lcd;
LGFX_Sprite frame(&lcd);  // the whole screen, composed off-screen then shown at once
LGFX_Sprite base(&lcd);   // the scope's static layer: grid, rings and the Pi's map
render::Screen screen(frame, base);
WiFiManager wm;
WebServer server(80);
Preferences prefs;

char g_host[64];
uint16_t g_port;
char g_mdnsName[32];
char g_footer[80];

WiFiManagerParameter paramHost("pi_host", "SkyScope Pi address (e.g. skyscope.local or 192.168.1.20)", kDefaultHost, 63);
WiFiManagerParameter paramPort("pi_port", "SkyScope port", "7700", 6);

void loadSettings() {
  prefs.begin("satellite", true);
  strlcpy(g_host, prefs.getString("host", kDefaultHost).c_str(), sizeof g_host);
  g_port = prefs.getUShort("port", kDefaultPort);
  prefs.end();
}

void saveSettings(const char* host, uint16_t port) {
  strlcpy(g_host, host, sizeof g_host);
  g_port = port ? port : kDefaultPort;
  prefs.begin("satellite", false);
  prefs.putString("host", g_host);
  prefs.putUShort("port", g_port);
  prefs.end();
  feed::setTarget(g_host, g_port);
}

// --- the device's own small settings page -------------------------------------------

String htmlEscape(const String& s) {
  String o;
  for (char c : s) {
    if (c == '<') o += "&lt;";
    else if (c == '>') o += "&gt;";
    else if (c == '&') o += "&amp;";
    else if (c == '"') o += "&quot;";
    else o += c;
  }
  return o;
}

void handleRoot() {
  String page = F("<!doctype html><html><head><meta charset=utf-8><meta name=viewport content='width=device-width,initial-scale=1'>"
                  "<title>SkyScope Satellite</title><style>body{margin:0;background:#040a1c;color:#e8ecf2;font:16px system-ui,sans-serif}"
                  "main{max-width:560px;margin:0 auto;padding:20px}h1{color:#46ff78;margin:0}header{display:flex;gap:14px;align-items:center}"
                  "section{background:#0a1530;border:1px solid #1d2f55;border-radius:12px;padding:14px 16px;margin-top:16px}"
                  "h2{color:#ffc800;font-size:.8rem;letter-spacing:.09em;text-transform:uppercase;margin:0 0 8px}"
                  "label{display:block;margin-top:10px;font-weight:600}input{width:100%;box-sizing:border-box;padding:9px;margin-top:4px;"
                  "background:#071028;color:#e8ecf2;border:1px solid #1d2f55;border-radius:8px;font:inherit}"
                  "button{margin-top:14px;padding:10px 22px;border:0;border-radius:8px;background:#46ff78;color:#040a1c;font-weight:700}"
                  ".dim{color:#8a9abb}a{color:#46ff78}</style></head><body><main>"
                  "<header><img src=/logo.jpg width=72 height=72 alt='Project Moon Pi'><div><h1>SkyScope Satellite</h1>"
                  "<div class=dim>A Project Moon Pi project</div></div></header>");
  page += F("<section><h2>SkyScope Pi</h2><form method=post action=/save>"
            "<label>Address<input name=host value='");
  page += htmlEscape(g_host);
  page += F("'></label><label>Port<input name=port type=number value='");
  page += String(g_port);
  page += F("'></label><button>Save</button></form><p class=dim>");
  page += htmlEscape(feed::status());
  page += F("</p></section><section><h2>About</h2><p>Mirrors the radar of a SkyScope Pi on your network: its range, "
            "filters and map. Change those on the Pi's settings page.</p><p class=dim>Version " SKYSCOPE_SAT_VERSION
            " &middot; this display: ");
  page += htmlEscape(String(g_mdnsName) + ".local (" + device::address() + ")");
  page += F("</p><p><a href='https://github.com/ricsne2010/skyscope-satellite'>github.com/ricsne2010/skyscope-satellite</a></p>"
            "<p class=dim>Hold the BOOT button for 3 seconds to set up Wi-Fi again. Not for navigation.</p></section></main></body></html>");
  server.send(200, "text/html", page);
}

void handleSave() {
  const String host = server.arg("host");
  const long port = server.arg("port").toInt();
  if (host.length() && host.length() < sizeof g_host && port > 0 && port < 65536) {
    saveSettings(host.c_str(), static_cast<uint16_t>(port));
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleLogo() {
  size_t len;
  const uint8_t* jpg = logo::smallJpg(&len);
  server.sendHeader("Cache-Control", "max-age=86400");
  server.send_P(200, "image/jpeg", reinterpret_cast<const char*>(jpg), len);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  lcd.init();
  lcd.setBrightness(255);
  scope::color::init();
  device::showSplash(lcd, kProduct, "Version " SKYSCOPE_SAT_VERSION);
  delay(2500);

  loadSettings();
  paramHost.setValue(g_host, 63);
  wm.addParameter(&paramHost);
  wm.addParameter(&paramPort);
  wm.setSaveParamsCallback([] {
    saveSettings(paramHost.getValue(), static_cast<uint16_t>(atoi(paramPort.getValue())));
  });
  device::connectWifi(lcd, wm, "SkyScope-Satellite-Setup", kProduct);

  uint8_t mac[6];
  WiFi.macAddress(mac);
  snprintf(g_mdnsName, sizeof g_mdnsName, "skyscope-sat-%02x%02x", mac[4], mac[5]);
  device::startMdns(g_mdnsName);
  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/logo.jpg", HTTP_GET, handleLogo);
  server.begin();

  frame.setPsram(true);
  frame.setColorDepth(16);
  base.setPsram(true);
  base.setColorDepth(16);
  if (!frame.createSprite(board::kWidth, board::kHeight) || !base.createSprite(scope::geo.panelX, board::kHeight)) {
    const char* lines[] = {"Not enough memory for the screen buffers.", "This board needs 8 MB of PSRAM."};
    device::showMessage(lcd, "Can't start", lines, 2);
    for (;;) delay(1000);
  }
  screen.init();
  snprintf(g_footer, sizeof g_footer, "%s.local  -  %s", g_mdnsName, device::address().c_str());
  feed::begin(g_host, g_port);
}

void loop() {
  static unsigned long last = 0;
  server.handleClient();
  device::pollBootButton(lcd, wm);
  if (millis() - last >= kFrameMs) {
    last = millis();
    feed::lock();
    const String status = feed::status();
    screen.draw(feed::traffic(), feed::map(), feed::takeMapChanged(), millis(), status.c_str(), g_footer);
    feed::unlock();
    frame.pushSprite(0, 0);
  }
  delay(2);
}
