/// @file tcp_client.h
/// @brief Client TCP avec timeout de connexion et de réception.
/// @author Jean-Claude Junior Raymond, Zakaria Chikri
/// @date 2026-08-12
/// @note Fait partie du module de communication TCP/IP réseau côté client.
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
  /// @brief Le serveur a fermé la connexion, ou erreur fatale.
  DISCONNECTED
};

/// @brief Encapsule la connexion TCP vers le serveur et les échanges de
/// messages d'un octet.
/// @details Le client est responsable d'initier les requêtes. Le serveur ne
/// fait que répondre. La connexion est fermée automatiquement à la
/// destruction, et l'objet est réutilisable pour une nouvelle tentative après
/// une coupure.
class TcpClient {
public:
  /// @brief Construit un client non connecté.
  TcpClient() = default;

  /// @brief Ferme la connexion si elle est encore ouverte.
  ~TcpClient();

  /// @brief Copie interdite : la classe possède un descripteur de fichier,
  /// qu'une copie fermerait deux fois.
  /// @param other Instance source (copie interdite).
  TcpClient(const TcpClient &other) = delete;
  /// @brief Affectation interdite.
  /// @param other Instance source (affectation interdite).
  /// @return Référence sur l'instance (jamais appelée).
  TcpClient &operator=(const TcpClient &other) = delete;

  /// @brief Se connecte au serveur et applique le timeout de réception.
  /// @details La connexion est tentée en mode non bloquant puis surveillée
  /// par poll(). Sans cela, un serveur injoignable (câble débranché côté
  /// Odroid-C2) ferait attendre connect() plus d'une minute, pendant laquelle
  /// le client ne rafraîchirait ni sa fenêtre ni son message d'erreur.
  /// @param ip Adresse IPv4 du serveur (chaîne pointée-décimale).
  /// @param port Port TCP du serveur.
  /// @param recvTimeoutMs Timeout de réception, en millisecondes.
  /// @param connectTimeoutMs Temps maximal accordé à la tentative, en
  /// millisecondes.
  /// @return true si la connexion a réussi.
  bool connect(const char *ip, uint16_t port, int recvTimeoutMs,
               int connectTimeoutMs);

  /// @brief Envoie une commande d'un octet au serveur.
  /// @param command Identifiant du message à transmettre.
  /// @return false si erreur.
  bool send(uint8_t command);

  /// @brief Reçoit une réponse d'un octet du serveur.
  /// @param[out] response Réponse reçue, valide uniquement si RecvResult::OK.
  /// @return L'issue de la réception.
  RecvResult receive(uint8_t &response);

  /// @brief Reçoit exactement @p size octets, en bouclant tant que tout n'est
  /// pas arrivé.
  /// @details Cette boucle réassemble le bloc complet, car une image
  /// partielle est inutilisable. Les expirations sont réessayées jusqu'à
  /// @p timeoutMs : au-delà, le lien est considéré comme rompu, ce qui évite
  /// d'attendre indéfiniment la fin d'une image coupée en plein transfert.
  /// @param[out] buffer Destination, d'au moins @p size octets.
  /// @param size Nombre d'octets à lire.
  /// @param timeoutMs Temps maximal accordé au bloc, en millisecondes.
  /// @return OK si tout est lu, DISCONNECTED si la connexion est rompue.
  RecvResult receiveAll(void *buffer, size_t size, int timeoutMs);

  /// @brief Ferme la connexion. Sans effet si elle est déjà fermée.
  void close();

private:
  /// @brief Socket de la connexion, -1 si non connecté.
  int _fd = -1;
};

} // namespace Network