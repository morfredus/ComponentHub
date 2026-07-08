/**
 * WiFiManager — Connexion WiFi multi-réseaux, NVS, portail captif, mDNS.
 * (généralisé depuis Gateway Lab V1)
 *
 * Hiérarchie de configuration (priorité décroissante) :
 *   1. Réseaux enregistrés en NVS (Preferences, namespace "wifi")
 *   2. WIFI_DEFAULT_SSID / WIFI_DEFAULT_PASSWORD dans include/secrets.h (dev)
 *   3. Portail de configuration (point d'accès AP_NAME, voir project_config.h)
 *
 * begin() tente la connexion selon la hiérarchie ci-dessus. Si aucun réseau
 * ne répond, un point d'accès + portail captif sont démarrés. loop() doit
 * être appelé sans interruption : il surveille la connexion (mode normal)
 * ou sert le portail (mode point d'accès).
 */

#pragma once
#include <Arduino.h>
#include <functional>
#include <vector>

struct WifiCredential {
    String ssid;
    String password;
};

class WiFiManager {
public:
    using Callback = std::function<void(bool connected)>;

    // Connexion initiale, bloquante jusqu'à WIFI_CONNECT_TIMEOUT_MS par réseau
    // connu. Si aucun réseau ne répond, démarre le portail de configuration.
    void begin(Callback cb = nullptr);

    // Surveillance de la connexion (mode normal) ou du portail (mode AP).
    void loop();

    bool    isConnected() const;
    bool    isApMode()    const;
    String  ssid()        const;
    String  localIP()     const;
    int8_t  rssi()         const;

    std::vector<WifiCredential> savedNetworks() const;
    bool addNetwork(const String& ssid, const String& password);
    bool removeNetwork(const String& ssid);
};

extern WiFiManager wifiMgr;
