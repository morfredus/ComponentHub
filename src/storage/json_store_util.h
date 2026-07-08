/**
 * JsonStore — Petit utilitaire partagé par les dépôts JSON (component/
 * location/stock_movement). Chaque fichier LittleFS a la forme :
 *   { "nextId": <int>, "items": [ {...}, {...} ] }
 *
 * Seule cette couche (src/storage/) connaît LittleFS/ArduinoJson : le
 * domaine (src/domain/) ne voit que des std::vector<Component> etc.
 */

#pragma once
#include <ArduinoJson.h>
#include "../services/storage_manager/storage_manager.h"
#include "../services/log_manager/log_manager.h"

namespace JsonStore {

inline JsonDocument load(const char* path) {
    JsonDocument doc;
    if (storage.exists(path)) {
        String raw = storage.readFile(path);
        DeserializationError err = deserializeJson(doc, raw);
        // Un fichier illisible (flash corrompue, écriture interrompue par une
        // coupure d'alimentation) ne doit jamais se traduire silencieusement
        // par un inventaire vide : on le signale explicitement dans les logs.
        if (err && raw.length() > 0) {
            LOG_ERROR("JsonStore", "Fichier corrompu ou illisible : %s (%s) — %u octets ignorés",
                      path, err.c_str(), (unsigned)raw.length());
            doc.clear();
        }
    }
    if (doc["items"].isNull()) doc["items"].to<JsonArray>();
    if (doc["nextId"].isNull()) doc["nextId"] = 1;
    return doc;
}

inline bool save(const char* path, JsonDocument& doc) {
    String out;
    serializeJson(doc, out);

    // Écriture atomique : on écrit d'abord dans un fichier temporaire, puis
    // on le renomme par-dessus l'original. Si l'alimentation est coupée
    // pendant l'écriture, seul le .tmp est incomplet — le fichier de données
    // existant reste intact tant que le renommage n'a pas eu lieu.
    String tmpPath = String(path) + ".tmp";
    if (!storage.writeFile(tmpPath, out)) return false;

    if (!storage.renameFile(tmpPath, path)) {
        storage.deleteFile(tmpPath);
        return false;
    }
    return true;
}

inline int takeNextId(JsonDocument& doc) {
    int id = doc["nextId"] | 1;
    doc["nextId"] = id + 1;
    return id;
}

} // namespace JsonStore
