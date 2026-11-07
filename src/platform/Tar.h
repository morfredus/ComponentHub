// Archive TAR (ustar) : fichiers réguliers et dossiers, noms relatifs avec « / » (jamais de « / »
// initial). Sert aux sauvegardes complètes de l'atelier (base, configuration, pièces jointes).
#pragma once
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace chdesktop::tar {

// Plafonds de sécurité : un dossier de données démesuré ou un arbre pathologique doit produire
// un message clair, pas une archive de plusieurs dizaines de Gio ni un parcours sans fin.
inline constexpr std::uint64_t kDefaultMaxArchiveBytes = 16ull * 1024 * 1024 * 1024;  // 16 Gio
inline constexpr std::uint64_t kMaxFileBytes = 077777777777ull;  // limite ustar (11 chiffres octaux)
inline constexpr int kMaxDepth = 8;                               // composantes d'un chemin

// Dossier supplémentaire à archiver sous le nom `archiveName` (ex. {"attachments", "<dir>/attachments"}).
struct TreeSource {
    std::string archiveName;
    std::string dir;
};

// Empaquette récursivement `dir` (et chaque `extraTrees`) dans l'archive `tarPath`. Les chemins sont
// relatifs, séparés par « / » ; les dossiers (même vides) sont archivés ; les liens symboliques sont
// ignorés. Les fichiers dont le nom se termine par une entrée de `excludeSuffixes` (ex. « .tmp ») ou
// contient « .tmp. » (écriture atomique en cours) sont ignorés. Retourne false en cas d'échec ; `error`
// reçoit alors un code « tar.xxx|détail » (name_too_long, too_deep, invalid_name, too_large,
// read_failed, write_failed) et l'archive partielle est supprimée.
bool pack(const std::string& tarPath, const std::string& dir,
          const std::vector<std::string>& excludeSuffixes = {".tmp"}, std::string* error = nullptr,
          const std::vector<TreeSource>& extraTrees = {},
          std::uint64_t maxTotalBytes = kDefaultMaxArchiveBytes);

// Vérifie rapidement le format avant d'écraser quoi que ce soit.
bool looksLikeTar(const std::string& tarPath);

// Extrait l'archive `tarPath` dans le dossier `dir` (écrase les fichiers existants) et renvoie le
// nombre de fichiers restaurés. Tout chemin qui sortirait de `dir` est refusé : « .. », chemin absolu,
// antislash, « : », noms réservés Windows (CON, NUL, COM1...), liens, périphériques, profondeur
// excessive. `errors` reçoit « tar.unsafe_entry|nom » (entrée ignorée), « tar.write_failed|nom »
// (écriture ou données tronquées) ou « tar.corrupt » (en-tête invalide, archive sans marque de fin).
int extract(const std::string& tarPath, const std::string& dir, std::vector<std::string>& errors);

} // namespace chdesktop::tar
