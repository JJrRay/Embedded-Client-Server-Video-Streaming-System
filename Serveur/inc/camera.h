/// @file camera.h
/// @brief Capture d'images depuis la caméra USB et encodage JPEG.
/// @author Jean-Claude Junior Raymond, Zakaria Chikri
/// @date 2026-08-12
/// @note Fait partie du module d'acquisition côté serveur. Encapsule
/// cv::VideoCapture pour isoler OpenCV du reste de la logique.
#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include <opencv2/opencv.hpp>

namespace Vision {

/// @brief Résultat d'une capture réussie.
struct Capture {
  /// @brief Octets JPEG de l'image capturée.
  std::vector<uint8_t> jpeg;
  /// @brief Luminosité moyenne de l'image, de 0 à 255.
  double brightness = 0.0;
};

/// @brief Encapsule la caméra USB et l'encodage JPEG des images capturées.
/// @details La résolution est fixée à 800x600. La caméra est ouverte à la
/// construction et libérée à la destruction (RAII).
class Camera {
public:
  /// @brief Ouvre la caméra et fixe la résolution à 800x600.
  /// @param device Index du périphérique (0 pour la première caméra USB).
  explicit Camera(int device = 0);

  /// @brief Indique si la caméra a été ouverte correctement.
  /// @return true si la caméra est prête.
  bool isOpened() const;

  /// @brief Capture une image, mesure sa luminosité et l'encode en JPEG.
  /// @return La capture, ou std::nullopt si la capture ou l'encodage a échoué.
  std::optional<Capture> captureJpeg();

private:
  /// @brief Flux de la caméra USB.
  cv::VideoCapture _capture;
};

} // namespace Vision