#include "system_info.h"
#include <ArduinoJson.h>
#include <WiFi.h>
#include <LittleFS.h>
#include "project_config.h"
#include "../wifi_manager/wifi_manager.h"

SystemInfo systemInfo;

String SystemInfo::getJson() const {
    JsonDocument doc;
    doc["project"]       = PROJECT_NAME;
    doc["version"]       = PROJECT_VERSION;
    doc["build_date"]    = BUILD_DATE;
    doc["build_time"]    = BUILD_TIME;
    doc["git_commit"]    = GIT_COMMIT;
    doc["uptime"]        = millis() / 1000;
    doc["free_heap"]     = ESP.getFreeHeap();
    doc["chip_model"]    = ESP.getChipModel();
    doc["cpu_freq_mhz"]  = ESP.getCpuFreqMHz();
    doc["wifi"]          = wifiMgr.isConnected() ? wifiMgr.ssid() : "disconnected";
    doc["rssi"]          = wifiMgr.rssi();
    doc["ip"]            = wifiMgr.localIP();

    if (psramFound()) {
        doc["free_psram"]  = ESP.getFreePsram();
        doc["total_psram"] = ESP.getPsramSize();
    }

    JsonObject fs = doc["littlefs"].to<JsonObject>();
    fs["total"] = LittleFS.totalBytes();
    fs["used"]  = LittleFS.usedBytes();

    String json;
    serializeJson(doc, json);
    return json;
}
