#include "boot_log.h"
#include <ArduinoJson.h>
#include <WiFi.h>
#include <esp_system.h>
#include <esp_heap_caps.h>
#include "project_config.h"
#include "../../services/log_manager/log_manager.h"
#include "../../services/storage_manager/storage_manager.h"
#include "../../services/config_manager/config_manager.h"
#include "../../services/web_assets/web_assets.h"
#include "../../api/api_router/api_router.h"

#ifndef MAX_BOOT_LOG_ENTRIES
#define MAX_BOOT_LOG_ENTRIES 10
#endif
#ifndef BOOT_LOG_BUFFER_LINES
#define BOOT_LOG_BUFFER_LINES 20
#endif
#ifndef BOOT_LOG_LINE_MAX_LEN
#define BOOT_LOG_LINE_MAX_LEN 160
#endif
#ifndef BOOT_LOG_STATS_INTERVAL_MS
#define BOOT_LOG_STATS_INTERVAL_MS 30000
#endif

// IMPORTANT : ce magic doit être incrémenté à chaque fois que la structure
// BootLogRtcData change de layout (champs ajoutés/retirés/réordonnés ou
// dimensions kLines/kLineLen modifiées). RTC_NOINIT_ATTR survit à un flash de
// firmware (la RAM RTC n'est pas touchée par l'écriture de la partition app) :
// si le magic reste identique après une mise à jour qui change le layout,
// begin() relira les anciens octets bruts à travers le nouveau layout et
// produira des données corrompues.
static const uint32_t kMagic = 0xB007106Bu;

static const int kLines   = BOOT_LOG_BUFFER_LINES;
static const int kLineLen = BOOT_LOG_LINE_MAX_LEN;
static const int kTaskLen = 40;
static const int kIpLen   = 16;

struct BootLogRtcData {
    uint32_t magic;
    uint16_t count;
    uint16_t next;
    char     lines[kLines][kLineLen];

    char     lastTask[kTaskLen];

    uint32_t lastUptimeMs;
    BootLogRuntimeStats stats;

    int8_t   wifiStatus;
    int8_t   wifiRssi;
    char     wifiIp[kIpLen];

    float    temperatureC;
};

// Survit à un reboot logiciel/crash/watchdog, pas à une coupure
// d'alimentation ni à un reset franc (EN/bouton reset physique).
RTC_NOINIT_ATTR static BootLogRtcData _rtc;

BootLogModule bootLogModule;

static const char* _reasonText(esp_reset_reason_t r) {
    switch (r) {
        case ESP_RST_POWERON:   return "Mise sous tension";
        case ESP_RST_EXT:       return "Reset externe (broche)";
        case ESP_RST_SW:        return "Redémarrage logiciel (ESP.restart)";
        case ESP_RST_PANIC:     return "PANIC / exception non gérée";
        case ESP_RST_INT_WDT:   return "WATCHDOG (interrupt)";
        case ESP_RST_TASK_WDT:  return "WATCHDOG (tâche bloquée)";
        case ESP_RST_WDT:       return "WATCHDOG (autre)";
        case ESP_RST_DEEPSLEEP: return "Réveil deep sleep";
        case ESP_RST_BROWNOUT:  return "BROWNOUT (chute de tension)";
        case ESP_RST_SDIO:      return "Reset SDIO";
        default:                return "Inconnue";
    }
}

// Reset "anormal" = à comptabiliser dans crash_count.
static bool _isCrashReason(esp_reset_reason_t r) {
    switch (r) {
        case ESP_RST_PANIC:
        case ESP_RST_INT_WDT:
        case ESP_RST_TASK_WDT:
        case ESP_RST_WDT:
        case ESP_RST_BROWNOUT:
            return true;
        default:
            return false;
    }
}

