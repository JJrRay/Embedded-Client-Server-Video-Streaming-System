/// @file camera.cpp
#include "camera.h"

#include <opencv2/opencv.hpp>

#include "config.h"

namespace Vision {

Camera::Camera(int device) : _capture(device) {
  if (_capture.isOpened()) {
    _capture.set(cv::CAP_PROP_FOURCC,
                 cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));
    _capture.set(cv::CAP_PROP_FRAME_WIDTH, Config::FRAME_WIDTH);
    _capture.set(cv::CAP_PROP_FRAME_HEIGHT, Config::FRAME_HEIGHT);
    _capture.set(cv::CAP_PROP_FPS, Config::CAMERA_FPS);
  }
}

bool Camera::isOpened() const { return _capture.isOpened(); }

std::optional<Capture> Camera::captureJpeg() {
  cv::Mat frame;
  if (!_capture.read(frame) || frame.empty()) {
    return std::nullopt;
  }

  Capture capture;

  // Métrique de luminosité : moyenne de tous les canaux de tous les pixels.
  // Une scène éclairée et une scène noire se distinguent par leur niveau
  // moyen, et cv::mean parcourt l'image une seule fois, sans conversion.
  const cv::Scalar channelMeans = cv::mean(frame);
  capture.brightness =
      (channelMeans[0] + channelMeans[1] + channelMeans[2]) / 3.0;

  // imencode remplit directement un vector d'octets.
  if (!cv::imencode(".jpg", frame, capture.jpeg)) {
    return std::nullopt;
  }
  return capture;
}

} // namespace Vision