/// @file config.h
/// @brief Constantes de configuration du client.
/// @author Jean-Claude Junior Raymond, Zakaria Chikri
/// @date 2026-08-12
#pragma once

/// @brief Constantes de configuration du client.
namespace Config {

/// @brief Adresse IPv4 de l'Odroid-C2.
constexpr char SERVER_IP[] = "192.168.7.2";

/// @brief Timeout de réception TCP, en millisecondes.
/// @details Appliqué via SO_RCVTIMEO. Une réception qui expire signale une
/// absence de donnée, et non une déconnexion.
constexpr int RECV_TIMEOUT_MS = 60;

/// @brief Silence toléré avant de déclarer le lien perdu, en millisecondes.
/// @details Un câble débranché ne produit aucune erreur TCP : le socket reste
/// ouvert et recv() se contente d'expirer. C'est l'accumulation de ces
/// expirations qui trahit la coupure. La valeur est plus longue que le délai
/// d'abandon du serveur, pour que celui-ci soit déjà revenu en attente
/// lorsque le client se reconnecte.
constexpr int LINK_TIMEOUT_MS = 3000;

/// @brief Intervalle entre deux tentatives de reconnexion, en millisecondes.
constexpr int RECONNECT_PERIOD_MS = 500;

/// @brief Temps maximal accordé à une tentative de connexion (ms).
/// @details Plus court que RECONNECT_PERIOD_MS, pour qu'une tentative vers un
/// serveur injoignable n'empiète pas sur la suivante et que la fenêtre
/// continue d'être rafraîchie pendant la coupure.
constexpr int CONNECT_TIMEOUT_MS = 400;

/// @brief Durée d'une scrutation du clavier hors session, en millisecondes.
/// @details Sert d'argument à cv::waitKey pendant l'attente entre deux
/// tentatives de reconnexion. La fenêtre n'est vivante que le temps d'un
/// waitKey : une seule scrutation par tentative laisserait la touche q
/// inaperçue 499 ms sur 500.
constexpr int UI_POLL_MS = 30;

/// @brief Période du cycle requête/réponse, en millisecondes.
/// @details Sert d'argument à cv::waitKey, qui cadence la boucle principale.
constexpr int CYCLE_PERIOD_MS = 1;

/// @brief Titre de la fenêtre d'affichage.
constexpr char WINDOW_NAME[] = "Surveillance vidéo";

/// @brief Largeur de l'image affichée, en pixels.
constexpr int FRAME_WIDTH = 800;

/// @brief Hauteur de l'image affichée, en pixels.
constexpr int FRAME_HEIGHT = 600;

/// @brief Répertoire où sont sauvegardées les images associées à un appui.
/// @details Créé s'il n'existe pas, vidé au démarrage du client.
constexpr char IMAGE_DIR[] = "images";

/// @brief Préfixe du nom des fichiers sauvegardés.
/// @details Le numéro de frame est ajouté ensuite.
constexpr char IMAGE_PREFIX[] = "capture_frame_";

/// @brief Extension des fichiers sauvegardés.
constexpr char IMAGE_EXTENSION[] = ".jpg";

/// @brief Message affiché lorsque le serveur signale NO_LIGHT.
/// @details Sans accent : les polices Hershey de cv::putText ne couvrent que
/// l'ASCII et remplacent les caractères accentués par des points
/// d'interrogation.
constexpr char NO_LIGHT_MESSAGE[] = "Lumiere insuffisante";

/// @brief Message affiché lorsque le serveur signale SENSOR_ERROR.
constexpr char SENSOR_ERROR_MESSAGE[] = "Capteur errone";

/// @brief Message affiché tant que le lien avec le serveur est rompu.
constexpr char CONNECTION_LOST_MESSAGE[] = "CONNEXION PERDUE";

/// @brief Temps maximal d'attente du STOP_ACK après l'envoi de STOP (ms).
/// @details Laisse au serveur le temps d'écouler les réponses déjà en file
/// devant le STOP avant d'y répondre.
constexpr int SHUTDOWN_TIMEOUT_MS = 2000;

} // namespace Config