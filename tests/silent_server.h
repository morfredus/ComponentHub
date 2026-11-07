// Serveur TCP local qui accepte les connexions (file d'attente du système) mais ne répond jamais :
// simule un hub figé pour tester les délais et l'annulation du client HTTP.
#pragma once
#include <string>

#if defined(_WIN32)
  #include <winsock2.h>
  #include <ws2tcpip.h>
#else
  #include <arpa/inet.h>
  #include <netinet/in.h>
  #include <sys/socket.h>
  #include <unistd.h>
#endif

class SilentServer {
public:
    SilentServer() {
#if defined(_WIN32)
        WSADATA w;
        WSAStartup(MAKEWORD(2, 2), &w);
#endif
        sock_ = ::socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = 0;
        ok_ = ::bind(sock_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0 && ::listen(sock_, 4) == 0;
        socklen_t len = sizeof(addr);
        ::getsockname(sock_, reinterpret_cast<sockaddr*>(&addr), &len);
        port_ = ntohs(addr.sin_port);
    }
    ~SilentServer() {
#if defined(_WIN32)
        closesocket(sock_);
#else
        ::close(sock_);
#endif
    }
    bool ok() const { return ok_; }
    std::string url() const { return "http://127.0.0.1:" + std::to_string(port_); }

private:
#if defined(_WIN32)
    SOCKET sock_;
#else
    int sock_;
#endif
    bool ok_ = false;
    int port_ = 0;
};
