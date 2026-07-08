#include "ota_manager.h"
#include <ArduinoOTA.h>
#include <Update.h>
#include "project_config.h"
#include "../log_manager/log_manager.h"

static const char* TAG = "OTA";

OtaManager otaMgr;

void OtaManager::begin(const char* hostname) {
#ifdef ENABLE_OTA
    ArduinoOTA.setHostname(hostname);

    if (!_callbacksRegistered) {
        ArduinoOTA.onStart([]() { LOG_INFO(TAG, "ArduinoOTA: réception du firmware..."); });
        ArduinoOTA.onEnd([]()   { LOG_INFO(TAG, "ArduinoOTA: installation terminée, redémarrage..."); });
        ArduinoOTA.onProgress([](unsigned int p, unsigned int t) {
            LOG_DEBUG(TAG, "Progression : %u%%", t ? (p * 100 / t) : 0);
        });
        ArduinoOTA.onError([](ota_error_t err) {
            LOG_ERROR(TAG, "Erreur OTA [%u]", (unsigned)err);
        });
        _callbacksRegistered = true;
    }

    ArduinoOTA.begin();
    LOG_INFO(TAG, "ArduinoOTA actif (nom réseau : %s)", hostname);
#else
    (void)hostname;
#endif
}

void OtaManager::loop() {
#ifdef ENABLE_OTA
    ArduinoOTA.handle();
#endif
}

void OtaManager::registerRoutes(WebRouter& router) {
    router.withUpload("/api/ota/update", HTTP_POST,
        [](AsyncWebServerRequest* request) {
            AsyncWebServerResponse* res = request->beginResponse(200, "application/json",
                Update.hasError() ? "{\"ok\":false}" : "{\"ok\":true}");
            res->addHeader("Connection", "close");
            request->send(res);
            // Redémarre seulement une fois la réponse envoyée au navigateur.
            request->onDisconnect([]() { delay(200); ESP.restart(); });
        },
        [](AsyncWebServerRequest* request, const String& filename, size_t index, uint8_t* data, size_t len, bool isFinal) {
            (void)request;
            if (index == 0) {
                LOG_INFO(TAG, "Web OTA: début de réception — %s", filename.c_str());
                if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial);
            }
            if (Update.write(data, len) != len) Update.printError(Serial);
            if (isFinal) {
                if (Update.end(true)) {
                    LOG_INFO(TAG, "Web OTA: %u octets écrits — succès", (unsigned)(index + len));
                } else {
                    Update.printError(Serial);
                }
            }
        }
    );
}
