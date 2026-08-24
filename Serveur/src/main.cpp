/// @file main.cpp
#include <iostream>

#include "config.h"
#include "protocol.h"
#include "server_app.h"

int main() {
  Application::installSignalHandlers();

  Application::ServerApp app(Protocol::PORT, Config::RECV_TIMEOUT_MS);
  if (!app.valid()) {
    std::cerr << "Démarrage impossible" << std::endl;
    return 1;
  }

  app.run();
  return 0;
}