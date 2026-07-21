/**
 * HttpClient — client HTTP/1.1 minimal, bloquant, sans dépendance externe.
 *
 * Pendant client du serveur de morfSync : même philosophie (pas de TLS, LAN
 * privé, on écrit les deux bouts). Portable Windows (winsock2) / Linux / ARM64
 * (sockets POSIX). Aucune dépendance à Qt : réutilisable et testable dans le
 * cœur métier (componenthub_core).
 */

#pragma once
#include <string>
#include <map>

namespace chnet {

struct HttpResult {
    bool ok = false;          // connexion + échange HTTP menés à bien (statut lu)
    int status = 0;           // code HTTP (200, 404…) si ok
    std::string body;         // corps de la réponse
    std::string error;        // message si !ok (réseau injoignable, URL invalide…)
};

// GET/POST synchrones. `url` de la forme http://hôte:port/chemin (pas de https).
// `headers` : en-têtes additionnels (ex. Authorization). `timeoutMs` : délai de
// connexion/lecture (0 = défaut raisonnable).
HttpResult httpGet(const std::string& url,
                   const std::map<std::string, std::string>& headers = {},
                   int timeoutMs = 0);

HttpResult httpPost(const std::string& url, const std::string& body,
                    const std::map<std::string, std::string>& headers = {},
                    int timeoutMs = 0);

} // namespace chnet
