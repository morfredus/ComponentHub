#include "app.h"
#include "project_config.h"
#include "../services/log_manager/log_manager.h"
#include "../services/wifi_manager/wifi_manager.h"
#include "../services/mdns_manager/mdns_manager.h"
#include "../services/ota_manager/ota_manager.h"
#include "../services/web_manager/web_manager.h"
#include "../services/storage_manager/storage_manager.h"
#include "../services/config_manager/config_manager.h"
#include "../services/time_manager/time_manager.h"

static const char* TAG = "App";

App app;

static void _onWifiEvent(bool connected) {
    if (!connected) return;
#ifdef ENABLE_MDNS
    mdnsMgr.start(MDNS_HOSTNAME);
#endif
#ifdef ENABLE_OTA
    otaMgr.begin(MDNS_HOSTNAME);
#endif
#ifdef ENABLE_TIME_SYNC
    timeMgr.begin(TIME_ZONE, NTP_SERVER);
#endif
}

void App::begin() {
    LOG_INFO(TAG, "%s v%s — démarrage (build %s %s, commit %s)",
             PROJECT_NAME, PROJECT_VERSION, BUILD_DATE, BUILD_TIME, GIT_COMMIT);

    config.begin("app");
    storage.begin();

#ifdef ENABLE_WIFI
    wifiMgr.begin(_onWifiEvent);
#endif

#ifdef ENABLE_WEB_SERVER
    webMgr.begin(WEB_SERVER_PORT);
    WebRouter router = webMgr.router();
    modules.registerAllRoutes(router);
#endif

    modules.beginAll();

    LOG_INFO(TAG, "Initialisation terminée — %u modules actifs", (unsigned)modules.modules().size());
}

void App::loop() {
#ifdef ENABLE_WIFI
    wifiMgr.loop();
#endif
#ifdef ENABLE_OTA
    otaMgr.loop();
#endif
#ifdef ENABLE_WEB_SERVER
    webMgr.loop();
#endif
    modules.loopAll();
}
