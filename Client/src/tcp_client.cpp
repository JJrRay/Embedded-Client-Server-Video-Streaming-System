/// @file tcp_client.cpp
#include "tcp_client.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <iostream>

namespace Network::Interne {

using Clock = std::chrono::steady_clock;

void applyRecvTimeout(int fd, int timeoutMs) {
  timeval tv{};
  tv.tv_sec = timeoutMs / 1000;
  tv.tv_usec = (timeoutMs % 1000) * 1000;
  setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
}

// Attend la fin d'une connexion non bloquante et en lit le résultat.
bool waitForConnect(int fd, int timeoutMs) {
  pollfd watched{};
  watched.fd = fd;
  watched.events = POLLOUT;

  // Le socket devient inscriptible dès que la poignée de main aboutit ou
  // échoue. C'est SO_ERROR qui départage les deux cas.
  if (poll(&watched, 1, timeoutMs) <= 0) {
    return false;
  }

  int socketError = 0;
  socklen_t length = sizeof(socketError);
  if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &socketError, &length) < 0) {
    return false;
  }
  return socketError == 0;
}

} // namespace Network::Interne

namespace Network {

TcpClient::~TcpClient() { close(); }

bool TcpClient::connect(const char *ip, uint16_t port, int recvTimeoutMs,
                        int connectTimeoutMs) {
  close(); // Une tentative précédente peut avoir laissé un socket ouvert.

  _fd = socket(AF_INET, SOCK_STREAM, 0);
  if (_fd < 0) {
    std::perror("socket");
    return false;
  }

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  if (inet_pton(AF_INET, ip, &addr.sin_addr) <= 0) {
    std::cerr << "Adresse IP invalide : " << ip << std::endl;
    close();
    return false;
  }

  // Connexion non bloquante : un serveur injoignable (câble débranché côté
  // Odroid-C2) laisserait sinon connect() attendre plus d'une minute, pendant
  // laquelle ni la fenêtre ni la touche q ne seraient traitées.
  const int flags = fcntl(_fd, F_GETFL, 0);
  fcntl(_fd, F_SETFL, flags | O_NONBLOCK);

  if (::connect(_fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
    // EINPROGRESS est le cas nominal en mode non bloquant : la poignée de
    // main est lancée, son résultat viendra par poll().
    if (errno != EINPROGRESS ||
        !Interne::waitForConnect(_fd, connectTimeoutMs)) {
      close();
      return false;
    }
  }

  fcntl(_fd, F_SETFL, flags); // Retour en mode bloquant pour les échanges.
  Interne::applyRecvTimeout(_fd, recvTimeoutMs);
  return true;
}

bool TcpClient::send(uint8_t command) {
  // MSG_NOSIGNAL : pas de SIGPIPE si le serveur a déjà fermé la connexion.
  return ::send(_fd, &command, sizeof(command), MSG_NOSIGNAL) >= 0;
}

RecvResult TcpClient::receive(uint8_t &response) {
  const ssize_t n = ::recv(_fd, &response, sizeof(response), 0);
  if (n == 0) {
    return RecvResult::DISCONNECTED;
  }
  if (n < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return RecvResult::TIMEOUT;
    }
    return RecvResult::DISCONNECTED;
  }
  return RecvResult::OK;
}

RecvResult TcpClient::receiveAll(void *buffer, size_t size, int timeoutMs) {
  auto *const bytes = static_cast<uint8_t *>(buffer);
  const auto deadline =
      Interne::Clock::now() + std::chrono::milliseconds(timeoutMs);

  size_t received = 0;
  while (received < size) {
    const ssize_t n = ::recv(_fd, bytes + received, size - received, 0);
    if (n == 0) {
      return RecvResult::DISCONNECTED;
    }
    if (n < 0) {
      // Une image partielle est inutilisable : on réessaie tant que le lien
      // reste plausible. Passé le délai, l'image est abandonnée avec la
      // session, sinon une coupure en plein transfert bloquerait le client.
      if ((errno == EAGAIN || errno == EWOULDBLOCK) &&
          Interne::Clock::now() < deadline) {
        continue;
      }
      return RecvResult::DISCONNECTED;
    }
    received += static_cast<size_t>(n);
  }
  return RecvResult::OK;
}

void TcpClient::close() {
  if (_fd >= 0) {
    ::close(_fd);
    _fd = -1;
  }
}

} // namespace Network