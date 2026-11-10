/**
 * HttpClient - minimal, blocking HTTP/1.1 client with no external dependency.
 *
 * Client counterpart of the morfSync server: same philosophy (no TLS, private
 * LAN, we write both ends). Portable across Windows (winsock2) / Linux / ARM64
 * (POSIX sockets). No dependency on Qt: reusable and testable in the
 * business core (componenthub_core).
 */

#pragma once
#include <atomic>
#include <string>
#include <map>

namespace chnet {

struct HttpResult {
    bool ok = false;          // connection + HTTP exchange completed (status read)
    int status = 0;           // HTTP code (200, 404...) if ok
    std::string body;         // response body
    std::string error;        // message if !ok (network unreachable, invalid URL...)
};

// Attaches a cancellation flag to the CALLING THREAD (nullptr = none). When it turns
// true, the requests running in that thread stop within a few hundred ms with
// the error "http.cancelled". The flag must outlive the thread or be removed beforehand.
void setThreadCancelFlag(const std::atomic<bool>* flag);

// Synchronous GET/POST. `url` looks like http://host:port/path (no https).
// `headers`: additional headers (e.g. Authorization). `timeoutMs`: connection
// timeout (0 = 5 s) and per read; a TOTAL timeout of 60 s and a cap of
// 128 MiB per response also bound every request.
HttpResult httpGet(const std::string& url,
                   const std::map<std::string, std::string>& headers = {},
                   int timeoutMs = 0);

HttpResult httpPost(const std::string& url, const std::string& body,
                    const std::map<std::string, std::string>& headers = {},
                    int timeoutMs = 0);

// PUT with a binary body (Content-Type: application/octet-stream). Used to
// upload an attachment blob to the morfSync store.
HttpResult httpPut(const std::string& url, const std::string& body,
                   const std::map<std::string, std::string>& headers = {},
                   int timeoutMs = 0);

// HEAD: presence without a body. Used to know whether the hub already holds a blob before
// uploading it.
HttpResult httpHead(const std::string& url,
                    const std::map<std::string, std::string>& headers = {},
                    int timeoutMs = 0);

// Pure functions exposed for the tests (ctest) - not meant for callers.
// Decodes a Transfer-Encoding: chunked body. Returns false if the stream is truncated or invalid.
bool decodeChunked(const std::string& in, std::string& out);
// Parses a raw HTTP response (status, body, chunked), completing `r`.
HttpResult parseResponse(const std::string& raw, HttpResult r = HttpResult());

} // namespace chnet
