#pragma once

#include <QList>
#include <QString>

namespace chui {

enum class Currency { Euro, Dollar, Pound };

// A UI language: `code` is the file suffix of its catalog ("fr" for
// translations/componenthub_fr.ts), `name` is the language written in itself.
struct LanguageEntry {
    QString code;
    QString name;
};

// English (the language of the source strings) plus every catalog compiled into
// the executable. Adding translations/componenthub_xx.ts is enough to add an entry.
QList<LanguageEntry> availableLanguages();

// Code of the language in use: the saved choice, else the system language when a
// catalog exists for it, else "en".
QString language();
void setLanguage(const QString& code);

// Installs (or replaces) the translator of the current language; safe to call at runtime.
void applyLanguage();

// Long texts (help, about) live in resources/doc/<baseName>.<code>.html rather than in
// the catalogs. Falls back to English when the current language has no such file.
QString docHtml(const QString& baseName);

Currency currency();
void setCurrency(Currency currency);
QString currencyCode(Currency currency);
QString currencySymbol(Currency currency);
QString networkError(const QString& error);
QString archiveError(const QString& error);
QString syncError(const QString& error);

// Translates a source string (English). `disambiguation` tells two meanings of the same
// English text apart, e.g. tr("Import", "title") versus tr("Import") for a button.
QString tr(const char* sourceText, const char* disambiguation = nullptr);
QString tr(const QString& sourceText, const char* disambiguation = nullptr);

} // namespace chui

// Marks a string for translation without translating it now, for static tables whose
// entries are translated later with chui::tr(variable). scripts/i18n/update-catalogs.py
// collects these literals like the ones passed to chui::tr().
#define CHUI_NOOP(text) text
