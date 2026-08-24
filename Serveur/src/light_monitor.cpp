/// @file light_monitor.cpp
#include "light_monitor.h"

#include <fstream>
#include <iostream>

namespace Lighting {

LightMonitor::LightMonitor(const char *sensorPath, int rawThreshold,
                           bool rawRises, double imageThreshold, int holdMs)
    : _path(sensorPath), _rawThreshold(rawThreshold), _rawRises(rawRises),
      _imageThreshold(imageThreshold), _hold(holdMs) {
  _available = readRaw().has_value();
  if (!_available) {
    std::cout << "Capteur de luminosité indisponible : " << _path << std::endl;
  }
}

std::optional<int> LightMonitor::readRaw() const {
  // Le fichier iio est relu à chaque appel. Le pilote échantillonne le canal
  // au moment de l'ouverture, une valeur mise en cache serait figée.
  std::ifstream file(_path);
  int raw = 0;
  if (!(file >> raw)) {
    return std::nullopt;
  }
  return raw;
}

SceneState LightMonitor::classify(double imageBrightness) const {
  const auto raw = readRaw();
  if (!raw) {
    // Sans capteur, aucune incohérence ne peut être établie. Le flux vidéo
    // continue plutôt que de signaler une erreur qui n'est pas démontrée.
    return SceneState::NORMAL;
  }

  const bool sensorDark =
      _rawRises ? (*raw < _rawThreshold) : (*raw > _rawThreshold);
  const bool imageDark = imageBrightness < _imageThreshold;

  if (sensorDark && imageDark) {
    return SceneState::NO_LIGHT;
  }
  if (sensorDark != imageDark) {
    return SceneState::SENSOR_ERROR;
  }
  return SceneState::NORMAL;
}

SceneState LightMonitor::update(double imageBrightness) {
  const SceneState observed = classify(imageBrightness);

  // Un état doit être observé sans interruption pendant _hold avant d'être
  // adopté. Une perturbation plus courte laisse l'état précédent en place.
  if (observed != _candidate) {
    _candidate = observed;
    _candidateSince = Clock::now();
  }
  if (_candidate != _state && Clock::now() - _candidateSince >= _hold) {
    _state = _candidate;
  }
  return _state;
}

} // namespace Lighting