#include "HttpClient.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <sstream>

#if defined(_WIN32)
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <winsock2.h>
  #include <ws2tcpip.h>
  static bool setNonBlocking(SOCKET s, bool on) { u_long m = on ? 1 : 0; return ioctlsocket(s, FIONBIO, &m) == 0; }
  using socket_t = SOCKET;
  static const socket_t kInvalidSocket = INVALID_SOCKET;
  static void closeSocket(socket_t s) { closesocket(s); }
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <netdb.h>
  #include <unistd.h>
  #include <arpa/inet.h>
  #include <fcntl.h>
  #include <sys/select.h>
  static bool setNonBlocking(int s, bool on) {
      int f = fcntl(s, F_GETFL, 0);
      if (f < 0) return false;
      return fcntl(s, F_SETFL, on ? (f | O_NONBLOCK) : (f & ~O_NONBLOCK)) == 0;
  }
  using socket_t = int;
  static const socket_t kInvalidSocket = -1;
  static void closeSocket(socket_t s) { ::close(s); }
#endif

namespace chnet {
namespace {

using Clock = std::chrono::steady_clock;

// Cancellation flag of the current thread (see setThreadCancelFlag). thread_local: the
// synchronization runs in a dedicated thread, and cancelling it must not affect the
// requests that another thread may be running.
thread_local const std::atomic<bool>* t_cancel = nullptr;
bool cancelled() { return t_cancel && t_cancel->load(std::memory_order_relaxed); }

// Wait slice: the maximum time between two checks of the cancellation flag.
constexpr int kSliceMs = 200;

// Safety caps: a failing hub (or something other than a hub on the port) must
// neither freeze the application indefinitely nor make it swallow an endless response.
constexpr int kTotalTimeoutMs = 60000;                          // TOTAL time limit of a request
constexpr std::size_t kMaxResponseBytes = 128u * 1024u * 1024u; // attachment blobs included

// Connection with timeout: a blocking connect() can take tens of seconds on a
// host that does not answer. Switch to non-blocking + select, then back to blocking mode.
bool connectWithTimeout(socket_t fd, const sockaddr* addr, int len, int timeoutMs) {
    if (!setNonBlocking(fd, true)) return ::connect(fd, addr, len) == 0;
    int rc = ::connect(fd, addr, len);
    if (rc != 0) {
#if defined(_WIN32)
        const bool pending = (WSAGetLastError() == WSAEWOULDBLOCK);
#else
        const bool pending = (errno == EINPROGRESS);
#endif
        if (!pending) return false;
        // Waiting by slices: cancellation is tested between two slices.
        fd_set wr, ex;
        const auto until = Clock::now() + std::chrono::milliseconds(timeoutMs);
        for (;;) {
            if (cancelled() || Clock::now() >= until) return false;
            FD_ZERO(&wr); FD_ZERO(&ex);
            FD_SET(fd, &wr); FD_SET(fd, &ex);
            timeval tv{0, kSliceMs * 1000};
            const int ready = ::select(static_cast<int>(fd) + 1, nullptr, &wr, &ex, &tv);
            if (ready < 0) return false;
            if (ready > 0) break;
        }
        if (FD_ISSET(fd, &ex)) return false;
        int err = 0;
#if defined(_WIN32)
        int elen = sizeof(err);
        getsockopt(fd, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&err), &elen);
#else
        socklen_t elen = sizeof(err);
        getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &elen);
#endif
        if (err != 0) return false;
    }
    setNonBlocking(fd, false);
    return true;
}

std::string lowerCopy(std::string v) {
    std::transform(v.begin(), v.end(), v.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return v;
}

#if defined(_WIN32)
struct WsaInit {
    WsaInit()  { WSADATA w; WSAStartup(MAKEWORD(2, 2), &w); }
    ~WsaInit() { WSACleanup(); }
};
#endif
void ensureNetwork() {
#if defined(_WIN32)
    static WsaInit init; // initialize winsock once per process
    (void)init;
#endif
}

// Splits http://host:port/path. Returns false if the URL is not http.
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

// Send timeout = `timeoutMs`; reading is cut into slices of kSliceMs (the total
// inactivity timeout is tracked per request) so that a cancellation is noticed quickly.
void applyTimeout(socket_t fd, int timeoutMs) {
    if (timeoutMs <= 0) timeoutMs = 5000;
#if defined(_WIN32)
    DWORD snd = static_cast<DWORD>(timeoutMs);
    DWORD rcv = static_cast<DWORD>(std::min(timeoutMs, kSliceMs));
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&rcv), sizeof(rcv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&snd), sizeof(snd));
#else
    const int rcvMs = std::min(timeoutMs, kSliceMs);
    timeval rcv{rcvMs / 1000, (rcvMs % 1000) * 1000};
    timeval snd{timeoutMs / 1000, (timeoutMs % 1000) * 1000};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &rcv, sizeof(rcv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &snd, sizeof(snd));
