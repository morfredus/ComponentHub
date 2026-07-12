// Archive TAR (ustar, fichiers normaux) — format identique à celui produit par
// le firmware ESP32 (noms de fichiers relatifs, sans « / » initial), donc les
// sauvegardes .tar sont interchangeables entre l'ESP32 et le desktop.
#pragma once
#include <string>
#include <vector>

namespace chdesktop::tar {

// Empaquette tous les fichiers réguliers du dossier `dir` (non récursif) dans
// l'archive `tarPath`. Les fichiers dont le nom se termine par une entrée de
// `excludeSuffixes` sont ignorés (ex. « .tmp »). Retourne false en cas d'échec
// d'écriture.
bool pack(const std::string& tarPath, const std::string& dir,
          const std::vector<std::string>& excludeSuffixes = {".tmp"});

// Extrait l'archive `tarPath` dans le dossier `dir` (écrase les fichiers
// existants). Renseigne `errors` et renvoie le nombre de fichiers restaurés.
// `looksLikeTar` permet de vérifier le format avant d'écraser quoi que ce soit.
bool looksLikeTar(const std::string& tarPath);
int  extract(const std::string& tarPath, const std::string& dir, std::vector<std::string>& errors);

} // namespace chdesktop::tar
