#pragma once

#include <QString>
#include <QUrl>
#include <QUrlQuery>
#include <QStringList>

namespace CustomFeatures {

// Очистка URL от рекламных и трекинговых параметров (UTM, si, ref, fbclid, etc.)
inline QString CleanTrackingParameters(const QString &rawUrl) {
    QUrl url(rawUrl);
    if (!url.isValid() || url.scheme().isEmpty()) {
        return rawUrl;
    }

    // Список известных трекинговых query-параметров
    static const QStringList kTrackingParams = {
        "utm_source",
        "utm_medium",
        "utm_campaign",
        "utm_term",
        "utm_content",
        "si",          // Spotify / YouTube share tracker
        "feature",     // YouTube share tracker
        "ref",
        "ref_src",
        "fbclid",      // Facebook Click ID
        "gclid",       // Google Click ID
        "dclid",
        "msclkid",     // Microsoft Click ID
        "yclid",       // Yandex Click ID
        "_openstat",   // Openstat tracker
        "igshid",      // Instagram share tracker
        "twclid"       // Twitter Click ID
    };

    if (!url.hasQuery()) {
        return rawUrl;
    }

    QUrlQuery query(url.query());
    bool modified = false;

    const QString host = url.host().toLower();
    const bool isYouTube = host.contains("youtube.com") || host.contains("youtu.be");

    for (const auto &param : kTrackingParams) {
        if (param == "feature" && !isYouTube) {
            continue;
        }
        if (query.hasQueryItem(param)) {
            query.removeQueryItem(param);
            modified = true;
        }
    }

    if (!modified) {
        return rawUrl;
    }

    url.setQuery(query.isEmpty() ? QString() : query.toString(QUrl::FullyEncoded));
    return url.toString();
}

} // namespace CustomFeatures
