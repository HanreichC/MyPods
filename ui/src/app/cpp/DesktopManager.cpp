// MagicPodsLinux: https://github.com/steam3d/MagicPodsLinux
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app>
// License: GPL-3.0

#include "DesktopManager.h"

#include <QDesktopServices>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSettings>
#include <QTimer>
#include <QUrl>
#include <QVersionNumber>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QDebug>

#ifdef Q_OS_WIN
#include <windows.h>
#include <QAbstractNativeEventFilter>
#include <QProcess>

// Ctrl+Alt+Shift with a letter: Ctrl+Alt alone is AltGr, which types characters on many layouts
static const struct { int id; UINT key; const char *action; } kHotkeys[] = {
    {1, 'N', "noise-next"}, {2, 'O', "output-next"}, {3, 'H', "move-here"},
};

// WM_HOTKEY comes to the GUI thread's queue (RegisterHotKey without a window); the action runs like a shortcut
// bound to `mypods --action` would, in a process of its own that talks to the daemon and exits
struct HotkeyFilter : QAbstractNativeEventFilter {
    bool nativeEventFilter(const QByteArray &, void *message, qintptr *) override {
        const MSG *msg = static_cast<const MSG *>(message);
        if (msg->message != WM_HOTKEY)
            return false;
        for (const auto &hotkey : kHotkeys)
            if (static_cast<int>(msg->wParam) == hotkey.id)
                QProcess::startDetached(QCoreApplication::applicationFilePath(), {QStringLiteral("--action"), QString::fromLatin1(hotkey.action)});
        return true;
    }
};
#endif

DesktopManager::DesktopManager(QObject *parent)
    : QObject(parent)
{
#ifdef Q_OS_WIN
    QTimer::singleShot(30 * 1000, this, &DesktopManager::checkForUpdate);
    auto *daily = new QTimer(this);
    connect(daily, &QTimer::timeout, this, &DesktopManager::checkForUpdate);
    daily->start(24 * 60 * 60 * 1000);
    applyHotkeys();
#endif
    // MyPods was called MagicPods: a menu entry made then moves to the new name
    const QString data = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    if (QFile::remove(data + "/applications/app.magicpods.desktop")) {
        QFile::remove(data + "/icons/magicpods.png");
        createDesktopFile();
    }
}

QString DesktopManager::logDir()
{
#ifdef Q_OS_WIN
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/MyPods/logs");
#else
    return QStandardPaths::writableLocation(QStandardPaths::GenericStateLocation) + QStringLiteral("/mypods");
#endif
}

void DesktopManager::openLogDir() const
{
    QDir().mkpath(logDir());
    QDesktopServices::openUrl(QUrl::fromLocalFile(logDir()));
}

bool DesktopManager::checkUpdates() const
{
    return QSettings().value(QStringLiteral("checkUpdates"), true).toBool();
}

void DesktopManager::setCheckUpdates(bool check)
{
    QSettings().setValue(QStringLiteral("checkUpdates"), check);
    if (!check) {
        m_updateVersion.clear();
        m_updateUrl.clear();
    }
    emit updateChanged();
    if (check)
        checkForUpdate();
}

void DesktopManager::checkForUpdate()
{
#ifdef Q_OS_WIN
    if (!checkUpdates())
        return;
    QNetworkRequest request(QUrl(QStringLiteral("https://api.github.com/repos/HanreichC/MyPods/releases/latest")));
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("MyPods")); // GitHub's API wants one
    if (!m_network)
        m_network = new QNetworkAccessManager(this);
    QNetworkReply *reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError)
            return; // offline: tomorrow again
        const QJsonObject release = QJsonDocument::fromJson(reply->readAll()).object();
        const QString tag = release.value(QStringLiteral("tag_name")).toString(); // "v1.0.50"
        const QVersionNumber latest = QVersionNumber::fromString(tag.mid(tag.startsWith(QLatin1Char('v')) ? 1 : 0));
        const QString url = release.value(QStringLiteral("html_url")).toString();
        // only ever a page of this repository's releases is opened, whatever came back
        if (latest.isNull() || latest <= QVersionNumber::fromString(QCoreApplication::applicationVersion()) ||
            !url.startsWith(QStringLiteral("https://github.com/HanreichC/MyPods/releases/")))
            return;
        m_updateVersion = latest.toString();
        m_updateUrl = url;
        emit updateChanged();
    });
