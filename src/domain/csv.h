/**
 * Utilitaires CSV génériques (RFC 4180 simplifié) — aucune dépendance
 * plateforme. Gère les champs cités ("...") pouvant contenir le
 * délimiteur, un guillemet (échappé en "") ou un retour à la ligne.
 */

#pragma once
#include <string>
#include <vector>

namespace domain {

// BOM UTF-8 : à placer en tête des fichiers CSV exportés pour que les
// tableurs (Excel/LibreOffice sous Windows notamment) reconnaissent l'UTF-8
// et affichent correctement « € » et les caractères accentués.
extern const char* const kUtf8Bom;

std::vector<std::vector<std::string>> parseCsv(const std::string& text, char delimiter = ';');

std::string csvField(const std::string& value, char delimiter = ';');
std::string joinCsvRow(const std::vector<std::string>& fields, char delimiter = ';');

} // namespace domain
