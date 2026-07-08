#pragma once

/**
 * board_config.h — Brochage matériel par défaut (ESP32-S3 DevKitC-1 N16R8).
 *
 * Ce fichier ne contient aucune logique métier : uniquement des numéros de
 * broches génériques (I2C, SPI, boutons). Chaque projet utilisant le
 * framework est libre de le remplacer entièrement selon sa carte.
 */

#define BOARD_NAME "ESP32-S3 DevKitC-1"

// I2C — capteurs / écran (pull-up 4.7 kΩ obligatoire)
#define I2C_SDA_PIN 15
#define I2C_SCL_PIN 16

// SPI — écran / SD (si utilisé par le projet)
#define SPI_SCK_PIN  12
#define SPI_MOSI_PIN 11
#define SPI_MISO_PIN 13

// Boutons génériques
#define BUTTON_BOOT_PIN 0   // strapping — ne rien forcer LOW au démarrage

// Sortie LED/NeoPixel (si présente sur la carte)
#define NEOPIXEL_PIN 48

// ============================================================
// RAPPELS DE SÉCURITÉ ESP32-S3
// ============================================================
// - GPIO 3.3V uniquement (aucune broche 5V tolérante).
// - GPIO0 : strapping BOOT — ne rien connecter qui force LOW.
// - GPIO46 : entrée uniquement, attention au boot/JTAG.
// - I2C : pull-up 4.7 kΩ obligatoire.
// ============================================================
