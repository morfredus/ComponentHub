// Sauvegarde / restauration complète du dossier de données (base SQLite + fichiers annexes).
//
// Pourquoi ce module : la base tourne en WAL et reste ouverte pendant la sauvegarde. Copier le
// fichier brut (et ses -wal/-shm) donne un état incohérent, et écraser une base ouverte à la
// restauration la corrompt silencieusement. Ici :
//   - la sauvegarde archive un INSTANTANÉ (VACUUM INTO), jamais les -wal/-shm ;
//   - la restauration est en deux temps : validation + mise en attente (base encore ouverte),
//     puis remplacement au prochain démarrage, avant l'ouverture de la base.
// Sans Qt : testable en ctest.
#pragma once
#include <map>
#include <string>
#include <vector>

#include "storage/SQLiteRepositories.h"

namespace chdesktop::backup {

// Nom du fichier de base dans le dossier de données et dans l'archive.
inline constexpr const char* kDbName = "componenthub.sqlite";
// Sous-dossier où une restauration validée attend le prochain démarrage.
inline constexpr const char* kPendingDir = "restore-pending";
// Sous-dossier des pièces jointes (magasin adressé par contenu), dans le dossier de données et
// dans l'archive.
inline constexpr const char* kAttachmentsDir = "attachments";

// Crée l'archive `tarPath` : instantané cohérent de la base + autres fichiers réguliers du
// dossier (hors anciens .bak) + `extraFiles` (nom -> contenu, ex. la configuration) + le dossier
// attachments/ (récursif). `error` reçoit un code « xxx|détail » en cas d'échec.
bool create(SQLiteDatabase& db, const std::string& dataDir, const std::string& tarPath,
            const std::map<std::string, std::string>& extraFiles, std::string& error);

// Extrait `tarPath` dans <dataDir>/restore-pending et valide la base qu'il contient (présence,
// integrity_check, table components) et les pièces jointes (nom = SHA-256 du contenu, sinon
// écartées avec une alerte). Ne touche PAS aux données courantes. `warnings` reçoit les entrées
// ignorées. À l'échec, le dossier d'attente est supprimé.
bool stageRestore(const std::string& tarPath, const std::string& dataDir,
                  std::vector<std::string>& warnings, std::string& error);

// À appeler au démarrage AVANT d'ouvrir la base : applique une restauration en attente
// (ancienne base conservée en .bak, -wal/-shm supprimés ; ancien attachments/ mis de côté en
// attachments.bak, jamais supprimé avant le succès, remis en place si une étape échoue). Une
// archive sans attachments/ (1.11 et antérieures) laisse les pièces jointes locales intactes.
// Retourne true s'il n'y avait rien ou si tout s'est bien passé ; `applied` indique si un
// remplacement a eu lieu.
bool applyPendingRestore(const std::string& dataDir, bool& applied, std::string& error);

} // namespace chdesktop::backup
