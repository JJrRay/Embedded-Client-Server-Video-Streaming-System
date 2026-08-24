/// @file tcp_server.h
/// @brief Serveur TCP mono-client avec timeout de réception.
/// @author Jean-Claude Junior Raymond, Zakaria Chikri
/// @date 2026-08-13
/// @note Fait partie du module de communication TCP/IP réseau côté serveur.
#pragma once

#include <cstddef>
#include <cstdint>

namespace Network {

/// @brief Résultat d'une tentative de réception sur le socket.
enum class RecvResult {
  /// @brief Un octet a été reçu.
  OK,
  /// @brief Délai (SO_RCVTIMEO) expiré, aucune donnée reçue.
  TIMEOUT,
  /// @brief Le pair a fermé la connexion, ou erreur fatale.
  DISCONNECTED
};

/// @brief Serveur TCP acceptant un client à la fois.
/// @details Gère l'ouverture du socket d'écoute, l'attente d'une connexion,
/// puis l'envoi et la réception de messages d'un octet. Le socket d'écoute
/// survit à la déconnexion d'un client, ce qui permet à un nouveau client de
/// se connecter sans redémarrer le serveur.
class TcpServer {
public:
  /// @brief Ouvre le socket d'écoute et effectue bind et listen.
  /// @param port Port TCP d'écoute.
  /// @param recvTimeoutMs Timeout de réception appliqué à chaque client.
  TcpServer(uint16_t port, int recvTimeoutMs);

  /// @brief Ferme le client courant, s'il y en a un, et le socket d'écoute.
  ~TcpServer();

  /// @brief Copie interdite : la classe possède des descripteurs de fichier,
  /// qu'une copie fermerait deux fois.
  /// @param other Instance source (copie interdite).
  TcpServer(const TcpServer &other) = delete;
  /// @brief Affectation interdite.
  /// @param other Instance source (affectation interdite).
  /// @return Référence sur l'instance (jamais appelée).
  TcpServer &operator=(const TcpServer &other) = delete;

  /// @brief Vérifie si le socket d'écoute est prêt.
  /// @return true si le socket d'écoute est prêt.
  bool valid() const { return _listenFd >= 0; }

  /// @brief Bloque jusqu'à l'acceptation d'un client.
  /// @return true si un client a été accepté.
  bool waitForClient();

  /// @brief Reçoit une commande d'un octet du client courant.
  /// @param[out] cmd Commande reçue, valide uniquement si RecvResult::OK.
  /// @return L'issue de la réception.
  RecvResult receiveCommand(uint8_t &cmd);

  /// @brief Envoie une réponse d'un octet au client courant.
  /// @param response Identifiant du message à transmettre.
  /// @return false si erreur.
  bool sendResponse(uint8_t response);

  /// @brief Envoie exactement @p size octets, en bouclant tant que tout n'est
  /// pas transmis.
  /// @details Un seul send() peut n'accepter qu'une partie des données. Cette
  /// boucle garantit que le bloc complet part, ou échoue.
  /// @param data Pointeur vers les octets à envoyer.
  /// @param size Nombre d'octets à envoyer.
  /// @return false si la connexion est rompue avant la fin.
  bool sendAll(const void *data, size_t size);

  /// @brief Ferme la connexion au client courant, mais le socket d'écoute
  /// reste ouvert.
  void closeClient();

private:
  /// @brief Socket d'écoute, -1 si non ouvert.
  int _listenFd = -1;
  /// @brief Socket du client courant, -1 si aucun.
  int _clientFd = -1;
  /// @brief Timeout appliqué à chaque nouveau client.
  int _recvTimeoutMs;
};

} // namespace Network