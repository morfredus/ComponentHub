/**
 * Utilitaires CSV génériques (RFC 4180 simplifié) — aucune dépendance
 * Arduino/ESP32. Gère les champs cités ("...") pouvant contenir le
 * délimiteur, un guillemet (échappé en "") ou un retour à la ligne.
 */

#pragma once
#include <string>
#include <vector>

namespace domain {

std::vector<std::vector<std::string>> parseCsv(const std::string& text, char delimiter = ';');

std::string csvField(const std::string& value, char delimiter = ';');
std::string joinCsvRow(const std::vector<std::string>& fields, char delimiter = ';');

} // namespace domain
