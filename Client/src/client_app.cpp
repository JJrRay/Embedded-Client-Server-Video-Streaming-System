/// @file client_app.cpp
#include "client_app.h"

#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <arpa/inet.h>

#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include "config.h"
#include "protocol.h"

namespace Application::Interne {

cv::Mat createEmptyFrame() {
  return cv::Mat::zeros(Config::FRAME_HEIGHT, Config::FRAME_WIDTH, CV_8UC3);
}

bool isQuitKey(int key) { return key == 'q' || key == 'Q'; }

void drawFrameId(cv::Mat &image, uint32_t frameId) {
  const std::string text = "Frame " + std::to_string(frameId);
  cv::putText(image, text, cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 1.0,
              cv::Scalar(0, 255, 0), 2, cv::LINE_8, false);
}

void drawMessage(cv::Mat &image, const std::string &message) {
  cv::putText(image, message, cv::Point(40, Config::FRAME_HEIGHT / 2),
              cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(255, 255, 255), 2,
              cv::LINE_8, false);
}

// Compose p. ex. « Lumiere insuffisante (t = 2.3 s) ».
std::string withElapsed(const char *message, double seconds) {
  std::ostringstream stream;
  stream << message << " (t = " << std::fixed << std::setprecision(1) << seconds
         << " s)";
  return stream.str();
}

} // namespace Application::Interne

