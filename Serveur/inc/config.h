/// @file config.h
/// @brief Constantes de configuration du serveur.
/// @author Jean-Claude Junior Raymond, Zakaria Chikri
/// @date 2026-08-13
/// @note Regroupe les valeurs réglables du serveur en un seul point, afin
/// de les retrouver plus facilement.
#pragma once

/// @brief Constantes de configuration du serveur.
namespace Config {

/// @brief Timeout de réception TCP appliqué à chaque client (ms).
/// @details Appliqué via SO_RCVTIMEO. Une réception qui expire signale une
/// absence de donnée, et non une déconnexion.
constexpr int RECV_TIMEOUT_MS = 60;

/// @brief Silence toléré sur une session avant de la considérer perdue (ms).
/// @details Un câble débranché ne provoque aucune erreur TCP : le socket
/// reste ouvert et recv() ne fait qu'expirer. Sans cette limite, le serveur
/// resterait accroché à un client disparu et n'accepterait jamais la nouvelle
/// connexion.
constexpr int CLIENT_IDLE_TIMEOUT_MS = 1000;

/// @brief Largeur de capture de la caméra, en pixels.
constexpr int FRAME_WIDTH = 800;

/// @brief Hauteur de capture de la caméra, en pixels.
constexpr int FRAME_HEIGHT = 600;

/// @brief Cadence demandée à la caméra, en images par seconde.
constexpr int CAMERA_FPS = 30;

/// @brief Contrôleur GPIO auquel le bouton est relié.
/// @details Sur l'Odroid-C2, gpiochip1 est le bus principal (periphs-banks,
/// 119 lignes), qui porte les broches du connecteur physique.
constexpr char GPIO_CHIP[] = "gpiochip1";

/// @brief Numéro de la ligne du bouton-poussoir B1 sur ce contrôleur.
constexpr unsigned int GPIO_LINE = 92;

/// @brief Nom présenté par le noyau (colonne « consumer » de gpioinfo).
constexpr char GPIO_CONSUMER[] = "ele4205-bouton";

/// @brief true si l'appui tire la ligne vers 0 (montage avec pull-up).
constexpr bool BUTTON_ACTIVE_LOW = true;

/// @brief Période d'échantillonnage de la ligne du bouton, en microsecondes.
/// @details Un appui humain dure plusieurs dizaines de millisecondes, un
/// échantillon par milliseconde suffit largement à n'en manquer aucun tout en
/// gardant le coût processeur nul.
constexpr int BUTTON_POLL_US = 1000;

/// @brief Durée d'anti-rebond du bouton, en millisecondes.
constexpr int BUTTON_DEBOUNCE_MS = 200;

/// @brief Fichier iio exposant la valeur brute de la photorésistance.
/// @details Le pilote saradc publie un fichier par canal. Vérifier le canal
/// réellement câblé.
constexpr char LIGHT_SENSOR_PATH[] =
    "/sys/bus/iio/devices/iio:device0/in_voltage0_raw";

/// @brief Seuil sur la valeur brute du capteur en dessous duquel la scène est
/// jugée sombre.
constexpr int LIGHT_RAW_THRESHOLD = 968;

/// @brief true si la valeur brute augmente avec la luminosité.
/// @details Dépend du sens du pont diviseur. À inverser si le capteur rapporte
/// une valeur élevée dans le noir.
constexpr bool LIGHT_RAW_RISES = false;

/// @brief Seuil de luminosité moyenne de l'image, sur 255, en dessous duquel
/// l'image est jugée sombre.
constexpr double IMAGE_DARK_THRESHOLD = 40.0;

/// @brief Durée minimale de persistance d'un état avant transmission (ms).
constexpr int STATE_HOLD_MS = 200;

} // namespace Config