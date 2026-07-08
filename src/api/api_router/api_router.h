/**
 * WebRouter — Façade de routage HTTP exposée aux modules métier.
 *
 * Découple les modules de l'implémentation concrète du serveur HTTP
 * (ESPAsyncWebServer) : un module appelle router.get/post/on/withBody/
 * withUpload, sans jamais connaître WebManager. Les handlers reçoivent un
 * AsyncWebServerRequest*, seul point où le choix "asynchrone" transparaît —
 * un remplacement futur du serveur devra encore réécrire les modules, mais
 * jamais le core ni les services (voir docs/ARCHITECTURE.md).
 */

#pragma once
#include <ESPAsyncWebServer.h>
#include <functional>

class WebRouter {
public:
    explicit WebRouter(AsyncWebServer& server) : _server(server) {}

    using Handler        = std::function<void(AsyncWebServerRequest*)>;
    using BodyHandler     = std::function<void(AsyncWebServerRequest*, const String& body)>;
    using UploadHandler   = std::function<void(AsyncWebServerRequest*, const String& filename,
                                                size_t index, uint8_t* data, size_t len, bool isFinal)>;

    void on(const String& path, WebRequestMethod method, Handler handler) {
        _server.on(path.c_str(), method, [handler](AsyncWebServerRequest* r) { handler(r); });
    }

    void get(const String& path, Handler handler)  { on(path, HTTP_GET, handler); }
    void post(const String& path, Handler handler) { on(path, HTTP_POST, handler); }

    // Route avec accès au corps brut (JSON) : accumule les éventuels chunks
    // puis appelle `handler` une seule fois, corps complet en main.
    void withBody(const String& path, WebRequestMethod method, BodyHandler handler) {
        _server.on(path.c_str(), method,
            [](AsyncWebServerRequest*) {},  // la réponse est envoyée depuis onBody, une fois le corps reçu
            nullptr,
            [handler](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
                String* body = static_cast<String*>(request->_tempObject);
                if (!body) {
                    body = new String();
                    body->reserve(total);
                    request->_tempObject = body;
                }
                body->concat(reinterpret_cast<const char*>(data), len);
                if (index + len == total) {
                    handler(request, *body);
                    delete body;
                    request->_tempObject = nullptr;
                }
            });
    }

    // Route d'upload de fichier (multipart) : `onUpload` reçoit chaque chunk,
    // `onDone` envoie la réponse finale une fois l'upload terminé.
    void withUpload(const String& path, WebRequestMethod method, Handler onDone, UploadHandler onUpload) {
        _server.on(path.c_str(), method,
            [onDone](AsyncWebServerRequest* request) { onDone(request); },
            [onUpload](AsyncWebServerRequest* request, const String& filename, size_t index,
                       uint8_t* data, size_t len, bool isFinal) {
                onUpload(request, filename, index, data, len, isFinal);
            });
    }

    AsyncWebServer& raw() { return _server; }

private:
    AsyncWebServer& _server;
};
