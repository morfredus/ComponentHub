/**
 * Generic CSV utilities (simplified RFC 4180) - no platform
 * dependency. Handles quoted fields ("...") that may contain the
 * delimiter, a quote (escaped as "") or a line break.
 */

#pragma once
#include <string>
#include <vector>

namespace domain {

// UTF-8 BOM: put it at the start of exported CSV files so that
// spreadsheets (Excel/LibreOffice on Windows in particular) recognize UTF-8
// and display "€" and accented characters correctly.
extern const char* const kUtf8Bom;

std::vector<std::vector<std::string>> parseCsv(const std::string& text, char delimiter = ';');

std::string csvField(const std::string& value, char delimiter = ';');
std::string joinCsvRow(const std::vector<std::string>& fields, char delimiter = ';');

} // namespace domain
