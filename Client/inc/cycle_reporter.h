/// @file cycle_reporter.h
/// @brief Mesure et affichage périodique des statistiques de cycle.
/// @author Jean-Claude Junior Raymond, Zakaria Chikri
/// @date 2026-08-12
/// @note Fait partie du module applicatif côté client. Utilisé par ClientApp
/// pour afficher une ligne de statut par seconde.
#pragma once

#include <chrono>

namespace Metrics {

/// @brief Accumule la durée des cycles et affiche un résumé une fois par
/// seconde.
/// @details La ligne affichée contient la date et l'heure, le nombre de cycles
/// par seconde et la durée moyenne d'un cycle. Les compteurs sont remis à zéro
/// après chaque affichage. beginCycle() et endCycle() encadrent chaque
/// itération de la boucle principale.
class CycleReporter {
public:
  /// @brief Marque le début d'un cycle.
  void beginCycle();

  /// @brief Marque la fin d'un cycle et affiche un résumé si au moins une
  /// seconde s'est écoulée depuis le dernier.
  void endCycle();

  /// @brief Remet les compteurs à zéro.
  /// @details Appelé après une coupure : les cycles écoulés pendant celle-ci
  /// fausseraient la moyenne de la seconde en cours.
  void reset();

private:
  /// @brief Horloge monotone, insensible aux changements d'heure système.
  using Clock = std::chrono::steady_clock;
  /// @brief Début du cycle courant.
  Clock::time_point _cycleStart{};
  /// @brief Date du dernier affichage.
  Clock::time_point _lastReport = Clock::now();
  /// @brief Cycles depuis l'affichage.
  int _cyclesSinceReport = 0;
  /// @brief Somme des durées, en millisecondes.
  double _accumulatedMs = 0.0;
};

} // namespace Metrics