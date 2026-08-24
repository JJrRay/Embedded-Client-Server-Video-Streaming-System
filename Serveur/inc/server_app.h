/// @file server_app.h
/// @brief Logique applicative du serveur pour les sessions client et
/// aiguillage des commandes du protocole.
/// @author Jean-Claude Junior Raymond, Zakaria Chikri
/// @date 2026-08-13
/// @note Fait partie du module applicatif côté serveur. S'appuie sur TcpServer
/// pour le fonctionnement des sockets et sur ButtonMonitor pour le GPIO.
#pragma once

#include <chrono>
#include <cstdint>
#include <vector>

#include "button.h"
#include "camera.h"
#include "light_monitor.h"
#include "tcp_server.h"

namespace Application {

/// @brief Issue possible d'une session client.
enum class SessionOutcome {
  /// @brief STOP reçu : le serveur doit se terminer.
  STOP_REQUESTED,
  /// @brief Client parti : le serveur doit réattendre.
  DISCONNECTED
};

/// @brief Ignore SIGPIPE au niveau du processus.
/// @details Sans cela, un send() vers un client déjà parti tuerait le serveur
/// par signal, ce qui violerait l'exigence de robustesse.
void installSignalHandlers();

/// @brief Application serveur : accepte les clients successifs et répond aux
/// commandes du protocole jusqu'à réception de STOP.
/// @details Le socket d'écoute survit à chaque session, ce qui permet à un
/// nouveau client de se connecter sans redémarrer le serveur. Aucune perte de
/// connexion ne termine le programme : une session interrompue est abandonnée
/// et le serveur retourne en attente.
class ServerApp {
public:
  /// @brief Construit l'application, ouvre le socket d'écoute et démarre la
  /// surveillance du bouton.
  /// @param port Port TCP d'écoute.
  /// @param recvTimeoutMs Timeout de réception appliqué à chaque client.
  ServerApp(uint16_t port, int recvTimeoutMs);

  /// @brief Vérifie si le socket d'écoute est prêt.
  /// @return true si le socket d'écoute est prêt.
  bool valid() const;

  /// @brief Boucle principale. Sert les clients successifs jusqu'à STOP.
  void run();

private:
  /// @brief Horloge monotone, insensible aux changements d'heure système.
  using Clock = std::chrono::steady_clock;

  /// @brief Sert un client jusqu'à STOP, déconnexion ou silence prolongé.
  /// @return L'issue de la session.
  SessionOutcome serveClient();

  /// @brief Lit une commande et l'exécute.
  /// @param[out] outcome Renseigné uniquement si la session doit se terminer.
  /// @return true si la session se poursuit.
  bool processNextCommand(SessionOutcome &outcome);

  /// @brief Aiguille une commande reçue vers son gestionnaire.
  /// @param cmd Identifiant du message reçu du client.
  /// @param[out] outcome Renseigné uniquement si la session doit se terminer.
  /// @return true si la session se poursuit.
  bool dispatchCommand(uint8_t cmd, SessionOutcome &outcome);

  /// @brief Répond à GET_FRAME selon l'état de la scène : NO_LIGHT ou
  /// SENSOR_ERROR sans image, sinon FRAME_HDR, ou BUTTON_PRESS si un appui est
  /// en attente.
  /// @return false si le client est parti.
  bool handleGetFrame();

  /// @brief Envoie l'octet d'en-tête suivi de frameId.
  /// @param header FRAME_HDR, BUTTON_PRESS, NO_LIGHT ou SENSOR_ERROR.
  /// @return false si le client est parti.
  bool sendHeader(uint8_t header);

  /// @brief Envoie l'en-tête et les données d'une image.
  /// @param header FRAME_HDR ou BUTTON_PRESS.
  /// @param jpeg Données JPEG de l'image.
  /// @return false si le client est parti.
  bool sendFrame(uint8_t header, const std::vector<uint8_t> &jpeg);

  /// @brief Répond à STOP par STOP_ACK et journalise la demande d'arrêt.
  void handleStop();

  /// @brief Socket d'écoute et connexion au client courant.
  Network::TcpServer _server;
  /// @brief Caméra USB.
  Vision::Camera _camera;
  /// @brief Surveillance du bouton-poussoir sur GPIO.
  Gpio::ButtonMonitor _button;
  /// @brief Surveillance de la photorésistance et cohérence avec l'image.
  Lighting::LightMonitor _light;
  /// @brief Compteur d'images, incrémenté à chaque capture.
  uint32_t _frameId = 0;
  /// @brief Date de la dernière commande reçue du client courant.
  Clock::time_point _lastCommand = Clock::now();
};

} // namespace Application