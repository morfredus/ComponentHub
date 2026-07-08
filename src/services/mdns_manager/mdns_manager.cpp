#include "mdns_manager.h"
#include <ESPmDNS.h>
#include "../log_manager/log_manager.h"

static const char* TAG = "mDNS";

MdnsManager mdnsMgr;

bool MdnsManager::start(const char* hostname) {
    // start() est rappelé à chaque (re)connexion WiFi. Sans MDNS.end()
    // préalable, un second MDNS.begin() peut renvoyer true tout en laissant
    // le répondeur dans un état incohérent (nom plus annoncé sur le réseau) :
    // on repart donc systématiquement d'un état propre.
    if (_active) MDNS.end();

    if (MDNS.begin(hostname)) {
        MDNS.setInstanceName(hostname);
        MDNS.addService("http", "tcp", 80);
        _active = true;
        LOG_INFO(TAG, "mDNS actif : http://%s.local", hostname);
        return true;
    }
    _active = false;
    LOG_ERROR(TAG, "Échec démarrage mDNS");
    return false;
}
