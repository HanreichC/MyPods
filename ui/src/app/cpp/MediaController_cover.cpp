// MyPods
// License: GPL-3.0

// Covers for the tray popup, on both platforms: saved for the Image, and for the iPhone (AMS has
// none) found through the iTunes Search API.

#include "MediaController.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrl>
#include <QUrlQuery>

namespace {
const QString kPrefix = QStringLiteral("mypods-cover-");
}

// The cover as a file for the Image; a new file name per track, it would keep showing a cached one otherwise
QString MediaController::saveArt(const QString &key, const QByteArray &art)
{
    if (art.isEmpty())
        return {};
    const QString path = QDir::temp().filePath(kPrefix
        + QString::fromLatin1(QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Md5).toHex()));
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(art) != art.size())
        return {};
    return QUrl::fromLocalFile(path).toString();
}

// Only what saveArt wrote: other players' covers (an MPRIS artUrl) are theirs
void MediaController::dropArt(const QString &url)
{
    const QString path = QUrl(url).toLocalFile();
    if (QFileInfo(path).fileName().startsWith(kPrefix))
        QFile::remove(path);
}

// The iTunes Search API finds it by artist and title, which it sends to Apple
void MediaController::fetchIphoneArt(const QString &key, const QString &title, const QString &artist)
{
    if (!m_network)
        m_network = new QNetworkAccessManager(this);
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("term"), artist + QLatin1Char(' ') + title);
    query.addQueryItem(QStringLiteral("entity"), QStringLiteral("song"));
    query.addQueryItem(QStringLiteral("limit"), QStringLiteral("1"));
    // the store of this country, where Apple Music found the song too
    if (const QString country = QLocale::territoryToCode(QLocale::system().territory()); !country.isEmpty())
        query.addQueryItem(QStringLiteral("country"), country);
    QUrl url(QStringLiteral("https://itunes.apple.com/search"));
    url.setQuery(query);

    QNetworkReply *search = m_network->get(QNetworkRequest(url));
    connect(search, &QNetworkReply::finished, this, [this, search, key] {
        search->deleteLater();
        const QJsonArray results = QJsonDocument::fromJson(search->readAll()).object().value(QStringLiteral("results")).toArray();
        QString artwork = results.isEmpty() ? QString() : results.first().toObject().value(QStringLiteral("artworkUrl100")).toString();
        if (key != m_artKey || artwork.isEmpty())
            return;
        artwork.replace(QStringLiteral("100x100"), QStringLiteral("300x300")); // the same picture, larger
        QNetworkReply *image = m_network->get(QNetworkRequest(QUrl(artwork)));
        connect(image, &QNetworkReply::finished, this, [this, image, key] {
            image->deleteLater();
            if (image->error() == QNetworkReply::NoError)
                showArt(key, image->readAll());
        });
    });
}

// A cover that arrived after the track was shown, if the track is still the same
void MediaController::showArt(const QString &key, const QByteArray &art)
{
    if (key != m_artKey || m_player.isEmpty())
        return;
    const QString artUrl = saveArt(key, art);
    if (artUrl.isEmpty())
        return;
    m_player[QStringLiteral("artUrl")] = artUrl;
    emit playerChanged();
}
