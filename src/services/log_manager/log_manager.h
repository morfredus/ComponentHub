/**
 * LogManager — Journalisation unifiée (fusion Gateway Lab + MeteoHub)
 *
 * Sorties :
 *   - Port série (toujours, format horodaté avec niveau et tag)
 *   - Tampon mémoire circulaire (consultable via getEntry()/count(), exposé
 *     en JSON par WebManager sur /api/logs et en HTML sur /logs)
 *
 * Utilisation :
 *   LOG_DEBUG("MonModule", "x=%d", x);
 *   LOG_INFO("MonModule", "Démarrage OK");
 *   LOG_WARNING("MonModule", "Heap bas : %u", ESP.getFreeHeap());
 *   LOG_ERROR("MonModule", "Échec critique");
 *
 * Niveau de compilation (désactive les niveaux les plus verbeux) :
 *   build_flags = -D LOG_LEVEL=2   ; 0=off 1=error 2=warn 3=info(défaut) 4=debug
 */

#pragma once
#include <Arduino.h>
#include <vector>

#ifndef LOG_LEVEL
#define LOG_LEVEL 3
#endif

#ifndef LOG_BUFFER_SIZE
#define LOG_BUFFER_SIZE 100
#endif

struct LogEntry {
    unsigned long timestampMs;
    char level;       // 'D','I','W','E'
    String tag;
    String message;
};

class LogManager {
public:
    void log(char level, const char* tag, const char* fmt, va_list args);

    size_t count() const { return _entries.size(); }
    const LogEntry& entry(size_t index) const { return _entries[index]; }
    void clear() { _entries.clear(); }

    // Sérialise tout le tampon en JSON (utilisé par /api/logs)
    String toJson() const;

private:
    std::vector<LogEntry> _entries;
};

extern LogManager logMgr;

namespace LogDetail {
__attribute__((format(printf, 3, 4)))
inline void emit(char level, const char* tag, const char* fmt, ...) {
    va_list a;
    va_start(a, fmt);
    logMgr.log(level, tag, fmt, a);
    va_end(a);
}
}

#if LOG_LEVEL >= 4
#define LOG_DEBUG(tag, ...) LogDetail::emit('D', tag, __VA_ARGS__)
#else
#define LOG_DEBUG(tag, ...) do {} while (0)
#endif

#if LOG_LEVEL >= 3
#define LOG_INFO(tag, ...) LogDetail::emit('I', tag, __VA_ARGS__)
#else
#define LOG_INFO(tag, ...) do {} while (0)
#endif

#if LOG_LEVEL >= 2
#define LOG_WARNING(tag, ...) LogDetail::emit('W', tag, __VA_ARGS__)
#else
#define LOG_WARNING(tag, ...) do {} while (0)
#endif

#if LOG_LEVEL >= 1
#define LOG_ERROR(tag, ...) LogDetail::emit('E', tag, __VA_ARGS__)
#else
#define LOG_ERROR(tag, ...) do {} while (0)
#endif
