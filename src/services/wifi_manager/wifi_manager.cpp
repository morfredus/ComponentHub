#include "wifi_manager.h"
#include <algorithm>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <Preferences.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include "project_config.h"
#include "../log_manager/log_manager.h"

#if __has_include("secrets.h")
#include "secrets.h"
#endif

static const char* TAG           = "WiFi";
static const char* NVS_NAMESPACE = "wifi";
static const char* NVS_KEY       = "networks";
static const IPAddress AP_IP(192, 168, 4, 1);

static WiFiMulti _multi;
static constexpr unsigned long RECONNECT_DEBOUNCE_MS = 30000;
static unsigned long _lastReconnectAttempt = 0;
static WiFiManager::Callback _storedCb;
static bool _wasConnected = false;
static bool _apMode = false;

static DNSServer _dns;
static WebServer _portal(80);

WiFiManager wifiMgr;

static std::vector<WifiCredential> _loadNetworks() {
    std::vector<WifiCredential> list;
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, true);
    String json = prefs.getString(NVS_KEY, "[]");
    prefs.end();

    JsonDocument doc;
    if (deserializeJson(doc, json) == DeserializationError::Ok) {
        for (JsonObject o : doc.as<JsonArray>()) {
            WifiCredential c;
            c.ssid     = o["ssid"].as<String>();
            c.password = o["password"].as<String>();
            if (!c.ssid.isEmpty()) list.push_back(c);
        }
    }
    return list;
}

static bool _saveNetworks(const std::vector<WifiCredential>& list) {
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    for (const auto& c : list) {
        JsonObject o = arr.add<JsonObject>();
        o["ssid"]     = c.ssid;
        o["password"] = c.password;
    }
    String json;
    serializeJson(doc, json);

    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false);
    size_t written = prefs.putString(NVS_KEY, json);
    prefs.end();
    return written == (size_t)json.length();
}

std::vector<WifiCredential> WiFiManager::savedNetworks() const { return _loadNetworks(); }

bool WiFiManager::addNetwork(const String& ssid, const String& password) {
    if (ssid.isEmpty()) return false;
    auto list = _loadNetworks();
    for (auto& c : list) {
        if (c.ssid == ssid) {
            c.password = password;
            return _saveNetworks(list);
        }
    }
    list.push_back({ssid, password});
    return _saveNetworks(list);
}

bool WiFiManager::removeNetwork(const String& ssid) {
    auto list = _loadNetworks();
    size_t before = list.size();
    list.erase(std::remove_if(list.begin(), list.end(),
        [&](const WifiCredential& c) { return c.ssid == ssid; }), list.end());
    if (list.size() == before) return false;
    return _saveNetworks(list);
}

// Page de configuration autonome (CSS inline) — servie avant toute connexion
// réseau, donc indépendante du pipeline web habituel (WebAssets + WebManager).
static const char PORTAL_PAGE[] PROGMEM = R"HTML(
<!DOCTYPE html><html lang="fr"><head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Configuration WiFi</title>
<style>
body{font-family:system-ui,sans-serif;background:#0f172a;color:#e2e8f0;display:flex;
  flex-direction:column;align-items:center;padding:2rem 1rem;margin:0}
