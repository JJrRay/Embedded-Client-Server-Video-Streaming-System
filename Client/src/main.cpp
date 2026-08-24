/// @file main.cpp
#include "client_app.h"

int main() {
  Application::ClientApp app;
  if (!app.initialize()) {
    return 1;
  }

  app.run();
  return 0;
}