/// @file light_monitor.h
/// @brief Lecture de la photorésistance et vérification de la cohérence entre
/// le capteur et l'image capturée.
/// @author Jean-Claude Junior Raymond, Zakaria Chikri
/// @date 2026-08-13
/// @note Fait partie du module d'acquisition côté serveur. Isole l'accès à
/// l'interface iio du reste de la logique.
#pragma once

#include <chrono>
#include <optional>
#include <string>

namespace Lighting {

/// @brief État de la scène déduit du capteur et de l'image.
enum class SceneState {
  /// @brief Capteur et image cohérents : l'image est transmise.
  NORMAL,
  /// @brief Scène sombre confirmée par les deux sources.
  NO_LIGHT,
  /// @brief Capteur et image en désaccord.
  SENSOR_ERROR
};

/// @brief Compare la luminosité mesurée par la photorésistance à celle de
/// l'image et filtre les changements d'état trop courts.
/// @details Le capteur est lu par le fichier iio à chaque évaluation. Un
/// nouvel état n'est adopté que s'il persiste pendant la durée demandée ;
/// avant cela, c'est l'état précédent qui reste transmis au client.
class LightMonitor {
public:
  /// @brief Construit le moniteur et vérifie que le capteur est lisible.
  /// @param sensorPath Fichier iio exposant la valeur brute du capteur.
  /// @param rawThreshold Seuil sur la valeur brute séparant sombre et éclairé.
  /// @param rawRises true si la valeur brute augmente avec la luminosité.
  /// @param imageThreshold Seuil de luminosité moyenne de l'image, sur 255.
  /// @param holdMs Durée minimale de persistance d'un état, en millisecondes.
  LightMonitor(const char *sensorPath, int rawThreshold, bool rawRises,
               double imageThreshold, int holdMs);

  /// @brief Indique si le capteur a pu être lu au démarrage.
  /// @return true si la surveillance est active.
  bool valid() const { return _available; }

  /// @brief Met à jour l'état de la scène pour l'image qui vient d'être
  /// capturée.
  /// @param imageBrightness Luminosité moyenne de l'image, de 0 à 255.
  /// @return L'état à transmettre au client.
  SceneState update(double imageBrightness);

private:
  /// @brief Horloge monotone, insensible aux changements d'heure système.
  using Clock = std::chrono::steady_clock;

  /// @brief Lit la valeur brute du capteur.
  /// @return La valeur lue, ou std::nullopt si le fichier iio est absent ou
  /// illisible.
  std::optional<int> readRaw() const;

  /// @brief Croise la mesure du capteur et celle de l'image.
  /// @param imageBrightness Luminosité moyenne de l'image, de 0 à 255.
  /// @return L'état observé, avant filtrage temporel.
  SceneState classify(double imageBrightness) const;

  /// @brief Chemin du fichier iio du capteur.
  std::string _path;
  /// @brief Seuil sur la valeur brute du capteur.
  int _rawThreshold;
  /// @brief Sens de variation de la valeur brute avec la luminosité.
  bool _rawRises;
  /// @brief Seuil de luminosité moyenne de l'image.
  double _imageThreshold;
  /// @brief Durée minimale de persistance d'un état.
  std::chrono::milliseconds _hold;
  /// @brief true si le capteur a été lu correctement au démarrage.
  bool _available = false;
  /// @brief État actuellement transmis au client.
  SceneState _state = SceneState::NORMAL;
  /// @brief Dernier état observé, pas encore adopté.
  SceneState _candidate = SceneState::NORMAL;
  /// @brief Date de la première observation de l'état candidat.
  Clock::time_point _candidateSince = Clock::now();
};

} // namespace Lighting