#endif
}

// True if the last recv failed because of a timeout (slice elapsed), not a disconnection.
bool lastRecvTimedOut() {
#if defined(_WIN32)
    const int e = WSAGetLastError();
    return e == WSAETIMEDOUT || e == WSAEWOULDBLOCK;
#else
    return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR;
#endif
}

HttpResult request(const std::string& method, const std::string& url, const std::string& body,
                   const std::map<std::string, std::string>& headers, int timeoutMs,
                   const std::string& contentType = "application/json; charset=utf-8") {
    HttpResult r;
    ensureNetwork();

    std::string host, port, path;
    if (!parseUrl(url, host, port, path)) { r.error = "http.invalid_url|" + url; return r; }

    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* res = nullptr;
    if (getaddrinfo(host.c_str(), port.c_str(), &hints, &res) != 0 || !res) {
        r.error = "http.host_not_found|" + host + ":" + port;
        return r;
    }

    socket_t fd = ::socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd == kInvalidSocket) { freeaddrinfo(res); r.error = "http.socket_failed"; return r; }
    applyTimeout(fd, timeoutMs);

    if (!connectWithTimeout(fd, res->ai_addr, static_cast<int>(res->ai_addrlen), timeoutMs > 0 ? timeoutMs : 5000)) {
        freeaddrinfo(res);
        closeSocket(fd);
        r.error = cancelled() ? "http.cancelled" : "http.connection_refused|" + host + ":" + port;
        return r;
    }
    freeaddrinfo(res);

    // Builds the request. Connection: close -> the server closes after the
    // response, so we read until the connection is closed.
    std::ostringstream req;
    req << method << ' ' << path << " HTTP/1.1\r\n"
        << "Host: " << host << ':' << port << "\r\n"
        << "Connection: close\r\n";
    for (const auto& [k, v] : headers) req << k << ": " << v << "\r\n";
    // POST and PUT carry a body (synchronization JSON for one, the binary of a blob
    // for the other). Content-Length is exact to the byte: the body is written
    // verbatim, with no assumption about its content.
    const bool hasBody = (method == "POST" || method == "PUT");
    if (hasBody) {
        req << "Content-Type: " << contentType << "\r\n"
            << "Content-Length: " << body.size() << "\r\n";
    }
    req << "\r\n";
    if (hasBody) req << body;

    const auto deadline = Clock::now() + std::chrono::milliseconds(kTotalTimeoutMs);
    const std::string payload = req.str();
    std::size_t sent = 0;
    while (sent < payload.size()) {
        if (cancelled()) { closeSocket(fd); r.error = "http.cancelled"; return r; }
        if (Clock::now() > deadline) { closeSocket(fd); r.error = "http.timeout"; return r; }
        int n = ::send(fd, payload.data() + sent, static_cast<int>(payload.size() - sent), 0);
        if (n <= 0) { closeSocket(fd); r.error = "http.send_failed"; return r; }
        sent += static_cast<std::size_t>(n);
    }

    // Reads the whole response. Each recv lasts at most one slice: between two, cancellation and
    // the inactivity timeout (`idleMs`, as before: SO_RCVTIMEO) are checked.
    std::string raw;
    char buf[4096];
    const int idleMs = timeoutMs > 0 ? timeoutMs : 5000;
    auto lastData = Clock::now();
    for (;;) {
        if (cancelled()) { closeSocket(fd); r.error = "http.cancelled"; return r; }
        int n = ::recv(fd, buf, sizeof(buf), 0);
        if (n < 0 && lastRecvTimedOut()) {
            if (Clock::now() - lastData < std::chrono::milliseconds(idleMs)) {
                if (Clock::now() > deadline) { closeSocket(fd); r.error = "http.timeout"; return r; }
                continue;
            }
            break;   // inactive for too long: end of reading (previous behavior)
        }
        if (n <= 0) break;
        lastData = Clock::now();
        raw.append(buf, static_cast<std::size_t>(n));
        if (raw.size() > kMaxResponseBytes) { closeSocket(fd); r.error = "http.response_too_large"; return r; }
        // The inactivity timeout only bounds ONE silence: a server dripping data would hold
        // the request forever, hence this total timeout.
        if (Clock::now() > deadline) { closeSocket(fd); r.error = "http.timeout"; return r; }
    }
    closeSocket(fd);

    if (raw.empty()) { r.error = "http.empty_response"; return r; }

    return parseResponse(raw, r);
}

} // namespace

