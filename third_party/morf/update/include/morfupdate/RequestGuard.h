/*
 * morfUpdate - garde-fous purs de l'API locale et du helper privilegie
 * Copyright (C) 2026 morfredus
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QByteArray>
#include <QString>
#include <QUrl>

namespace morfupdate {

// Methodes qui modifient un etat : seules soumises au controle d'origine.
inline bool isWriteMethod(const QByteArray& method) {
    return method == "POST" || method == "PUT" || method == "PATCH" || method == "DELETE";
}

// Valeur d'un en-tete (insensible a la casse) dans le bloc d'en-tetes, ou vide.
inline QByteArray headerValue(const QByteArray& headerBlock, const QByteArray& name) {
    const QByteArray prefix = name.toLower() + ":";
    for (const QByteArray& line : headerBlock.split('\n')) {
        const QByteArray l = line.trimmed();
        if (l.toLower().startsWith(prefix))
            return l.mid(prefix.size()).trimmed();
    }
    return {};
}

// Anti-CSRF (specification commune du parc) : un navigateur envoie TOUJOURS `Origin`
// sur une ecriture cross-origin, les clients non navigateur (morfMonitor, curl) jamais.
// Ecriture dont l'Origin n'a pas le meme host:port que `Host` = page tierce : refus.
// L'origine opaque « null » differe de Host et est donc refusee.
inline bool isCrossOriginWrite(const QByteArray& method, const QByteArray& origin,
                               const QByteArray& host) {
    if (!isWriteMethod(method) || origin.trimmed().isEmpty())
        return false;
    QByteArray o = origin.trimmed().toLower();
    const int scheme = o.indexOf("://");
    if (scheme >= 0)
        o = o.mid(scheme + 3);
    return o != host.trimmed().toLower();
}

// Anti DNS-rebinding : le serveur n'ecoute que sur la boucle locale, mais une page
// hostile peut faire resoudre son propre nom vers 127.0.0.1 et envoyer `Host: evil.tld`.
// Seuls 127.0.0.1 et localhost (avec ou sans port) sont des Host legitimes. Un Host
// absent (client HTTP/1.0 non navigateur) est tolere : un navigateur en envoie toujours un.
inline bool isLocalHost(const QByteArray& hostHeader) {
    const QByteArray h = hostHeader.trimmed().toLower();
    if (h.isEmpty())
        return true;
    const int colon = h.lastIndexOf(':');
    const QByteArray name = colon >= 0 ? h.left(colon) : h;
    if (colon >= 0) {
        bool ok = false;
        const int port = h.mid(colon + 1).toInt(&ok);
        if (!ok || port < 1 || port > 65535)
            return false;
    }
    return name == "127.0.0.1" || name == "localhost";
}

// URL de sante acceptable pour le helper root : http, hote EXACTEMENT 127.0.0.1, port
// explicite, aucune information d'identification. Un simple startsWith("http://127.0.0.1:")
// laissait passer « http://127.0.0.1:80@evil.example/ » (l'hote reel est evil.example).
inline bool isLoopbackHealthUrl(const QString& text) {
    const QUrl url(text, QUrl::StrictMode);
    return url.isValid() && url.scheme() == QLatin1String("http")
        && url.host() == QLatin1String("127.0.0.1")
        && url.port() > 0
        && url.userInfo().isEmpty();
}

} // namespace morfupdate