#endif
}

bool DesktopManager::hotkeys() const
{
    return QSettings().value(QStringLiteral("hotkeys"), false).toBool();
}

void DesktopManager::setHotkeys(bool on)
{
    QSettings().setValue(QStringLiteral("hotkeys"), on);
    applyHotkeys();
    emit hotkeysChanged();
}

void DesktopManager::applyHotkeys()
{
#ifdef Q_OS_WIN
    static HotkeyFilter filter;
    static const bool installed = (QCoreApplication::instance()->installNativeEventFilter(&filter), true);
    Q_UNUSED(installed);
    for (const auto &hotkey : kHotkeys) {
        UnregisterHotKey(nullptr, hotkey.id);
        if (hotkeys() && !RegisterHotKey(nullptr, hotkey.id, MOD_CONTROL | MOD_ALT | MOD_SHIFT | MOD_NOREPEAT, hotkey.key))
            qWarning("Shortcut Ctrl+Alt+Shift+%c is taken by another program", static_cast<char>(hotkey.key));
    }
#endif
}

static QString iconInstallPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
           + "/icons/mypods.png";
}

static bool installIcon()
{
    const QString iconDir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
                            + "/icons";
    if (!QDir().mkpath(iconDir))
        return false;

    const QString dest = iconInstallPath();
    if (QFile::exists(dest))
        QFile::remove(dest);

    return QFile::copy(":/qt/qml/mypods/src/app/qml/assets/images/logo-512.png", dest);
}

bool DesktopManager::createDesktopFile()
{
    if (!installIcon())
        qWarning() << "[DesktopManager] Failed to install icon";

    // Use the AppImage file (its runtime sets APPIMAGE; the binary itself lives in a temporary mount)
    // or launcher script path if available (CQtDeployer sets CQT_RUN_FILE),
    // otherwise fall back to the binary itself (dev builds)
    QString exePath = qEnvironmentVariable("APPIMAGE", qEnvironmentVariable("CQT_RUN_FILE"));
    if (exePath.isEmpty())
        exePath = QCoreApplication::applicationFilePath();

    const QString dirPath = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
                            + QString("/applications");
    qDebug() << dirPath;

    if (!QDir().mkpath(dirPath))
        return false;

    const QString filePath = dirPath + "/app.mypods.desktop";
    QFile f(filePath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        return false;

    QTextStream out(&f);
    out << QString(
R"([Desktop Entry]
Type=Application
Name=MyPods
Comment=The control center for your Bluetooth headphones
Comment[de]=Die Kontrollzentrale für deine Bluetooth-Kopfhörer
Exec=%1
Icon=%2
Terminal=false
Categories=Utility;
StartupWMClass=MyPods
)")
    .arg(desktopExecArgument(exePath), iconInstallPath());
    return true;
}

QString DesktopManager::desktopFilePath() const
{
    const QString path = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
                         + "/applications/app.mypods.desktop";
    return QFileInfo::exists(path) ? path : QString{};
}

bool DesktopManager::isInstalled() const
{
    return !desktopFilePath().isEmpty();
}

bool DesktopManager::install()
{
    if (isInstalled()) {
        qDebug() << "[DesktopManager] Desktop file already exists";
        return true;
    }

    qDebug() << "[DesktopManager] Creating desktop file";
    return createDesktopFile();
}

bool DesktopManager::uninstall()
{
    QFile::remove(iconInstallPath());
    return QFile::remove(desktopFilePath());
}
