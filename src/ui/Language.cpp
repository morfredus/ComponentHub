#include "ui/Language.h"

#include <QSettings>
#include <QCoreApplication>
#include <QLocale>
#include <QTranslator>
#include <QDir>
#include <QFile>
#include <algorithm>
#include <QStringList>

namespace {

// The language of the source strings; it needs no catalog.
const QString kSourceLanguage = QStringLiteral("en");

// Each catalog translates this string into the name of its own language (for example
// "Español"), which the language selector shows.
const char* const kSelfName = CHUI_NOOP("English");

// Codes of the catalogs compiled into the executable (componenthub_<code>.qm).
QStringList catalogCodes() {
    const QString prefix = QStringLiteral("componenthub_");
    QStringList codes;
    for (const QString& file : QDir(QStringLiteral(":/i18n")).entryList({prefix + "*.qm"}))
        codes << file.mid(prefix.size(), file.size() - prefix.size() - 3);
    return codes;
}

}  // namespace

namespace chui {

QList<LanguageEntry> availableLanguages() {
    QList<LanguageEntry> list{{kSourceLanguage, QStringLiteral("English")}};
    for (const QString& code : catalogCodes()) {
        QTranslator t;
        if (!t.load(QStringLiteral(":/i18n/componenthub_%1.qm").arg(code))) continue;
        const QString name = t.translate("chui", kSelfName);
        list.append({code, name.isEmpty() ? code : name});
    }
    std::sort(list.begin(), list.end(), [](const LanguageEntry& a, const LanguageEntry& b) {
        return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
    });
    return list;
}

QString language() {
    QStringList codes{kSourceLanguage};
    codes += catalogCodes();

    QSettings settings("morfredus", "ComponentHub");
    const QString saved = settings.value("language").toString();
    if (codes.contains(saved)) return saved;

    // First start: the system language ("fr_CA" and "fr_FR" both give "fr").
    const QString system = QLocale::system().name().section('_', 0, 0);
    return codes.contains(system) ? system : kSourceLanguage;
}

void setLanguage(const QString& code) {
    QSettings("morfredus", "ComponentHub").setValue("language", code);
}

void applyLanguage() {
    // One translator for the whole process lifetime: remove it, reload it with the
    // wanted catalog, install it again. English has no catalog (it is the source text).
    static QTranslator translator;
    QCoreApplication::removeTranslator(&translator);
    const QString code = language();
    if (code != kSourceLanguage &&
        translator.load(QStringLiteral(":/i18n/componenthub_%1.qm").arg(code)))
        QCoreApplication::installTranslator(&translator);
}

QString docHtml(const QString& baseName) {
    for (const QString& code : {language(), kSourceLanguage}) {
        QFile f(QStringLiteral(":/doc/%1.%2.html").arg(baseName, code));
        if (f.open(QIODevice::ReadOnly)) return QString::fromUtf8(f.readAll());
    }
    return QString();
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
        return tr("Invalid URL (http:// expected): %1").arg(detail);
    if (code == "http.host_not_found")
        return tr("Host not found: %1").arg(detail);
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
        return tr("Skipped unsafe archive entry: %1").arg(name);
    if (code == "tar.write_failed")
        return tr("Could not restore file: %1").arg(name);
    if (code == "tar.name_too_long")
        return tr("File name too long for the archive: %1").arg(name);
    if (code == "tar.read_failed")
        return tr("Could not read file: %1").arg(name);
    if (code == "tar.invalid_name")
        return tr("File name not allowed in the archive: %1").arg(name);
    if (code == "tar.too_deep")
        return tr("Folder tree too deep for the archive: %1").arg(name);
    if (code == "tar.too_large")
        return tr("Data too large for a single archive: %1").arg(name);
    if (code == "tar.corrupt")
        return tr("The archive is damaged or incomplete.");
    if (code == "backup.attachment_invalid")
        return tr("Skipped damaged attachment (it will be downloaded again from the hub): %1").arg(name);
    if (code == "backup.apply_failed")
        return tr("Could not replace the data: %1").arg(name);
    if (code == "backup.not_a_backup" || code == "backup.no_database" || code == "backup.invalid_database")
        return tr("The archive does not contain a valid ComponentHub database.");
    if (code == "backup.snapshot_failed" || code == "backup.staging_failed" || code == "backup.copy_failed")
        return tr("Backup preparation failed: %1").arg(name);
    return error;
}

QString syncError(const QString& error) {
    const QStringList parts = error.split('|');
    const QString code = parts.value(0);
    const QString detail = parts.value(1);
    if (code == "sync.http_status") return tr("HTTP status: %1").arg(detail);
    if (code == "sync.unexpected_response") return tr("Unexpected response.");
    if (code == "sync.hub_unreachable") return tr("Hub unreachable (HTTP %1).").arg(detail);
    if (code == "sync.authentication_refused") return tr("Authentication refused (invalid token?).");
    if (code == "sync.reset_refused") return tr("Hub reset refused (HTTP %1).").arg(detail);
    if (code == "sync.push_failed") return tr("PUSH failed (HTTP %1).").arg(detail);
    if (code == "sync.push_invalid_response") return tr("Invalid PUSH response.");
    if (code == "sync.pull_failed") return tr("PULL failed (HTTP %1).").arg(detail);
    if (code == "sync.pull_invalid_response") return tr("Invalid PULL response.");
    if (code == "sync.cancelled") return tr("Synchronization cancelled.");
    if (code == "sync.database_error") return tr("Local database error: %1").arg(detail);
    return networkError(error);
}

QString tr(const char* sourceText, const char* disambiguation) {
    return QCoreApplication::translate("chui", sourceText, disambiguation);
}

QString tr(const QString& sourceText, const char* disambiguation) {
    return QCoreApplication::translate("chui", sourceText.toUtf8().constData(), disambiguation);
}

} // namespace chui