static uint32_t _largestFreeBlock() {
    return (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
}

void BootLogModule::begin() {
    esp_reset_reason_t reason = esp_reset_reason();
    bool hadPreviousLines = (_rtc.magic == kMagic);

    uint32_t bootCount  = (uint32_t)config.getInt("bl_boot_count", 0) + 1;
    uint32_t crashCount = (uint32_t)config.getInt("bl_crash_count", 0);
    if (hadPreviousLines && _isCrashReason(reason)) crashCount++;
    config.setInt("bl_boot_count",  (int)bootCount);
    config.setInt("bl_crash_count", (int)crashCount);

    float temperatureC = temperatureRead();

    JsonDocument doc;
    if (storage.exists("/bootlog.json")) {
        String prev = storage.readFile("/bootlog.json");
        DeserializationError err = deserializeJson(doc, prev);
        if (err) doc.to<JsonArray>();
    } else {
        doc.to<JsonArray>();
    }
    JsonArray arr = doc.is<JsonArray>() ? doc.as<JsonArray>() : doc.to<JsonArray>();

    JsonObject entry = arr.add<JsonObject>();
    entry["bootMs"]          = (uint32_t)millis();
    entry["resetReason"]     = _reasonText(reason);
    entry["resetReasonCode"] = (int)reason;
    entry["bootCount"]       = bootCount;
    entry["crashCount"]      = crashCount;
    entry["temperature"]     = temperatureC;

    if (hadPreviousLines) {
        entry["uptimeAtResetMs"]     = _rtc.lastUptimeMs;
        entry["freeHeapAtReset"]     = _rtc.stats.freeHeap;
        entry["largestBlockAtReset"] = _rtc.stats.largestBlock;
        entry["lastTask"]            = _rtc.lastTask[0] ? String(_rtc.lastTask) : String("");
        entry["wifiStatus"]          = (int)_rtc.wifiStatus;
        entry["wifiRssi"]            = (int)_rtc.wifiRssi;
        entry["wifiIp"]              = String(_rtc.wifiIp);

        JsonObject stats = entry["lastStats"].to<JsonObject>();
        stats["uptime"]       = _rtc.stats.uptime;
        stats["freeHeap"]     = _rtc.stats.freeHeap;
        stats["largestBlock"] = _rtc.stats.largestBlock;
        stats["devicesCount"] = _rtc.stats.devicesCount;
        stats["pagesServed"]  = _rtc.stats.pagesServed;
        stats["apiCalls"]     = _rtc.stats.apiCalls;

        JsonArray lines = entry["lines"].to<JsonArray>();
        uint16_t start = (_rtc.count < kLines) ? 0 : _rtc.next;
        for (uint16_t i = 0; i < _rtc.count; i++) {
            uint16_t idx = (start + i) % kLines;
            lines.add(String(_rtc.lines[idx]));
        }
    } else {
        entry["lines"].to<JsonArray>();
    }

    while ((int)arr.size() > MAX_BOOT_LOG_ENTRIES)
        arr.remove(0);

    String out;
    serializeJson(doc, out);
    storage.writeFile("/bootlog.json", out);

    memset(&_rtc, 0, sizeof(_rtc));
    _rtc.magic       = kMagic;
    _rtc.temperatureC = temperatureC;
    _rtc.wifiStatus   = (int8_t)WL_IDLE_STATUS;

    LOG_INFO(name(), "Raison du dernier reset : %s (%d) — boot #%u, crash #%u, %.1f °C",
             _reasonText(reason), (int)reason, (unsigned)bootCount, (unsigned)crashCount, temperatureC);
}

void BootLogModule::loop() {
    if (_rtc.magic != kMagic) return;

    _rtc.lastUptimeMs = millis();

    if (_lastStatsSnapMs != 0 && _rtc.lastUptimeMs - _lastStatsSnapMs < BOOT_LOG_STATS_INTERVAL_MS)
        return;
    _lastStatsSnapMs = _rtc.lastUptimeMs;

    _rtc.stats.uptime       = _rtc.lastUptimeMs;
    _rtc.stats.freeHeap     = ESP.getFreeHeap();
    _rtc.stats.largestBlock = _largestFreeBlock();
    _rtc.stats.devicesCount = _devicesCountProvider ? _devicesCountProvider() : 0;

    _rtc.wifiStatus = (int8_t)WiFi.status();
    _rtc.wifiRssi   = (WiFi.status() == WL_CONNECTED) ? (int8_t)WiFi.RSSI() : 0;
    String ip = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : String("");
    strncpy(_rtc.wifiIp, ip.c_str(), kIpLen - 1);

    _rtc.temperatureC = temperatureRead();
}

void BootLogModule::registerRoutes(WebRouter& router) {
    router.get("/api/bootlog", [this](AsyncWebServerRequest* request) {
        AsyncWebServerResponse* res = request->beginResponse(200, "application/json", getLogJson());
        res->addHeader("Cache-Control", "no-cache");
        request->send(res);
    });
    router.on("/api/bootlog", HTTP_DELETE, [this](AsyncWebServerRequest* request) {
        clear();
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    });

    router.get("/debug", [](AsyncWebServerRequest* request) { WebAssets::send(request, "/debug.html"); });
}

void BootLogModule::capture(const char* level, const char* tag, const char* msg) {
    if (_rtc.magic != kMagic) return;
    char* dst = _rtc.lines[_rtc.next];
    snprintf(dst, kLineLen,
             "{\"t\":%lu,\"lvl\":\"%s\",\"tag\":\"%s\",\"heap\":%u,\"blk\":%u,\"msg\":\"%s\"}",
             (unsigned long)millis(), level, tag,
             (unsigned)ESP.getFreeHeap(), (unsigned)_largestFreeBlock(), msg);
    _rtc.next = (_rtc.next + 1) % kLines;
    if (_rtc.count < kLines) _rtc.count++;
}

void BootLogModule::setLastTask(const String& task) {
    if (_rtc.magic != kMagic) return;
    strncpy(_rtc.lastTask, task.c_str(), kTaskLen - 1);
    _rtc.lastTask[kTaskLen - 1] = '\0';
}

void BootLogModule::setDevicesCountProvider(std::function<uint32_t()> provider) {
    _devicesCountProvider = provider;
}

void BootLogModule::notePageServed() {
    if (_rtc.magic == kMagic) _rtc.stats.pagesServed++;
}

void BootLogModule::noteApiCall() {
    if (_rtc.magic == kMagic) _rtc.stats.apiCalls++;
}

String BootLogModule::getLogJson() const {
    if (!storage.exists("/bootlog.json")) return "[]";
    String json = storage.readFile("/bootlog.json");
    return json.length() ? json : "[]";
}

void BootLogModule::clear() {
    storage.deleteFile("/bootlog.json");
    LOG_INFO(name(), "Journal de démarrage vidé");
}
