/// @file image_store.h
/// @brief Gestion du répertoire de sauvegarde et écriture des images sur
/// disque.
/// @author Jean-Claude Junior Raymond, Zakaria Chikri
/// @date 2026-08-12
/// @note Fait partie du module applicatif côté client. Gère l'accès au
/// système de fichiers en parallèle avec le reste de la logique.
#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

namespace Storage {

/// @brief Répertoire de sauvegarde des images associées à un appui.
/// @details Le répertoire est créé s'il n'existe pas et vidé au démarrage du
/// client. Les images sont écrites telles que reçues, sans réencodage (elles
/// sont déjà un JPEG complet).
class ImageStore {
public:
  /// @brief Construit le gestionnaire pour un répertoire donné.
  /// @param directory Chemin du répertoire, relatif au dossier d'exécution.
  explicit ImageStore(const char *directory);

  /// @brief Crée le répertoire s'il n'existe pas, ou le vide s'il existe.
  /// @return false si le répertoire n'a pas pu être préparé.
  bool reset() const;

  /// @brief Écrit une image sur disque sous le nom capture_frame_<id>.jpg.
  /// @param frameId Numéro de l'image, repris dans le nom du fichier.
  /// @param jpeg Données JPEG à écrire.
  /// @return false si l'écriture a échoué.
  bool save(uint32_t frameId, const std::vector<uint8_t> &jpeg) const;

private:
  /// @brief Chemin du répertoire de sauvegarde.
  std::filesystem::path _dir;
};

} // namespace Storage