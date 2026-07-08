#include "log_manager.h"
#include "project_config.h"
#ifdef ENABLE_BOOT_LOG
#include "../../modules/boot_log/boot_log.h"
#endif

LogManager logMgr;

static const char* _levelName(char level) {
    switch (level) {
        case 'D': return "DBG";
        case 'I': return "INF";
        case 'W': return "WRN";
        case 'E': return "ERR";
        default:  return "???";
    }
}

void LogManager::log(char level, const char* tag, const char* fmt, va_list args) {
    char buf[256];
    vsnprintf(buf, sizeof(buf), fmt, args);

    Serial.printf("[%s][%s] %s\n", _levelName(level), tag, buf);

#ifdef ENABLE_BOOT_LOG
    bootLogModule.capture(_levelName(level), tag, buf);
#endif

    if (_entries.size() >= LOG_BUFFER_SIZE) {
        _entries.erase(_entries.begin());
    }
    _entries.push_back(LogEntry{millis(), level, String(tag), String(buf)});
}

String LogManager::toJson() const {
    String json = "[";
    for (size_t i = 0; i < _entries.size(); i++) {
        const LogEntry& e = _entries[i];
        if (i) json += ",";
        json += "{\"t\":";
        json += e.timestampMs;
        json += ",\"level\":\"";
        json += _levelName(e.level);
        json += "\",\"tag\":\"";
        json += e.tag;
        json += "\",\"msg\":\"";
        for (size_t c = 0; c < e.message.length(); c++) {
            char ch = e.message[c];
            if (ch == '"' || ch == '\\') json += '\\';
            if (ch == '\n') { json += "\\n"; continue; }
            json += ch;
        }
        json += "\"}";
    }
    json += "]";
    return json;
}
