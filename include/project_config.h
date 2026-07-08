#pragma once

/**
 * project_config.h — Réglages globaux du framework.
 *
 * PROJECT_NAME / PROJECT_VERSION / BUILD_DATE / BUILD_TIME / GIT_COMMIT sont
 * normalement injectés par tools/build_info.py et tools/version_generator.py
 * via build_flags (platformio.ini). Les valeurs ci-dessous ne sont que des
 * valeurs de repli pour permettre une compilation manuelle directe.
 */

#ifndef PROJECT_NAME
#define PROJECT_NAME "ComponentHub"
#endif
#ifndef PROJECT_VERSION
#define PROJECT_VERSION "0.0.0-dev"
#endif
#ifndef BUILD_DATE
#define BUILD_DATE "unknown"
#endif
#ifndef BUILD_TIME
#define BUILD_TIME "unknown"
#endif
#ifndef GIT_COMMIT
#define GIT_COMMIT "unknown"
#endif

// --- Réseau ---
#define WEB_SERVER_PORT        80
#define MDNS_HOSTNAME           "componenthub"
#define WIFI_CONNECT_TIMEOUT_MS 15000
#define WIFI_PORTAL_AP_NAME     "ESP32-Setup"

// --- Temps (NTP) ---
#define TIME_ZONE   "UTC"
#define NTP_SERVER  "pool.ntp.org"

// --- Fonctionnalités optionnelles (commentez pour désactiver) ---
#define ENABLE_WIFI
#define ENABLE_WEB_SERVER
#define ENABLE_OTA
#define ENABLE_MDNS
#define ENABLE_TIME_SYNC   // horodatage correct des mouvements de stock (voir InventoryModule)

// BootLog — journal de redémarrage (raison du reset, derniers logs, état
// système avant crash). Module de débogage optionnel et autonome, voir
// src/modules/boot_log/boot_log.h pour le détail et la procédure de retrait.
#define ENABLE_BOOT_LOG
#ifdef ENABLE_BOOT_LOG
#define MAX_BOOT_LOG_ENTRIES        10
#define BOOT_LOG_BUFFER_LINES       20
#define BOOT_LOG_LINE_MAX_LEN       160
#define BOOT_LOG_STATS_INTERVAL_MS  30000
#endif

// --- Logs ---
#ifndef LOG_LEVEL
#define LOG_LEVEL 4     // 0=off 1=error 2=warn 3=info 4=debug
#endif
#ifndef LOG_BUFFER_SIZE
#define LOG_BUFFER_SIZE 100
#endif
