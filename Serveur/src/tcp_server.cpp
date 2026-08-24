/// @file tcp_server.cpp
#include "tcp_server.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>

namespace Network::Interne {

void applyRecvTimeout(int fd, int timeoutMs) {
  timeval tv{};
  tv.tv_sec = timeoutMs / 1000;
  tv.tv_usec = (timeoutMs % 1000) * 1000;
  setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
}

} // namespace Network::Interne

namespace Network {

TcpServer::TcpServer(uint16_t port, int recvTimeoutMs)
    : _recvTimeoutMs(recvTimeoutMs) {
  _listenFd = socket(AF_INET, SOCK_STREAM, 0);
  if (_listenFd < 0) {
    std::perror("socket");
    return;
  }

  // Le serveur peut être relancé immédiatement après un CTRL+C.
  const int opt = 1;
  if (setsockopt(_listenFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
    std::perror("setsockopt(SO_REUSEADDR)");
    ::close(_listenFd);
    _listenFd = -1;
    return;
  }

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(port);

  if (bind(_listenFd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
    std::perror("bind");
    ::close(_listenFd);
    _listenFd = -1;
    return;
  }
  if (listen(_listenFd, 1) < 0) {
    std::perror("listen");
    ::close(_listenFd);
    _listenFd = -1;
  }
}

TcpServer::~TcpServer() {
  closeClient();
  if (_listenFd >= 0) {
    ::close(_listenFd);
  }
}

bool TcpServer::waitForClient() {
  _clientFd = accept(_listenFd, nullptr, nullptr);
  if (_clientFd < 0) {
    std::perror("accept");
    return false;
  }
  Interne::applyRecvTimeout(_clientFd, _recvTimeoutMs);
  return true;
}

RecvResult TcpServer::receiveCommand(uint8_t &cmd) {
  const ssize_t n = recv(_clientFd, &cmd, sizeof(cmd), 0);
  if (n == 0) {
    return RecvResult::DISCONNECTED;
  }
  if (n < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return RecvResult::TIMEOUT;
    }
    std::perror("recv");
    return RecvResult::DISCONNECTED;
  }
  return RecvResult::OK;
}

bool TcpServer::sendResponse(uint8_t response) {
  // MSG_NOSIGNAL : pas de SIGPIPE si le client a déjà fermé la connexion.
  return send(_clientFd, &response, sizeof(response), MSG_NOSIGNAL) >= 0;
}

bool TcpServer::sendAll(const void *data, size_t size) {
  const auto *const bytes = static_cast<const uint8_t *>(data);
  size_t sent = 0;
  while (sent < size) {
    // MSG_NOSIGNAL : pas de SIGPIPE si le client a déjà fermé la connexion.
    const ssize_t n = send(_clientFd, bytes + sent, size - sent, MSG_NOSIGNAL);
    if (n <= 0) {
      return false; // Connexion rompue.
    }
    sent += static_cast<size_t>(n);
  }
  return true;
}

void TcpServer::closeClient() {
  if (_clientFd >= 0) {
    ::close(_clientFd);
    _clientFd = -1;
  }
}

} // namespace Network