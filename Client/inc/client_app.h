/// @file client_app.h
/// @brief Logique côté client : boucle de cycles, reconnexion automatique,
/// affichage, sauvegarde des images et séquence d'arrêt.
/// @author Jean-Claude Junior Raymond, Zakaria Chikri
/// @date 2026-08-12
/// @note Fait partie du module applicatif côté client. Utilise TcpClient pour
/// la mécanique des sockets, CycleReporter pour les statistiques et ImageStore
/// pour l'écriture sur disque.
#pragma once

#include <chrono>
#include <cstdint>

#include <opencv2/core.hpp>

#include "config.h"
#include "cycle_reporter.h"
#include "image_store.h"
#include "tcp_client.h"

namespace Application {

/// @brief Ce que la fenêtre doit montrer.
/// @details Sépare l'affichage du dialogue réseau : la fenêtre est recomposée
/// à chaque cycle à partir de cet état, ce qui fait avancer le compteur de
/// temps même quand aucune réponse n'arrive.
enum class DisplayState {
  /// @brief Flux vidéo normal.
  NORMAL,
  /// @brief Le serveur signale une luminosité insuffisante.
  NO_LIGHT,
  /// @brief Le serveur signale une incohérence capteur/image.
  SENSOR_ERROR,
  /// @brief Lien avec le serveur rompu, reconnexion en cours.
  LINK_LOST
};

/// @brief Raison de sortie d'une session.
enum class LoopExit {
  /// @brief L'utilisateur a appuyé sur q : arrêt propre attendu.
  USER_QUIT,
  /// @brief Lien rompu : le client tente de se reconnecter.
  LINK_LOST
};

/// @brief Application client. Interroge le serveur à chaque cycle, affiche la
/// fenêtre et publie les statistiques de cycle.
/// @details Le client initie les requêtes. Une réponse BUTTON_PRESS déclenche
/// en plus la sauvegarde de l'image reçue. Une perte de connexion ne termine
/// jamais le programme : la session est abandonnée, la fenêtre affiche
/// CONNEXION PERDUE et une nouvelle connexion est tentée périodiquement.
class ClientApp {
public:
  /// @brief Prépare le répertoire d'images et ouvre la fenêtre d'affichage.
  /// @return false si le répertoire n'a pas pu être préparé.
  bool initialize();

  /// @brief Boucle principale : connexion, session, reconnexion, jusqu'à ce
  /// que l'utilisateur quitte.
  void run();

private:
  /// @brief Horloge monotone, insensible aux changements d'heure système.
  using Clock = std::chrono::steady_clock;

  /// @brief Tente de se connecter au serveur jusqu'à y parvenir.
  /// @details La fenêtre continue d'afficher CONNEXION PERDUE et de réagir à
  /// la touche q entre deux tentatives.
  /// @return false si l'utilisateur a demandé l'arrêt pendant la coupure.
  bool connectToServer();

  /// @brief Enchaîne les cycles tant que le lien tient.
  /// @return La raison de sortie de la session.
  LoopExit runSession();

  /// @brief Effectue un échange GET_FRAME / réponse du serveur.
  /// @details Un timeout isolé n'est pas une erreur : la réponse est
  /// simplement absente de cette fenêtre de 60 ms. C'est leur accumulation
  /// au-delà de LINK_TIMEOUT_MS qui signale la coupure.
  /// @return false si le lien est rompu.
  bool exchangeFrame();

  /// @brief Lit frameId puis la suite du message propre à la réponse reçue.
  /// @param response En-tête reçu du serveur.
  /// @return false si le lien est rompu.
  bool handleResponse(uint8_t response);

  /// @brief Reçoit jpegSize et les données JPEG, puis décode.
  /// @details Appelée après réception de FRAME_HDR ou de BUTTON_PRESS. La
  /// structure du message est la même dans les deux cas.
  /// @param save true pour écrire aussi l'image sur disque.
  /// @return false si le lien est rompu.
  bool receiveImage(bool save);

  /// @brief Change l'état affiché et redémarre son chronomètre.
  /// @details Sans effet si l'état est déjà celui demandé, pour que le temps
  /// écoulé continue de croître tant que l'état persiste.
  /// @param state Nouvel état affiché.
  void setState(DisplayState state);

  /// @brief Temps écoulé depuis l'entrée dans l'état courant, en secondes.
  /// @return Le nombre de secondes, avec sa partie fractionnaire.
  double elapsedSeconds() const;

  /// @brief Construit l'image à afficher pour l'état courant.
  /// @details En état normal, la dernière image reçue. Sinon, un fond noir
  /// portant le message et, pour NO_LIGHT et SENSOR_ERROR, le temps écoulé.
  /// @return L'image à envoyer à la fenêtre.
  cv::Mat composeFrame() const;

  /// @brief Affiche l'image de l'état courant et scrute le clavier.
  /// @details L'attente de cv::waitKey cadence aussi la boucle principale.
  /// @param waitMs Durée de scrutation du clavier, en millisecondes.
  /// @return true si l'utilisateur demande l'arrêt.
  bool displayAndPollKey(int waitMs);

  /// @brief Séquence d'arrêt : STOP, puis attente du STOP_ACK.
  /// @details Les réponses aux GET_FRAME déjà envoyés arrivent avant le
  /// STOP_ACK. Elles sont consommées jusqu'à celui-ci : fermer la connexion
  /// plus tôt empêcherait le serveur de lire le STOP et de se terminer.
  void requestShutdown();

  /// @brief Connexion TCP vers le serveur.
  Network::TcpClient _client;
  /// @brief Statistiques de cycle affichées chaque seconde.
  Metrics::CycleReporter _reporter;
  /// @brief Répertoire de sauvegarde des images.
  Storage::ImageStore _store{Config::IMAGE_DIR};
  /// @brief Dernière image reçue et décodée.
  cv::Mat _frame;
  /// @brief Numéro de la dernière image reçue.
  uint32_t _frameId = 0;
  /// @brief Ce que la fenêtre montre actuellement.
  DisplayState _state = DisplayState::LINK_LOST;
  /// @brief Date d'entrée dans l'état courant.
  Clock::time_point _stateSince = Clock::now();
  /// @brief Date de la dernière réponse reçue du serveur.
  Clock::time_point _lastResponse = Clock::now();
};

} // namespace Application