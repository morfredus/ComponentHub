#include "ui/Language.h"

#include <QSettings>
#include <QCoreApplication>
#include <QLocale>
#include <QStringList>

namespace chui {

Language languageFromCode(const QString& code) {
    if (code == "fr") return Language::French;
    if (code == "es") return Language::Spanish;
    return Language::English;
}

QString languageCode(Language value) {
    switch (value) {
    case Language::French: return QStringLiteral("fr");
    case Language::Spanish: return QStringLiteral("es");
    default: return QStringLiteral("en");
    }
}

Language language() {
    QSettings settings("morfredus", "ComponentHub");
    if (settings.contains("language"))
        return languageFromCode(settings.value("language").toString());

    // French and Spanish are selected for every regional variant of the
    // system locale (fr_FR, fr_CA, es_ES, es_MX...). All other system
    // locales default to English.
    switch (QLocale::system().language()) {
    case QLocale::French: return Language::French;
    case QLocale::Spanish: return Language::Spanish;
    default: return Language::English;
    }
}

void setLanguage(Language value) {
    QSettings("morfredus", "ComponentHub").setValue("language", languageCode(value));
}

QString languageName(Language value) {
    switch (value) {
    case Language::French: return QStringLiteral("Français");
    case Language::Spanish: return QStringLiteral("Español");
    default: return QStringLiteral("English");
    }
}

Currency currency() {
    QSettings settings("morfredus", "ComponentHub");
    if (settings.contains("currency")) {
        const QString code = settings.value("currency").toString();
        if (code == "GBP") return Currency::Pound;
        if (code == "USD") return Currency::Dollar;
        return Currency::Euro;
    }

    const QString systemCode = QLocale::system().currencySymbol(QLocale::CurrencyIsoCode);
    if (systemCode == "GBP") return Currency::Pound;
    if (systemCode == "EUR") return Currency::Euro;
    return Currency::Dollar;
}

void setCurrency(Currency value) {
    QSettings("morfredus", "ComponentHub")
        .setValue("currency", currencyCode(value));
}

QString currencyCode(Currency value) {
    switch (value) {
    case Currency::Pound: return QStringLiteral("GBP");
    case Currency::Dollar: return QStringLiteral("USD");
    default: return QStringLiteral("EUR");
    }
}

QString currencySymbol(Currency value) {
    switch (value) {
    case Currency::Pound: return QStringLiteral("£");
    case Currency::Dollar: return QStringLiteral("$");
    default: return QStringLiteral("€");
    }
}

QString networkError(const QString& error) {
    const QStringList parts = error.split('|');
    const QString code = parts.value(0);
    const QString detail = parts.value(1);
    if (code == "http.invalid_url")
        return tr("Invalid URL (http:// expected): ") + detail;
    if (code == "http.host_not_found")
        return tr("Host not found: ") + detail;
    if (code == "http.socket_failed") return tr("Socket creation failed.");
    if (code == "http.connection_refused")
        return tr("Connection refused by %1 (hub stopped?).").arg(detail);
    if (code == "http.send_failed") return tr("Failed to send the request.");
    if (code == "http.empty_response") return tr("Empty response (timeout?).");
    if (code == "http.invalid_response") return tr("Invalid HTTP response.");
    return error;
}

QString archiveError(const QString& error) {
    const QStringList parts = error.split('|');
    const QString code = parts.value(0);
    const QString name = parts.value(1);
    if (code == "tar.unsafe_entry")
        return tr("Skipped unsafe archive entry: ") + name;
    if (code == "tar.write_failed")
        return tr("Could not restore file: ") + name;
    return error;
}

QString syncError(const QString& error) {
    const QStringList parts = error.split('|');
    const QString code = parts.value(0);
    const QString detail = parts.value(1);
    if (code == "sync.http_status") return tr("HTTP status: ") + detail;
    if (code == "sync.unexpected_response") return tr("Unexpected response.");
    if (code == "sync.hub_unreachable") return tr("Hub unreachable (HTTP %1).").arg(detail);
    if (code == "sync.authentication_refused") return tr("Authentication refused (invalid token?).");
    if (code == "sync.reset_refused") return tr("Hub reset refused (HTTP %1).").arg(detail);
    if (code == "sync.push_failed") return tr("PUSH failed (HTTP %1).").arg(detail);
    if (code == "sync.push_invalid_response") return tr("Invalid PUSH response.");
    if (code == "sync.pull_failed") return tr("PULL failed (HTTP %1).").arg(detail);
    if (code == "sync.pull_invalid_response") return tr("Invalid PULL response.");
    return networkError(error);
}

QString tr(const char* sourceText) {
    return QCoreApplication::translate("chui", sourceText);
}

QString tr(const QString& sourceText) {
    return QCoreApplication::translate("chui", sourceText.toUtf8().constData());
}

} // namespace chui
