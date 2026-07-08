/**
 * ConfigManager — Persistance clé/valeur générique en NVS (Preferences).
 *
 * Sert de socle à tout réglage utilisateur persistant (ex : luminosité LED,
 * fréquence de surveillance...) sans que chaque module réinvente sa propre
 * gestion NVS. Namespace partagé "app" par défaut.
 */

#pragma once
#include <Arduino.h>

class ConfigManager {
public:
    void begin(const char* nsName = "app");

    String getString(const char* key, const String& def = "") const;
    bool   setString(const char* key, const String& value);

    int  getInt(const char* key, int def = 0) const;
    bool setInt(const char* key, int value);

    bool getBool(const char* key, bool def = false) const;
    bool setBool(const char* key, bool value);

    bool remove(const char* key);

private:
    const char* _ns = "app";
};

extern ConfigManager config;
