#include "feed.h"

#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <WiFi.h>

#include <cstring>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "feed_parse.h"

namespace feed {
namespace {

constexpr unsigned long kPollMs = 1000;
constexpr int kTimeoutMs = 4000;

SemaphoreHandle_t s_mutex;
Traffic* s_front;  // what the screen reads (under the lock)
Traffic* s_back;   // what the next poll fills
Map s_map;
volatile bool s_mapChanged = false;

char s_host[64];
uint16_t s_port = 7700;
volatile bool s_targetChanged = false;
IPAddress s_ip;
char s_status[96] = "Starting...";

// "skyscope.local" → look it up with mDNS; anything else is a name or address.
bool resolve() {
  String host = s_host;
  if (host.endsWith(".local")) {
    const IPAddress ip = MDNS.queryHost(host.substring(0, host.length() - 6), 3000);
    if (ip == IPAddress()) {
      snprintf(s_status, sizeof s_status, "Can't find %s on the network", s_host);
      return false;
    }
    s_ip = ip;
    return true;
  }
  if (!s_ip.fromString(s_host) && !WiFi.hostByName(s_host, s_ip)) {
    snprintf(s_status, sizeof s_status, "Can't find %s", s_host);
    return false;
  }
  return true;
}

bool get(const char* path, JsonDocument& doc) {
  WiFiClient client;
  HTTPClient http;
  http.setTimeout(kTimeoutMs);
  http.setConnectTimeout(kTimeoutMs);
  const String url = "http://" + s_ip.toString() + ":" + String(s_port) + path;
  if (!http.begin(client, url)) return false;
  const int code = http.GET();
  if (code != 200) {
    snprintf(s_status, sizeof s_status, "SkyScope at %s:%u answered %d", s_host, s_port, code);
    http.end();
    return false;
  }
  const DeserializationError err = deserializeJson(doc, http.getStream());
  http.end();
  if (err) {
    snprintf(s_status, sizeof s_status, "Bad data from SkyScope: %s", err.c_str());
    return false;
  }
  return true;
}

bool fetchMap(const char* key) {
  JsonDocument doc;
  if (!get("/api/satellite/map", doc)) return false;
  xSemaphoreTake(s_mutex, portMAX_DELAY);
  parseMap(doc, s_map);
  s_mapChanged = true;
  xSemaphoreGive(s_mutex);
  return true;
}

void task(void*) {
  bool resolved = false;
  for (;;) {
    const unsigned long start = millis();
    if (s_targetChanged) {
      s_targetChanged = false;
      resolved = false;
    }
    if (WiFi.status() != WL_CONNECTED) {
      strlcpy(s_status, "Waiting for Wi-Fi...", sizeof s_status);
    } else if (!resolved && !(resolved = resolve())) {
      // status already says why
    } else {
      JsonDocument doc;
      if (get("/api/satellite", doc)) {
        parseTraffic(doc, *s_back);
        s_back->ok = true;
        s_back->receivedMs = millis();
        snprintf(s_status, sizeof s_status, "Connected to %s (%s)", s_host, s_ip.toString().c_str());
        xSemaphoreTake(s_mutex, portMAX_DELAY);
        Traffic* t = s_front;  // swap: the screen now reads the fresh copy
        s_front = s_back;
        s_back = t;
        const bool newMap = strcmp(s_front->mapKey, s_map.key) != 0;
        xSemaphoreGive(s_mutex);
        if (newMap) fetchMap(s_front->mapKey);
      } else {
        xSemaphoreTake(s_mutex, portMAX_DELAY);
        s_front->ok = false;
        xSemaphoreGive(s_mutex);
        resolved = false;  // look the Pi up again next time (its address may change)
      }
    }
    const unsigned long spent = millis() - start;
    vTaskDelay(pdMS_TO_TICKS(spent < kPollMs ? kPollMs - spent : 50));
  }
}

}  // namespace

void begin(const char* host, uint16_t port) {
  s_mutex = xSemaphoreCreateMutex();
  s_front = static_cast<Traffic*>(ps_calloc(1, sizeof(Traffic)));
  s_back = static_cast<Traffic*>(ps_calloc(1, sizeof(Traffic)));
  s_map.seg = static_cast<Segment*>(ps_calloc(kMaxSegments, sizeof(Segment)));
  s_front->age = s_back->age = -1;
  setTarget(host, port);
  xTaskCreatePinnedToCore(task, "feed", 12288, nullptr, 1, nullptr, 0);
}

void setTarget(const char* host, uint16_t port) {
  strlcpy(s_host, host, sizeof s_host);
  s_port = port;
  s_targetChanged = true;
}

void lock() { xSemaphoreTake(s_mutex, portMAX_DELAY); }
void unlock() { xSemaphoreGive(s_mutex); }
Traffic& traffic() { return *s_front; }
Map& map() { return s_map; }

bool takeMapChanged() {
  if (!s_mapChanged) return false;
  s_mapChanged = false;
  return true;
}

String status() { return String(s_status); }

}  // namespace feed