.card{background:#1e293b;border-radius:12px;padding:1.5rem;max-width:380px;width:100%}
h1{font-size:1.05rem;margin:0 0 0.4rem}
p.hint{font-size:0.8rem;color:#94a3b8;margin-bottom:1rem}
label{display:block;margin:0.8rem 0 0.3rem;font-size:0.85rem;color:#94a3b8}
select,input{width:100%;padding:0.6rem;border-radius:6px;border:1px solid #334155;
  background:#0f172a;color:#e2e8f0;box-sizing:border-box;font-size:0.9rem}
button{width:100%;margin-top:1.2rem;padding:0.7rem;border:none;border-radius:6px;
  background:#3b82f6;color:#fff;font-weight:600;cursor:pointer;font-size:0.95rem}
button:hover{background:#2563eb}
#msg{margin-top:1rem;font-size:0.85rem;text-align:center;min-height:1.1rem}
</style></head><body>
<div class="card">
  <h1>Configuration WiFi</h1>
  <p class="hint">Choisissez votre réseau WiFi. L'appareil redémarrera et s'y connectera automatiquement.</p>
  <form id="f">
    <label>Réseau WiFi détecté</label>
    <select id="ssidSel"><option value="">Recherche en cours...</option></select>
    <label>Ou saisissez le SSID manuellement</label>
    <input id="ssidManual" placeholder="Nom du réseau (SSID)">
    <label>Mot de passe</label>
    <input id="password" type="password" placeholder="Mot de passe WiFi">
    <button type="submit">Enregistrer et connecter</button>
  </form>
  <p id="msg"></p>
</div>
<script>
fetch('/scan').then(function(r){return r.json();}).then(function(list){
  var sel=document.getElementById('ssidSel');
  sel.innerHTML='<option value="">-- Choisir un réseau --</option>';
  list.forEach(function(n){
    var o=document.createElement('option');
    o.value=n.ssid; o.textContent=n.ssid+' ('+n.rssi+' dBm)';
    sel.appendChild(o);
  });
}).catch(function(){
  document.getElementById('ssidSel').innerHTML='<option value="">Aucun réseau détecté</option>';
});
document.getElementById('f').addEventListener('submit',function(e){
  e.preventDefault();
  var ssid=document.getElementById('ssidManual').value || document.getElementById('ssidSel').value;
  var password=document.getElementById('password').value;
  var msg=document.getElementById('msg');
  if(!ssid){ msg.textContent='Veuillez choisir ou saisir un réseau.'; return; }
  msg.textContent='Enregistrement...';
  var fd=new FormData(); fd.append('ssid',ssid); fd.append('password',password);
  fetch('/save',{method:'POST',body:fd}).then(function(r){return r.json();}).then(function(d){
    msg.textContent = d.status==='ok'
      ? 'Enregistré - redémarrage...'
      : 'Erreur : '+(d.error||'inconnue');
  }).catch(function(){ msg.textContent='Erreur réseau.'; });
});
</script></body></html>
)HTML";

static void _portalHandleRoot() { _portal.send_P(200, "text/html", PORTAL_PAGE); }

static void _portalHandleScan() {
    int n = WiFi.scanNetworks();
    String json = "[";
    for (int i = 0; i < n; i++) {
        if (i) json += ",";
        json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",\"rssi\":" + String(WiFi.RSSI(i)) + "}";
    }
    json += "]";
    _portal.send(200, "application/json", json);
}

static void _portalHandleSave() {
    String ssid     = _portal.arg("ssid");
    String password = _portal.arg("password");
    if (ssid.isEmpty()) {
        _portal.send(400, "application/json", "{\"error\":\"ssid requis\"}");
        return;
    }
    wifiMgr.addNetwork(ssid, password);
    _portal.send(200, "application/json", "{\"status\":\"ok\"}");
    LOG_INFO(TAG, "Réseau \"%s\" enregistré via le portail — redémarrage", ssid.c_str());
    delay(500);
    ESP.restart();
}

static void _portalHandleNotFound() {
    _portal.sendHeader("Location", String("http://") + AP_IP.toString(), true);
    _portal.send(302, "text/plain", "");
}

static void _startPortal() {
    _apMode = true;
    WiFi.mode(WIFI_AP);
    WiFi.softAP(WIFI_PORTAL_AP_NAME);
    delay(100);
    WiFi.softAPConfig(AP_IP, AP_IP, IPAddress(255, 255, 255, 0));

    _dns.start(53, "*", AP_IP);

    _portal.on("/",     HTTP_GET,  _portalHandleRoot);
    _portal.on("/scan", HTTP_GET,  _portalHandleScan);
    _portal.on("/save", HTTP_POST, _portalHandleSave);
    _portal.onNotFound(_portalHandleNotFound);
    _portal.begin();

    LOG_INFO(TAG, "Portail de configuration actif — SSID \"%s\" — http://%s",
             WIFI_PORTAL_AP_NAME, AP_IP.toString().c_str());
}

static bool _tryConnect(const std::vector<WifiCredential>& networks) {
    WiFi.mode(WIFI_STA);
    for (const auto& c : networks) {
        _multi.addAP(c.ssid.c_str(), c.password.c_str());
    }

    LOG_INFO(TAG, "Connexion en cours...");
    unsigned long start = millis();
    while (_multi.run() != WL_CONNECTED &&
           millis() - start < WIFI_CONNECT_TIMEOUT_MS) {
        delay(500);
    }
    return WiFi.isConnected();
}

void WiFiManager::begin(Callback cb) {
    _storedCb = cb;

    std::vector<WifiCredential> networks = _loadNetworks();

#ifdef WIFI_DEFAULT_SSID
    if (networks.empty()) {
        networks.push_back({WIFI_DEFAULT_SSID, WIFI_DEFAULT_PASSWORD});
        LOG_INFO(TAG, "Aucun réseau en NVS — utilisation de WIFI_DEFAULT_SSID (secrets.h)");
    }
#endif

    if (!networks.empty() && _tryConnect(networks)) {
        LOG_INFO(TAG, "Connecté à \"%s\" — IP %s — RSSI %d dBm",
                 WiFi.SSID().c_str(), WiFi.localIP().toString().c_str(), WiFi.RSSI());
        _wasConnected = true;
        if (cb) cb(true);
        return;
    }

    LOG_WARNING(TAG, "Aucun réseau disponible — démarrage du portail de configuration");
    if (cb) cb(false);
    _startPortal();
}

void WiFiManager::loop() {
    if (_apMode) {
        _dns.processNextRequest();
        _portal.handleClient();
        return;
    }

    bool connected = WiFi.isConnected();

    if (!connected) {
        unsigned long now = millis();
        if (now - _lastReconnectAttempt >= RECONNECT_DEBOUNCE_MS) {
            _lastReconnectAttempt = now;
            LOG_WARNING(TAG, "WiFi perdu — tentative de reconnexion...");
            _multi.run();
        }
    } else if (!_wasConnected) {
        LOG_INFO(TAG, "WiFi rétabli sur \"%s\" — IP %s",
                 WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
        if (_storedCb) _storedCb(true);
    }

    _wasConnected = connected;
}

bool   WiFiManager::isConnected() const { return !_apMode && WiFi.isConnected(); }
bool   WiFiManager::isApMode()    const { return _apMode; }
String WiFiManager::ssid()        const { return WiFi.SSID(); }
String WiFiManager::localIP()     const {
    return _apMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
}
int8_t WiFiManager::rssi()        const { return (int8_t)WiFi.RSSI(); }
