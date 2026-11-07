/**
 * HttpClient - client HTTP/1.1 minimal, bloquant, sans dépendance externe.
 *
 * Pendant client du serveur de morfSync : même philosophie (pas de TLS, LAN
 * privé, on écrit les deux bouts). Portable Windows (winsock2) / Linux / ARM64
 * (sockets POSIX). Aucune dépendance à Qt : réutilisable et testable dans le
 * cœur métier (componenthub_core).
 */

#pragma once
#include <atomic>
#include <string>
#include <map>

namespace chnet {

struct HttpResult {
    bool ok = false;          // connexion + échange HTTP menés à bien (statut lu)
    int status = 0;           // code HTTP (200, 404…) si ok
    std::string body;         // corps de la réponse
    std::string error;        // message si !ok (réseau injoignable, URL invalide…)
};

// Associe au THREAD APPELANT un indicateur d'annulation (nullptr = aucun). Quand il passe à
// vrai, les requêtes en cours de ce thread s'interrompent en quelques centaines de ms avec
// l'erreur « http.cancelled ». L'indicateur doit survivre au thread ou être retiré avant.
void setThreadCancelFlag(const std::atomic<bool>* flag);

// GET/POST synchrones. `url` de la forme http://hôte:port/chemin (pas de https).
// `headers` : en-têtes additionnels (ex. Authorization). `timeoutMs` : délai de
// connexion (0 = 5 s) et par lecture ; un délai TOTAL de 60 s et un plafond de
// 128 Mio par réponse bornent en plus chaque requête.
HttpResult httpGet(const std::string& url,
                   const std::map<std::string, std::string>& headers = {},
                   int timeoutMs = 0);

HttpResult httpPost(const std::string& url, const std::string& body,
                    const std::map<std::string, std::string>& headers = {},
                    int timeoutMs = 0);

// PUT avec un corps binaire (Content-Type: application/octet-stream). Sert à
// téléverser un blob de pièce jointe vers le magasin de morfSync.
HttpResult httpPut(const std::string& url, const std::string& body,
                   const std::map<std::string, std::string>& headers = {},
                   int timeoutMs = 0);

// HEAD : présence sans corps. Sert à savoir si le hub détient déjà un blob avant
// de le téléverser.
HttpResult httpHead(const std::string& url,
                    const std::map<std::string, std::string>& headers = {},
                    int timeoutMs = 0);

// Fonctions pures exposées pour les tests (ctest) - non destinées aux appelants.
// Décode un corps Transfer-Encoding: chunked. Retourne false si le flux est tronqué ou invalide.
bool decodeChunked(const std::string& in, std::string& out);
// Analyse une réponse HTTP brute (statut, corps, chunked) en complétant `r`.
HttpResult parseResponse(const std::string& raw, HttpResult r = HttpResult());

} // namespace chnet
