/// @file image_store.cpp
#include "image_store.h"

#include <fstream>
#include <iostream>
#include <string>
#include <system_error>

#include "config.h"

namespace Storage {

ImageStore::ImageStore(const char *directory) : _dir(directory) {}

bool ImageStore::reset() const {
  std::error_code error;

  // remove_all supprime le répertoire et son contenu, s'il existe. Le
  // create_directories qui suit le recrée vide dans tous les cas.
  std::filesystem::remove_all(_dir, error);
  if (error) {
    std::cerr << "Impossible de vider " << _dir << " : " << error.message()
              << std::endl;
    return false;
  }

  std::filesystem::create_directories(_dir, error);
  if (error) {
    std::cerr << "Impossible de créer " << _dir << " : " << error.message()
              << std::endl;
    return false;
  }
  return true;
}

bool ImageStore::save(uint32_t frameId,
                      const std::vector<uint8_t> &jpeg) const {
  // Le numéro de frame rend chaque nom unique. Deux appuis successifs ne
  // peuvent pas écraser la même image.
  const auto path = _dir / (std::string(Config::IMAGE_PREFIX) +
                            std::to_string(frameId) + Config::IMAGE_EXTENSION);

  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  if (!file) {
    std::cerr << "Ouverture impossible : " << path << std::endl;
    return false;
  }

  // Les octets reçus forment déjà un JPEG valide. On les écrit tels quels,
  // sans décodage ni réencodage, ce qui garde la sauvegarde très courte et
  // n'interrompt pas l'affichage.
  file.write(reinterpret_cast<const char *>(jpeg.data()),
             static_cast<std::streamsize>(jpeg.size()));
  if (!file) {
    std::cerr << "Écriture incomplète : " << path << std::endl;
    return false;
  }

  std::cout << "Image sauvegardée : " << path << std::endl;
  return true;
}

} // namespace Storage