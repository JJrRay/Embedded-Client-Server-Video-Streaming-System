/// @file server_app.cpp
#include "server_app.h"

#include <arpa/inet.h>

#include <csignal>
#include <cstdio>
#include <iostream>

#include "config.h"
#include "protocol.h"

namespace Application::Interne {

void reportOutcome(SessionOutcome outcome) {
  if (outcome == SessionOutcome::DISCONNECTED) {
    std::cout << "Client déconnecté, en attente d'un nouveau client"
              << std::endl;
  }
}

// L'encodeur MJPG de la caméra insère du bourrage avant les marqueurs de
// restart. libjpeg le signale à chaque image, sans conséquence sur le
// décodage. On coupe stderr pour garder le terminal lisible.
void silenceJpegWarnings() {
  if (std::freopen("/dev/null", "w", stderr) == nullptr) {
    std::cout << "Redirection de stderr impossible" << std::endl;
  }
}

} // namespace Application::Interne

namespace Application {

void installSignalHandlers() { std::signal(SIGPIPE, SIG_IGN); }

ServerApp::ServerApp(uint16_t port, int recvTimeoutMs)
    : _server(port, recvTimeoutMs),
      _button(Config::GPIO_CHIP, Config::GPIO_LINE, Config::BUTTON_ACTIVE_LOW,
              Config::BUTTON_DEBOUNCE_MS, Config::BUTTON_POLL_US),
      _light(Config::LIGHT_SENSOR_PATH, Config::LIGHT_RAW_THRESHOLD,
             Config::LIGHT_RAW_RISES, Config::IMAGE_DARK_THRESHOLD,
             Config::STATE_HOLD_MS) {
  if (!_camera.isOpened()) {
    // Cause la plus fréquente : /dev/video0 déjà ouvert par une instance
    // précédente.
    std::cout << "Caméra indisponible : /dev/video0 absent ou déjà utilisé"
              << std::endl;
  }
  if (!_button.valid()) {
    // Le flux vidéo reste fonctionnel sans le bouton. On signale, sans
    // empêcher le serveur de démarrer.
    std::cout << "Bouton indisponible, le flux vidéo continue" << std::endl;
  }
}

bool ServerApp::valid() const { return _server.valid() && _camera.isOpened(); }

bool ServerApp::sendHeader(uint8_t header) {
  const uint32_t netFrameId = htonl(_frameId);

  // Début commun à toutes les réponses à GET_FRAME : en-tête (1 octet) puis
  // frameId. NO_LIGHT et SENSOR_ERROR s'arrêtent ici.
  if (!_server.sendResponse(header)) {
    return false;
  }
  return _server.sendAll(&netFrameId, sizeof(netFrameId));
}

bool ServerApp::sendFrame(uint8_t header, const std::vector<uint8_t> &jpeg) {
  const uint32_t netJpegSize = htonl(static_cast<uint32_t>(jpeg.size()));

  // En-tête + frameId + jpegSize + données.
  if (!sendHeader(header)) {
    return false;
  }
  if (!_server.sendAll(&netJpegSize, sizeof(netJpegSize))) {
    return false;
  }
  return _server.sendAll(jpeg.data(), jpeg.size());
}

bool ServerApp::handleGetFrame() {
  const auto capture = _camera.captureJpeg();
  if (!capture) {
    // Échec de capture : on n'envoie rien pour cette requête. Le client
    // expire et redemande au cycle suivant. L'appui éventuel reste en attente.
    return true;
  }

  const Lighting::SceneState state = _light.update(capture->brightness);
  if (state != Lighting::SceneState::NORMAL) {
    // Ces états sont prioritaires sur le bouton : l'appui en attente est
    // consommé sans être rapporté, donc aucune sauvegarde n'est déclenchée
    // pendant l'état ni au retour à la normale. Aucune image n'est transmise.
    _button.takePress();
    return sendHeader(state == Lighting::SceneState::NO_LIGHT
                          ? Protocol::NO_LIGHT
                          : Protocol::SENSOR_ERROR);
  }

  // Le compteur n'avance que pour une image réellement transmise : pendant
  // NO_LIGHT et SENSOR_ERROR il reste figé, puisque aucune image n'est
  // envoyée. Il repart à sa valeur précédente au retour à la normale.
  ++_frameId;

  // L'appui n'est consommé qu'une fois l'image prête. Il est donc toujours
  // associé à une image effectivement transmise, et jamais rapporté deux fois.
  const bool pressed = _button.takePress();
  if (pressed) {
    std::cout << "Appui détecté, image " << _frameId << std::endl;
  }

  return sendFrame(pressed ? Protocol::BUTTON_PRESS : Protocol::FRAME_HDR,
                   capture->jpeg);
}

void ServerApp::handleStop() {
  _server.sendResponse(Protocol::STOP_ACK);
  std::cout << "Arrêt demandé" << std::endl;
}

bool ServerApp::dispatchCommand(uint8_t cmd, SessionOutcome &outcome) {
  switch (cmd) {
  case Protocol::GET_FRAME:
    if (!handleGetFrame()) {
      outcome = SessionOutcome::DISCONNECTED;
      return false;
    }
    return true;

  case Protocol::STOP:
    handleStop();
    outcome = SessionOutcome::STOP_REQUESTED;
    return false;

  default:
    return true; // Commande inconnue. Ignorée sans rompre la session.
  }
}

bool ServerApp::processNextCommand(SessionOutcome &outcome) {
  uint8_t cmd = 0;
  switch (_server.receiveCommand(cmd)) {
  case Network::RecvResult::TIMEOUT:
    // Un câble débranché ne produit aucune erreur TCP : le socket reste
    // ouvert et recv() se contente d'expirer. C'est l'accumulation de ces
    // expirations qui trahit la perte du lien. Sans cette limite, le serveur
    // resterait accroché à un client disparu.
    if (Clock::now() - _lastCommand <
        std::chrono::milliseconds(Config::CLIENT_IDLE_TIMEOUT_MS)) {
      return true;
    }
    outcome = SessionOutcome::DISCONNECTED;
    return false;

  case Network::RecvResult::DISCONNECTED:
    outcome = SessionOutcome::DISCONNECTED;
    return false;

  case Network::RecvResult::OK:
    _lastCommand = Clock::now();
    return dispatchCommand(cmd, outcome);
  }
  return true;
}

SessionOutcome ServerApp::serveClient() {
  _lastCommand = Clock::now();
  SessionOutcome outcome = SessionOutcome::DISCONNECTED;
  while (processNextCommand(outcome)) {
  }
  return outcome;
}

void ServerApp::run() {
  Interne::silenceJpegWarnings();

  // Seul un STOP explicite termine le serveur. Toute autre fin de session le
  // ramène en attente, ce qui permet à un nouveau client de se connecter sans
  // relancer le programme.
  while (true) {
    if (!_server.waitForClient()) {
      continue;
    }
    std::cout << "Client connecté" << std::endl;

    const SessionOutcome outcome = serveClient();
    Interne::reportOutcome(outcome);
    _server.closeClient();

    if (outcome == SessionOutcome::STOP_REQUESTED) {
      return;
    }
  }
}

} // namespace Application