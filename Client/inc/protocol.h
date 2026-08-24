/// @file protocol.h
/// @brief Constantes du protocole applicatif, partagées par le client et le
/// serveur.
/// @author Jean-Claude Junior Raymond, Zakaria Chikri
/// @date 2026-08-12
#pragma once

#include <cstdint>

/// @brief Vocabulaire du protocole applicatif bâti au-dessus de TCP.
/// @details TCP ne transporte qu'un flux d'octets. C'est ce protocole qui
/// donne un sens à ces octets et délimite les messages.
///
/// Avec P2, GET_FRAME est suivie d'un en-tête binaire (frameId, jpegSize)
/// puis des données JPEG.
///
/// Avec P3, la réponse à GET_FRAME devient BUTTON_PRESS au lieu de FRAME_HDR
/// lorsqu'un appui sur le bouton a été détecté depuis la requête précédente.
/// La suite du message est identique : seul l'octet d'en-tête change.
///
/// Avec P4, toute réponse à GET_FRAME commence par l'octet d'en-tête suivi de
/// frameId. Seuls FRAME_HDR et BUTTON_PRESS transportent ensuite jpegSize et
/// les données JPEG : NO_LIGHT et SENSOR_ERROR s'arrêtent après frameId.
///
/// Avec P5, le protocole est inchangé. La robustesse est obtenue sans nouveau
/// message : une session interrompue est simplement abandonnée de part et
/// d'autre, puis une nouvelle connexion reprend le même dialogue.
///
/// Les identifiants sont répartis par plages :
/// - 1 à 99    : client vers serveur ;
/// - 100 à 199 : serveur vers client ;
/// - 200 à 255 : événements et erreurs.
namespace Protocol {

/// @brief Port TCP utilisé par le client et le serveur.
constexpr uint16_t PORT = 4099;

/// @brief GET_FRAME : demande la capture d'une image.
constexpr uint8_t GET_FRAME = 1;

/// @brief STOP : demande d'arrêt du programme.
constexpr uint8_t STOP = 2;

/// @brief FRAME_HDR : en-tête d'une image transmise.
constexpr uint8_t FRAME_HDR = 101;

/// @brief STOP_ACK : confirmation d'arrêt.
constexpr uint8_t STOP_ACK = 102;

/// @brief BUTTON_PRESS : en-tête d'une image associée à un appui sur le
/// bouton. Même structure que FRAME_HDR.
constexpr uint8_t BUTTON_PRESS = 200;

/// @brief NO_LIGHT : luminosité insuffisante, aucune image transmise.
constexpr uint8_t NO_LIGHT = 201;

/// @brief SENSOR_ERROR : incohérence capteur/image, aucune image transmise.
constexpr uint8_t SENSOR_ERROR = 202;

} // namespace Protocol