namespace Application {

bool ClientApp::initialize() {
  // Le répertoire est créé s'il manque, et vidé s'il existe déjà. Chaque
  // exécution repart d'un dossier propre.
  if (!_store.reset()) {
    return false;
  }

  cv::namedWindow(Config::WINDOW_NAME, cv::WINDOW_AUTOSIZE);
  _frame = Interne::createEmptyFrame();
  return true;
}

void ClientApp::setState(DisplayState state) {
  if (state == _state) {
    return; // Le chronomètre doit continuer tant que l'état persiste.
  }
  _state = state;
  _stateSince = Clock::now();
}

double ClientApp::elapsedSeconds() const {
  return std::chrono::duration<double>(Clock::now() - _stateSince).count();
}

cv::Mat ClientApp::composeFrame() const {
  if (_state == DisplayState::NORMAL) {
    return _frame;
  }

  cv::Mat canvas = Interne::createEmptyFrame();
  switch (_state) {
  case DisplayState::NO_LIGHT:
    Interne::drawMessage(canvas, Interne::withElapsed(Config::NO_LIGHT_MESSAGE,
                                                      elapsedSeconds()));
    break;
  case DisplayState::SENSOR_ERROR:
    Interne::drawMessage(
        canvas,
        Interne::withElapsed(Config::SENSOR_ERROR_MESSAGE, elapsedSeconds()));
    break;
  default:
    Interne::drawMessage(canvas, Config::CONNECTION_LOST_MESSAGE);
    break;
  }
  return canvas;
}

bool ClientApp::displayAndPollKey(int waitMs) {
  // La fenêtre est recomposée à chaque cycle plutôt qu'à chaque réponse : le
  // temps affiché avance ainsi en continu, même si une réponse se fait
  // attendre.
  cv::imshow(Config::WINDOW_NAME, composeFrame());
  return Interne::isQuitKey(cv::waitKey(waitMs));
}

bool ClientApp::connectToServer() {
  setState(DisplayState::LINK_LOST);

  while (true) {
    if (displayAndPollKey(Config::UI_POLL_MS)) {
      return false;
    }

    const auto attempt = Clock::now();
    if (_client.connect(Config::SERVER_IP, Protocol::PORT,
                        Config::RECV_TIMEOUT_MS, Config::CONNECT_TIMEOUT_MS)) {
      std::cout << "Connecté au serveur" << std::endl;
      return true;
    }

    // Attente active plutôt qu'un sleep : la fenêtre n'existe que pendant un
    // cv::waitKey, donc dormir 500 ms d'un bloc gèlerait l'affichage et
    // rendrait la touche q pratiquement inatteignable pendant la coupure.
    // L'espacement est mesuré depuis le début de la tentative, pour qu'une
    // tentative lente ne décale pas la suivante.
    const auto next =
        attempt + std::chrono::milliseconds(Config::RECONNECT_PERIOD_MS);
    while (Clock::now() < next) {
      if (displayAndPollKey(Config::UI_POLL_MS)) {
        return false;
      }
    }
  }
}

bool ClientApp::exchangeFrame() {
  if (!_client.send(Protocol::GET_FRAME)) {
    return false;
  }

  uint8_t response = 0;
  switch (_client.receive(response)) {
  case Network::RecvResult::TIMEOUT:
    // Un timeout isolé est normal. C'est leur accumulation qui trahit la
    // coupure : un câble débranché ne provoque aucune erreur TCP, le socket
    // reste ouvert et recv() se contente d'expirer.
    return Clock::now() - _lastResponse <
           std::chrono::milliseconds(Config::LINK_TIMEOUT_MS);
  case Network::RecvResult::DISCONNECTED:
    return false;
  case Network::RecvResult::OK:
    break;
  }

  _lastResponse = Clock::now();
  return handleResponse(response);
}

bool ClientApp::handleResponse(uint8_t response) {
  // Seules les réponses à GET_FRAME sont suivies de frameId.
  if (response != Protocol::FRAME_HDR && response != Protocol::BUTTON_PRESS &&
      response != Protocol::NO_LIGHT && response != Protocol::SENSOR_ERROR) {
    return true;
  }

  uint32_t netFrameId = 0;
  if (_client.receiveAll(&netFrameId, sizeof(netFrameId),
                         Config::LINK_TIMEOUT_MS) != Network::RecvResult::OK) {
    return false;
  }
  _frameId = ntohl(netFrameId);

  // Les deux en-têtes d'image annoncent la même structure. Seul BUTTON_PRESS
  // demande en plus une sauvegarde sur disque. NO_LIGHT et SENSOR_ERROR
  // s'arrêtent après frameId : aucune image à recevoir.
  switch (response) {
  case Protocol::FRAME_HDR:
    setState(DisplayState::NORMAL);
    return receiveImage(false);
  case Protocol::BUTTON_PRESS:
    setState(DisplayState::NORMAL);
    return receiveImage(true);
  case Protocol::NO_LIGHT:
    setState(DisplayState::NO_LIGHT);
    return true;
  default:
    setState(DisplayState::SENSOR_ERROR);
    return true;
  }
}

bool ClientApp::receiveImage(bool save) {
  uint32_t netJpegSize = 0;
  if (_client.receiveAll(&netJpegSize, sizeof(netJpegSize),
                         Config::LINK_TIMEOUT_MS) != Network::RecvResult::OK) {
    return false;
  }
  const uint32_t jpegSize = ntohl(netJpegSize);

  // Réception des octets JPEG dans un tampon de la taille annoncée.
  std::vector<uint8_t> jpeg(jpegSize);
  if (jpegSize > 0 &&
      _client.receiveAll(jpeg.data(), jpegSize, Config::LINK_TIMEOUT_MS) !=
          Network::RecvResult::OK) {
    return false;
  }

  // Écriture avant l'affichage. Les octets reçus sont déjà un JPEG, la
  // sauvegarde ne coûte qu'une écriture et le flux continue normalement.
  if (save && jpegSize > 0) {
    _store.save(_frameId, jpeg);
  }

  cv::Mat decoded = cv::imdecode(jpeg, cv::IMREAD_COLOR);
  if (!decoded.empty()) {
    Interne::drawFrameId(decoded, _frameId);
    _frame = decoded;
  }
  return true;
}

void ClientApp::requestShutdown() {
  std::cout << "Arrêt demandé" << std::endl;
  _client.send(Protocol::STOP);

  // Le STOP arrive derrière les réponses aux GET_FRAME déjà en vol. Tant que
  // le STOP_ACK n'est pas lu, ces réponses sont consommées normalement : si la
  // connexion se fermait avant, l'envoi en cours échouerait côté serveur, qui
  // repartirait en attente d'un nouveau client sans jamais lire le STOP.
  const auto deadline =
      Clock::now() + std::chrono::milliseconds(Config::SHUTDOWN_TIMEOUT_MS);

  while (Clock::now() < deadline) {
    uint8_t response = 0;
    switch (_client.receive(response)) {
    case Network::RecvResult::TIMEOUT:
      continue;
    case Network::RecvResult::DISCONNECTED:
      return;
    case Network::RecvResult::OK:
      break;
    }

    if (response == Protocol::STOP_ACK) {
      return;
    }
    if (!handleResponse(response)) {
      return; // Lien rompu pendant la réception d'une réponse en attente.
    }
  }

  std::cerr << "STOP_ACK non reçu" << std::endl;
}

void ClientApp::run() {
  // Une coupure ne termine jamais le client : la session est abandonnée, la
  // fenêtre affiche CONNEXION PERDUE et connectToServer() reprend la main.
  // Seule la touche q sort de cette boucle.
  while (connectToServer()) {
    _lastResponse = Clock::now();
    _reporter.reset();

    if (runSession() == LoopExit::USER_QUIT) {
      requestShutdown();
      break;
    }
    _client.close();
    std::cerr << "Connexion perdue, reconnexion en cours" << std::endl;
  }

  cv::destroyAllWindows();
}

LoopExit ClientApp::runSession() {
  while (true) {
    _reporter.beginCycle();

    if (!exchangeFrame()) {
      return LoopExit::LINK_LOST;
    }
    if (displayAndPollKey(Config::CYCLE_PERIOD_MS)) {
      return LoopExit::USER_QUIT;
    }

    _reporter.endCycle();
  }
}

} // namespace Application