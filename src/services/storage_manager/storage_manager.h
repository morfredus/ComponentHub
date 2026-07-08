/**
 * StorageManager — API générique de fichiers (LittleFS), upload/download web.
 *
 * Le framework n'expose que LittleFS par défaut. Un projet métier peut
 * étendre/remplacer ce service (ex : ajouter une carte SD) sans modifier
 * WebManager, qui ne connaît que cette interface.
 */

#pragma once
#include <Arduino.h>
#include <FS.h>
#include <vector>

struct StorageFileInfo {
    String name;
    size_t size;
    bool   isDir;
};

class StorageManager {
public:
    bool begin();

    String readFile(const String& path);
    bool   writeFile(const String& path, const String& content);
    bool   deleteFile(const String& path);
    bool   renameFile(const String& from, const String& to);
    std::vector<StorageFileInfo> listFiles(const String& path = "/");

    bool   exists(const String& path) const;
    size_t totalBytes() const;
    size_t usedBytes() const;
};

extern StorageManager storage;
