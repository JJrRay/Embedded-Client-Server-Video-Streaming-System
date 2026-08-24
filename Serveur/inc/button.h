/// @file button.h
/// @brief Surveillance d'un bouton-poussoir relié à un GPIO.
/// @author Jean-Claude Junior Raymond, Zakaria Chikri
/// @date 2026-08-13
/// @note Fait partie du module d'acquisition côté serveur.
#pragma once

#include <atomic>
#include <chrono>
#include <thread>

/// @brief Déclarés ici pour garder <gpiod.h> hors de l'interface publique.
struct gpiod_chip;
struct gpiod_line;

namespace Gpio {

/// @brief Surveille un bouton-poussoir sur un GPIO et mémorise les appuis.
/// @details La ligne est échantillonnée périodiquement dans un fil dédié. Le
/// fil fait que ni la capture d'images ni la communication réseau n'attendent
/// jamais après le GPIO.
class ButtonMonitor {
public:
  /// @brief Ouvre la ligne GPIO en entrée et démarre le fil de surveillance.
  /// @param chipName Nom du contrôleur GPIO (gpiochip1).
  /// @param lineOffset Numéro de la ligne sur ce contrôleur.
  /// @param activeLow true si l'appui tire la ligne vers 0.
  /// @param debounceMs Durée d'anti-rebond, en millisecondes.
  /// @param pollPeriodUs Période d'échantillonnage, en microsecondes.
  ButtonMonitor(const char *chipName, unsigned int lineOffset, bool activeLow,
                int debounceMs, int pollPeriodUs);

  /// @brief Arrête le fil de surveillance et libère la ligne GPIO.
  ~ButtonMonitor();

  /// @brief Copie interdite. La classe possède une ressource GPIO et un fil.
  /// @param other Instance source.
  ButtonMonitor(const ButtonMonitor &other) = delete;
  /// @brief Affectation interdite.
  /// @param other Instance source.
  /// @return Référence sur l'instance.
  ButtonMonitor &operator=(const ButtonMonitor &other) = delete;

  /// @brief Indique si la ligne GPIO a été ouverte correctement.
  /// @return true si la surveillance est active.
  bool valid() const { return _line != nullptr; }

  /// @brief Consomme un appui en attente, s'il y en a un.
  /// @details Lecture et remise à zéro. Un appui est donc rapporté à un seul
  /// appelant, une seule fois.
  /// @return true si un appui était en attente.
  bool takePress() { return _pressed.exchange(false); }

private:
  /// @brief Horloge monotone, insensible aux changements d'heure système.
  using Clock = std::chrono::steady_clock;

  /// @brief Boucle du fil de surveillance, échantillonne la ligne et arme le
  /// drapeau sur transition vers l'état appuyé, après filtrage anti-rebond.
  void watchLoop();

  /// @brief Contrôleur GPIO ouvert, nullptr si l'ouverture a échoué.
  gpiod_chip *_chip = nullptr;
  /// @brief Ligne du bouton, nullptr si la demande a échoué.
  gpiod_line *_line = nullptr;
  /// @brief Niveau logique correspondant à l'état appuyé.
  int _pressedLevel;
  /// @brief Période d'échantillonnage de la ligne.
  std::chrono::microseconds _pollPeriod;
  /// @brief Durée d'anti-rebond.
  std::chrono::milliseconds _debounce;
  /// @brief Date du dernier appui accepté. Utilisée par le fil uniquement.
  Clock::time_point _lastAccepted{};
  /// @brief Appui détecté et non encore consommé.
  std::atomic<bool> _pressed{false};
  /// @brief Faux demande au fil de surveillance de se terminer.
  std::atomic<bool> _running{false};
  /// @brief Fil de surveillance du GPIO.
  std::thread _worker;
};

} // namespace Gpio