HttpResult parseResponse(const std::string& raw, HttpResult r) {
    // Status: "HTTP/1.1 200 OK".
    std::size_t sp = raw.find(' ');
    if (sp != std::string::npos) r.status = std::atoi(raw.c_str() + sp + 1);

    std::size_t hdrEnd = raw.find("\r\n\r\n");
    if (hdrEnd == std::string::npos) {
        r.body.clear();
    } else {
        r.body = raw.substr(hdrEnd + 4);
        // Transfer-Encoding: chunked -> the raw body contains the chunk sizes, to be removed.
        const std::string headers = lowerCopy(raw.substr(0, hdrEnd));
        const std::size_t te = headers.find("\r\ntransfer-encoding:");
        if (te != std::string::npos) {
            const std::size_t eol = headers.find("\r\n", te + 2);
            const std::string value = headers.substr(te, eol == std::string::npos ? eol : eol - te);
            if (value.find("chunked") != std::string::npos) {
                std::string decoded;
                if (!decodeChunked(r.body, decoded)) {
                    r.ok = false;
                    r.error = "http.invalid_chunked";
                    return r;
                }
                r.body = std::move(decoded);
            }
        }
    }
    r.ok = (r.status != 0);
    if (!r.ok) r.error = "http.invalid_response";
    return r;
}

bool decodeChunked(const std::string& in, std::string& out) {
    out.clear();
    std::size_t pos = 0;
    for (;;) {
        const std::size_t eol = in.find("\r\n", pos);
        if (eol == std::string::npos) return false;           // truncated chunk size
        // Chunk extensions (";ext=...") are ignored; the size is in hexadecimal.
        std::string sizeStr = in.substr(pos, eol - pos);
        sizeStr = sizeStr.substr(0, sizeStr.find(';'));
        if (sizeStr.empty() || sizeStr.size() > 8) return false;  // empty or > 4 GiB: invalid
        char* endp = nullptr;
        const unsigned long size = std::strtoul(sizeStr.c_str(), &endp, 16);
        if (!endp || *endp != '\0') return false;
        pos = eol + 2;
        if (size == 0) return true;                           // final chunk (trailers ignored)
        if (in.size() - pos < size + 2) return false;         // truncated data
        if (out.size() + size > kMaxResponseBytes) return false;
        out.append(in, pos, size);
        pos += size;
        if (in.compare(pos, 2, "\r\n") != 0) return false;   // CRLF de fin de bloc manquant
        pos += 2;
    }
}

void setThreadCancelFlag(const std::atomic<bool>* flag) { t_cancel = flag; }

HttpResult httpGet(const std::string& url, const std::map<std::string, std::string>& headers, int timeoutMs) {
    return request("GET", url, "", headers, timeoutMs);
}
HttpResult httpPost(const std::string& url, const std::string& body,
                    const std::map<std::string, std::string>& headers, int timeoutMs) {
    return request("POST", url, body, headers, timeoutMs);
}
HttpResult httpPut(const std::string& url, const std::string& body,
                   const std::map<std::string, std::string>& headers, int timeoutMs) {
    return request("PUT", url, body, headers, timeoutMs, "application/octet-stream");
}
HttpResult httpHead(const std::string& url,
                    const std::map<std::string, std::string>& headers, int timeoutMs) {
    return request("HEAD", url, "", headers, timeoutMs);
}

} // namespace chnet
