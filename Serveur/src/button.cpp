/// @file button.cpp
#include "button.h"

#include <gpiod.h>

#include <cerrno>
#include <cstring>
#include <iostream>

#include "config.h"

namespace Gpio {

ButtonMonitor::ButtonMonitor(const char *chipName, unsigned int lineOffset,
                             bool activeLow, int debounceMs, int pollPeriodUs)
    : _pressedLevel(activeLow ? 0 : 1), _pollPeriod(pollPeriodUs),
      _debounce(debounceMs) {
  _chip = gpiod_chip_open_by_name(chipName);
  if (_chip == nullptr) {
    std::cout << "GPIO : ouverture de " << chipName
              << " impossible : " << std::strerror(errno) << " (errno " << errno
              << ")" << std::endl;
    return;
  }

  gpiod_line *const line = gpiod_chip_get_line(_chip, lineOffset);
  if (line == nullptr) {
    std::cout << "GPIO : ligne " << lineOffset
              << " introuvable : " << std::strerror(errno) << " (errno "
              << errno << ")" << std::endl;
    return;
  }

  // Le contrôleur de l'Odroid-C2 n'expose pas d'interruption sur ces lignes,
  // donc la lecture périodique est la seule méthode disponible.
  if (gpiod_line_request_input(line, Config::GPIO_CONSUMER) < 0) {
    std::cout << "GPIO : demande d'entrée refusée sur " << chipName << ":"
              << lineOffset << " : " << std::strerror(errno) << " (errno "
              << errno << ")" << std::endl;
    return;
  }

  _line = line;
  _running.store(true);
  _worker = std::thread(&ButtonMonitor::watchLoop, this);
}

ButtonMonitor::~ButtonMonitor() {
  _running.store(false);
  if (_worker.joinable()) {
    _worker.join();
  }
  if (_line != nullptr) {
    gpiod_line_release(_line);
  }
  if (_chip != nullptr) {
    gpiod_chip_close(_chip);
  }
}

void ButtonMonitor::watchLoop() {
  // État de départ. Sans cette lecture initiale, un bouton déjà relâché
  // paraîtrait avoir changé d'état au premier tour.
  int previous = gpiod_line_get_value(_line);
  if (previous < 0) {
    previous = _pressedLevel; // Lecture ratée : suppose appuyé.
  }

  // Doit survivre entre les tours : l'appui et le relâchement sont détectés à
  // deux itérations différentes.
  bool pressSeen = false;

  while (_running.load()) {
    std::this_thread::sleep_for(_pollPeriod);

    const int value = gpiod_line_get_value(_line);
    if (value < 0 || value == previous) {
      continue; // Lecture en erreur, ou niveau inchangé.
    }
    previous = value;

    if (value == _pressedLevel) {
      pressSeen = true;
      continue; // Appui : on attend le relâchement.
    }
    if (!pressSeen) {
      continue; // Relâchement sans appui préalable.
    }
    pressSeen = false;

    // Anti-rebond. On ne retient que le premier de chaque relâchement.
    const auto now = Clock::now();
    if (now - _lastAccepted < _debounce) {
      continue;
    }
    _lastAccepted = now;
    _pressed.store(true);
  }
}

} // namespace Gpio