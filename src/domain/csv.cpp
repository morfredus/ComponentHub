#include "csv.h"

namespace domain {

std::vector<std::vector<std::string>> parseCsv(const std::string& text, char delimiter) {
    std::vector<std::vector<std::string>> rows;
    std::vector<std::string> row;
    std::string field;
    bool inQuotes = false;
    size_t i = 0;
    size_t n = text.size();

    auto endField = [&]() { row.push_back(field); field.clear(); };
    auto endRow = [&]() { endField(); rows.push_back(row); row.clear(); };

    while (i < n) {
        char c = text[i];
        if (inQuotes) {
            if (c == '"') {
                if (i + 1 < n && text[i + 1] == '"') { field.push_back('"'); i += 2; continue; }
                inQuotes = false;
                i++;
                continue;
            }
            field.push_back(c);
            i++;
            continue;
        }
        if (c == '"') { inQuotes = true; i++; continue; }
        if (c == delimiter) { endField(); i++; continue; }
        if (c == '\r') { i++; continue; }
        if (c == '\n') { endRow(); i++; continue; }
        field.push_back(c);
        i++;
    }
    if (!field.empty() || !row.empty()) endRow();

    return rows;
}

std::string csvField(const std::string& value, char delimiter) {
    bool needsQuote = value.find(delimiter) != std::string::npos ||
                      value.find('"') != std::string::npos ||
                      value.find('\n') != std::string::npos;
    if (!needsQuote) return value;

    std::string out = "\"";
    for (char c : value) {
        if (c == '"') out += "\"\"";
        else out += c;
    }
    out += "\"";
    return out;
}

std::string joinCsvRow(const std::vector<std::string>& fields, char delimiter) {
    std::string out;
    for (size_t i = 0; i < fields.size(); i++) {
        if (i) out += delimiter;
        out += csvField(fields[i], delimiter);
    }
    return out;
}

} // namespace domain
