#include "HttpClient.h"

#include <cstring>
#include <cstdlib>
#include <sstream>

#if defined(_WIN32)
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <winsock2.h>
  #include <ws2tcpip.h>
  using socket_t = SOCKET;
  static const socket_t kInvalidSocket = INVALID_SOCKET;
  static void closeSocket(socket_t s) { closesocket(s); }
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <netdb.h>
  #include <unistd.h>
  #include <arpa/inet.h>
  using socket_t = int;
  static const socket_t kInvalidSocket = -1;
  static void closeSocket(socket_t s) { ::close(s); }
#endif

namespace chnet {
namespace {

#if defined(_WIN32)
struct WsaInit {
    WsaInit()  { WSADATA w; WSAStartup(MAKEWORD(2, 2), &w); }
    ~WsaInit() { WSACleanup(); }
};
#endif
void ensureNetwork() {
#if defined(_WIN32)
    static WsaInit init; // initialise winsock une fois par process
    (void)init;
#endif
}

// Décompose http://hôte:port/chemin. Retourne false si l'URL n'est pas http.
bool parseUrl(const std::string& url, std::string& host, std::string& port, std::string& path) {
    const std::string scheme = "http://";
    if (url.compare(0, scheme.size(), scheme) != 0) return false;
    std::string rest = url.substr(scheme.size());

    std::size_t slash = rest.find('/');
    std::string hostport = (slash == std::string::npos) ? rest : rest.substr(0, slash);
    path = (slash == std::string::npos) ? "/" : rest.substr(slash);

    std::size_t colon = hostport.find(':');
    if (colon == std::string::npos) { host = hostport; port = "80"; }
    else { host = hostport.substr(0, colon); port = hostport.substr(colon + 1); }
    return !host.empty();
}

void applyTimeout(socket_t fd, int timeoutMs) {
    if (timeoutMs <= 0) timeoutMs = 5000;
#if defined(_WIN32)
    DWORD tv = static_cast<DWORD>(timeoutMs);
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv));
#else
    timeval tv{timeoutMs / 1000, (timeoutMs % 1000) * 1000};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#endif
}

HttpResult request(const std::string& method, const std::string& url, const std::string& body,
                   const std::map<std::string, std::string>& headers, int timeoutMs) {
    HttpResult r;
    ensureNetwork();

    std::string host, port, path;
    if (!parseUrl(url, host, port, path)) { r.error = "URL invalide (http:// attendu) : " + url; return r; }

    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* res = nullptr;
    if (getaddrinfo(host.c_str(), port.c_str(), &hints, &res) != 0 || !res) {
        r.error = "hôte introuvable : " + host + ":" + port;
        return r;
    }

    socket_t fd = ::socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd == kInvalidSocket) { freeaddrinfo(res); r.error = "socket() a échoué"; return r; }
    applyTimeout(fd, timeoutMs);

    if (::connect(fd, res->ai_addr, static_cast<int>(res->ai_addrlen)) != 0) {
        freeaddrinfo(res);
        closeSocket(fd);
        r.error = "connexion refusée à " + host + ":" + port + " (hub arrêté ?)";
        return r;
    }
    freeaddrinfo(res);

    // Construit la requête. Connection: close -> le serveur ferme après la
    // réponse, on lit donc jusqu'à la fermeture.
    std::ostringstream req;
    req << method << ' ' << path << " HTTP/1.1\r\n"
        << "Host: " << host << ':' << port << "\r\n"
        << "Connection: close\r\n";
    for (const auto& [k, v] : headers) req << k << ": " << v << "\r\n";
    if (method == "POST") {
        req << "Content-Type: application/json; charset=utf-8\r\n"
            << "Content-Length: " << body.size() << "\r\n";
    }
    req << "\r\n";
    if (method == "POST") req << body;

    const std::string payload = req.str();
    std::size_t sent = 0;
    while (sent < payload.size()) {
        int n = ::send(fd, payload.data() + sent, static_cast<int>(payload.size() - sent), 0);
        if (n <= 0) { closeSocket(fd); r.error = "échec d'envoi de la requête"; return r; }
        sent += static_cast<std::size_t>(n);
    }

    // Lit toute la réponse.
    std::string raw;
    char buf[4096];
    for (;;) {
        int n = ::recv(fd, buf, sizeof(buf), 0);
        if (n <= 0) break;
        raw.append(buf, static_cast<std::size_t>(n));
    }
    closeSocket(fd);

    if (raw.empty()) { r.error = "réponse vide (délai dépassé ?)"; return r; }

    // Statut : "HTTP/1.1 200 OK".
    std::size_t sp = raw.find(' ');
    if (sp != std::string::npos) r.status = std::atoi(raw.c_str() + sp + 1);

    std::size_t hdrEnd = raw.find("\r\n\r\n");
    r.body = (hdrEnd == std::string::npos) ? "" : raw.substr(hdrEnd + 4);
    r.ok = (r.status != 0);
    if (!r.ok) r.error = "réponse HTTP illisible";
    return r;
}

} // namespace

HttpResult httpGet(const std::string& url, const std::map<std::string, std::string>& headers, int timeoutMs) {
    return request("GET", url, "", headers, timeoutMs);
}
HttpResult httpPost(const std::string& url, const std::string& body,
                    const std::map<std::string, std::string>& headers, int timeoutMs) {
    return request("POST", url, body, headers, timeoutMs);
}

} // namespace chnet
