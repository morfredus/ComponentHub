#include "ui/Language.h"

#include <QSettings>
#include <QCoreApplication>
#include <QLocale>
#include <QStringList>

namespace chui {

Language language() {
    QSettings settings("morfredus", "ComponentHub");
    if (settings.contains("language"))
        return settings.value("language").toString() == "fr"
                   ? Language::French : Language::English;

    // French is selected for every regional French locale, such as fr_FR
    // and fr_CA. All other system locales default to English.
    return QLocale::system().language() == QLocale::French
               ? Language::French : Language::English;
}

void setLanguage(Language value) {
    QSettings("morfredus", "ComponentHub")
        .setValue("language", value == Language::English ? "en" : "fr");
}

QString languageName(Language value) {
    return value == Language::English ? QStringLiteral("English")
                                      : QStringLiteral("Français");
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
    if (code == "http.cancelled") return tr("Cancelled.");
    if (code == "http.timeout") return tr("The hub took too long to answer.");
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
    if (code == "tar.name_too_long")
        return tr("File name too long for the archive: ") + name;
    if (code == "tar.read_failed")
        return tr("Could not read file: ") + name;
    if (code == "tar.invalid_name")
        return tr("File name not allowed in the archive: ") + name;
    if (code == "tar.too_deep")
        return tr("Folder tree too deep for the archive: ") + name;
    if (code == "tar.too_large")
        return tr("Data too large for a single archive: ") + name;
    if (code == "tar.corrupt")
        return tr("The archive is damaged or incomplete.");
    if (code == "backup.attachment_invalid")
        return tr("Skipped damaged attachment (it will be downloaded again from the hub): ") + name;
    if (code == "backup.apply_failed")
        return tr("Could not replace the data: ") + name;
    if (code == "backup.not_a_backup" || code == "backup.no_database" || code == "backup.invalid_database")
        return tr("The archive does not contain a valid ComponentHub database.");
    if (code == "backup.snapshot_failed" || code == "backup.staging_failed" || code == "backup.copy_failed")
        return tr("Backup preparation failed: ") + name;
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
    if (code == "sync.cancelled") return tr("Synchronization cancelled.");
    if (code == "sync.database_error") return tr("Local database error: ") + detail;
    return networkError(error);
}

QString tr(const char* sourceText) {
    return QCoreApplication::translate("chui", sourceText);
}

QString tr(const QString& sourceText) {
    return QCoreApplication::translate("chui", sourceText.toUtf8().constData());
}

} // namespace chui
