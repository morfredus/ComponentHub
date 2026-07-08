#include "storage_manager.h"
#include <LittleFS.h>
#include "../log_manager/log_manager.h"

static const char* TAG = "Storage";

StorageManager storage;

bool StorageManager::begin() {
    if (!LittleFS.begin(true)) {
        LOG_ERROR(TAG, "Échec montage LittleFS");
        return false;
    }
    LOG_INFO(TAG, "LittleFS prêt — %u/%u octets utilisés",
              (unsigned)LittleFS.usedBytes(), (unsigned)LittleFS.totalBytes());
    return true;
}

String StorageManager::readFile(const String& path) {
    File f = LittleFS.open(path, "r");
    if (!f) return "";
    String content = f.readString();
    f.close();
    return content;
}

bool StorageManager::writeFile(const String& path, const String& content) {
    File f = LittleFS.open(path, "w");
    if (!f) return false;
    size_t written = f.print(content);
    f.close();
    return written == content.length();
}

bool StorageManager::deleteFile(const String& path) {
    return LittleFS.remove(path);
}

bool StorageManager::renameFile(const String& from, const String& to) {
    return LittleFS.rename(from, to);
}

std::vector<StorageFileInfo> StorageManager::listFiles(const String& path) {
    std::vector<StorageFileInfo> out;
    File root = LittleFS.open(path);
    if (!root || !root.isDirectory()) return out;

    File f = root.openNextFile();
    while (f) {
        out.push_back({String(f.path()), f.size(), f.isDirectory()});
        f = root.openNextFile();
    }
    return out;
}

bool StorageManager::exists(const String& path) const { return LittleFS.exists(path); }
size_t StorageManager::totalBytes() const { return LittleFS.totalBytes(); }
size_t StorageManager::usedBytes() const { return LittleFS.usedBytes(); }
