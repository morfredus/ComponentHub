#pragma once

#include <QString>

namespace chui {

enum class Language { French, English, Spanish };
enum class Currency { Euro, Dollar, Pound };

Language language();
void setLanguage(Language language);
QString languageCode(Language language);
Language languageFromCode(const QString& code);
QString languageName(Language language);
Currency currency();
void setCurrency(Currency currency);
QString currencyCode(Currency currency);
QString currencySymbol(Currency currency);
QString networkError(const QString& error);
QString archiveError(const QString& error);
QString syncError(const QString& error);
QString tr(const char* sourceText);
QString tr(const QString& sourceText);

} // namespace chui
