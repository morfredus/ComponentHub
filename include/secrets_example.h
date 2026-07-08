#pragma once

// Dupliquez ce fichier en secrets.h pour le développement local.
// include/secrets.h est ignoré par Git — ne le committez jamais.
//
// Le portail de configuration WiFi (voir WiFiManager) est la méthode
// officielle de configuration : ces valeurs ne servent qu'en développement,
// et uniquement si aucun réseau n'est encore enregistré en NVS.

#define WIFI_DEFAULT_SSID     "MonWifi"
#define WIFI_DEFAULT_PASSWORD "MonMotDePasse"
