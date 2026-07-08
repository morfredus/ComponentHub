#include "config_manager.h"
#include <Preferences.h>

ConfigManager config;

void ConfigManager::begin(const char* nsName) { _ns = nsName; }

String ConfigManager::getString(const char* key, const String& def) const {
    Preferences prefs;
    prefs.begin(_ns, true);
    String v = prefs.getString(key, def);
    prefs.end();
    return v;
}

bool ConfigManager::setString(const char* key, const String& value) {
    Preferences prefs;
    prefs.begin(_ns, false);
    size_t written = prefs.putString(key, value);
    prefs.end();
    return written == (size_t)value.length();
}

int ConfigManager::getInt(const char* key, int def) const {
    Preferences prefs;
    prefs.begin(_ns, true);
    int v = prefs.getInt(key, def);
    prefs.end();
    return v;
}

bool ConfigManager::setInt(const char* key, int value) {
    Preferences prefs;
    prefs.begin(_ns, false);
    bool ok = prefs.putInt(key, value) == sizeof(int);
    prefs.end();
    return ok;
}

bool ConfigManager::getBool(const char* key, bool def) const {
    Preferences prefs;
    prefs.begin(_ns, true);
    bool v = prefs.getBool(key, def);
    prefs.end();
    return v;
}

bool ConfigManager::setBool(const char* key, bool value) {
    Preferences prefs;
    prefs.begin(_ns, false);
    bool ok = prefs.putBool(key, value) == sizeof(bool);
    prefs.end();
    return ok;
}

bool ConfigManager::remove(const char* key) {
    Preferences prefs;
    prefs.begin(_ns, false);
    bool ok = prefs.remove(key);
    prefs.end();
    return ok;
}
