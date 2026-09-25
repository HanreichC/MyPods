// MagicPodsLinux: https://github.com/steam3d/MagicPodsLinux
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app>
// License: GPL-3.0

#include <QApplication>
#include <QQmlApplicationEngine>
#include <QTranslator>
#include <QLocale>
#include <QDir>
#include <QRegularExpression>
#include <QSettings>
#include <QIcon>
#include <QLibraryInfo>
#include <QQmlContext>
#include <QDebug>
#include <QLocalServer>
#include <QLocalSocket>
#include <QAction>
#include <QFontDatabase>
#include <QMenu>
#include <QQmlComponent>
#include <QQuickStyle>
#include <QScopedPointer>
#include <QCursor>
#include <QScreen>
#include <QWindow>

#ifdef HAVE_LAYERSHELLQT
#include <LayerShellQt/window.h>
#endif

#include <QJsonDocument>
#include <QTimer>
#include <QWebSocket>
#include <cstdio>

#include "Actions.h"
#include "BackendManager.h"
#include "DesktopManager.h"
#include "MediaController.h"
#include "TrayIcon.h"
#include "TrayIconManager.h"
#include "Backend.h"

static void ensureEnvDefaults() {
    if (qEnvironmentVariableIsEmpty("QML2_IMPORT_PATH")) {
        const QString qmlPath = QLibraryInfo::path(QLibraryInfo::QmlImportsPath);
        if (!qmlPath.isEmpty()) {
            qputenv("QML2_IMPORT_PATH", qmlPath.toUtf8());
        }
    }

#ifdef Q_OS_LINUX
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        const bool hasWayland = !qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY");
        qputenv("QT_QPA_PLATFORM", hasWayland ? "wayland;xcb" : "xcb");
    }
#endif


    if (qEnvironmentVariableIsEmpty("QML_XHR_ALLOW_FILE_READ")) {
        qputenv("QML_XHR_ALLOW_FILE_READ", "1");
    }

}

// magicpods --action <name>, for keyboard shortcuts (Actions.h): asks the daemon for the active
// headphones, sends the change and exits. Needs no running window or tray.
static int runAction(int argc, char *argv[], const QString &action) {
    QCoreApplication app(argc, argv);
    if (!Actions::names.contains(action)) {
        std::fprintf(stderr, "Unknown action \"%s\". Available: %s\n", qUtf8Printable(action), qUtf8Printable(Actions::names.join(QStringLiteral(", "))));
        return 2;
    }

    QWebSocket socket;
    QObject::connect(&socket, &QWebSocket::connected, &app, [&]() {
        socket.sendTextMessage(QStringLiteral(R"({"method":"GetActiveDeviceInfo"})"));
    });
    bool sent = false;
    QObject::connect(&socket, &QWebSocket::textMessageReceived, &app, [&](const QString &message) {
        const QJsonObject json = QJsonDocument::fromJson(message.toUtf8()).object();
        // the socket also gets the broadcasts, among them the change just sent: acting on it again would undo a toggle
        if (sent || !json.contains(QStringLiteral("info")))
            return;
        sent = true;
        const QJsonObject request = Actions::request(action, json.value(QStringLiteral("info")).toObject());
        if (request.isEmpty()) {
            std::fprintf(stderr, "No connected headphones that can do \"%s\"\n", qUtf8Printable(action));
            app.exit(1);
            return;
        }
        socket.sendTextMessage(QString::fromUtf8(QJsonDocument(request).toJson(QJsonDocument::Compact)));
        socket.flush();
        QTimer::singleShot(200, &app, [&app]() { app.exit(0); }); // let the frame leave before the socket closes
    });
    QObject::connect(&socket, &QWebSocket::errorOccurred, &app, [&app]() {
        std::fprintf(stderr, "The MyPods daemon is not running\n");
        app.exit(1);
    });
    QTimer::singleShot(5000, &app, [&app]() {
        std::fprintf(stderr, "No answer from the MyPods daemon\n");
        app.exit(1);
    });
    socket.open(QUrl(QStringLiteral("ws://127.0.0.1:2020")));
    return app.exec();
}

int main(int argc, char *argv[]) {
    for (int i = 1; i + 1 < argc; ++i)
        if (qstrcmp(argv[i], "--action") == 0)
            return runAction(argc, argv, QString::fromLocal8Bit(argv[i + 1]));

    ensureEnvDefaults();

    QApplication app(argc, argv);

    QFontDatabase::addApplicationFont(":/fonts/Inter-Regular.ttf");
    QFontDatabase::addApplicationFont(":/fonts/Inter-Medium.ttf");
    QFontDatabase::addApplicationFont(":/fonts/Inter-SemiBold.ttf");
    QFontDatabase::addApplicationFont(":/fonts/Inter-Bold.ttf");
    app.setFont(QFont("Inter", app.font().pointSize()));

    QQuickStyle::setStyle("Material");
    app.setApplicationName("MyPods");
    app.setApplicationDisplayName("MyPods");
    app.setApplicationVersion(MAGICPODS_VERSION);
    app.setOrganizationName("MyPods");
    app.setOrganizationDomain("magicpods.app");
    app.setDesktopFileName("app.magicpods");
    app.setQuitOnLastWindowClosed(false);

    const QString serverName = QString("magicpods_%1_%2")
                                   .arg(app.organizationDomain(), app.applicationName());
    QLocalSocket socket;
    socket.connectToServer(serverName);
    if (socket.waitForConnected(150)) {
        socket.write("raise");
        socket.flush();
        socket.waitForBytesWritten(150);
        return 0;
    }

    QLocalServer::removeServer(serverName);
    QLocalServer server;
    if (!server.listen(serverName)) {
        qWarning() << "Failed to listen on local server:" << server.errorString();
    }

    QObject *root = nullptr;
    bool raisePending = false;
    auto raiseWindow = [&]() {
        if (!root) {
            raisePending = true;
            return;
        }
        if (auto window = qobject_cast<QWindow *>(root)) {
            window->show();
            window->raise();
            window->requestActivate();
        } else {
            root->setProperty("visible", true);
        }
    };
    auto handleRaiseRequest = [&]() {
        auto client = server.nextPendingConnection();
        if (client) {
            client->readAll();
            client->disconnectFromServer();
            client->deleteLater();
        }
        raiseWindow();
    };

    QObject::connect(&server, &QLocalServer::newConnection, handleRaiseRequest);
    while (server.hasPendingConnections()) {
        handleRaiseRequest();
    }

    QIcon appIcon;
    appIcon.addFile(QStringLiteral(":/qt/qml/magicpods/src/app/qml/assets/images/logo-16.png"),  QSize(16, 16));
    appIcon.addFile(QStringLiteral(":/qt/qml/magicpods/src/app/qml/assets/images/logo-32.png"),  QSize(32, 32));
    appIcon.addFile(QStringLiteral(":/qt/qml/magicpods/src/app/qml/assets/images/logo-48.png"),  QSize(48, 48));
    appIcon.addFile(QStringLiteral(":/qt/qml/magicpods/src/app/qml/assets/images/logo-256.png"), QSize(256, 256));
    appIcon.addFile(QStringLiteral(":/qt/qml/magicpods/src/app/qml/assets/images/logo-512.png"), QSize(512, 512));
    app.setWindowIcon(appIcon);

    QTranslator englishTranslation;
    const QString englishTranslationPath = QStringLiteral(":/i18n/locale_en.qm");
    if (!englishTranslation.load(englishTranslationPath)) {
        qFatal("Could not load %s!", qUtf8Printable(englishTranslationPath));
    }
    app.installTranslator(&englishTranslation);

    const bool gameScopeMode = !qEnvironmentVariableIsEmpty("GAMESCOPE_WAYLAND_DISPLAY");

    QQmlApplicationEngine engine;

    // Qt.uiLanguage: "" follows the system, otherwise a code like "de" (Settings > Language).
    // Stored locally, not in the daemon, so the first frame is already in the right language.
    QTranslator localizedTranslation;
    auto applyLanguage = [&]() {
        const QLocale locale = engine.uiLanguage().isEmpty() ? QLocale::system() : QLocale(engine.uiLanguage());
        QLocale::setDefault(locale);
        app.removeTranslator(&localizedTranslation);
        if (localizedTranslation.load(locale, QStringLiteral("locale"), QStringLiteral("_"), QStringLiteral(":/i18n/"))) {
            app.installTranslator(&localizedTranslation);
        }
        engine.retranslate();
    };
    engine.setUiLanguage(QSettings().value(QStringLiteral("language")).toString());
    applyLanguage();
    QObject::connect(&engine, &QQmlEngine::uiLanguageChanged, [&]() {
        QSettings().setValue(QStringLiteral("language"), engine.uiLanguage());
        applyLanguage();
    });
    QStringList languages = QDir(QStringLiteral(":/i18n")).entryList({QStringLiteral("locale_*.qm")});
    languages.replaceInStrings(QRegularExpression(QStringLiteral("^locale_|\\.qm$")), QString());
    engine.rootContext()->setContextProperty("availableLanguages", languages);

    BackendManager backendManager;
    DesktopManager desktopManager;
    Backend backend;
    TrayIcon trayIcon;
    MediaController mediaController;
    engine.rootContext()->setContextProperty("backendManager", &backendManager);
    engine.rootContext()->setContextProperty("desktopManager", &desktopManager);
    engine.rootContext()->setContextProperty("cppBackend", &backend);
    engine.rootContext()->setContextProperty("cppTrayIcon", &trayIcon);
    engine.rootContext()->setContextProperty("cppMedia", &mediaController);
    engine.rootContext()->setContextProperty("gameScopeMode", gameScopeMode);
    engine.rootContext()->setContextProperty("trayAvailable", QSystemTrayIcon::isSystemTrayAvailable());
    // Kein Tray-Check: beim Anmelden ist das Tray oft noch nicht bereit; erneutes Starten holt das Fenster ueber den LocalServer hervor
    engine.rootContext()->setContextProperty("startHidden", app.arguments().contains(QStringLiteral("--hidden")));
    backendManager.start();
    backend.connectSocket();
    engine.loadFromModule("magicpods", "Main");

    if (engine.rootObjects().isEmpty()) {
        return -1;
    }

    QQmlComponent popupAnimationComponent(
        &engine,
        QUrl(QStringLiteral("qrc:/qt/qml/magicpods/src/app/qml/PopupAnimation.qml"))
    );
    if (popupAnimationComponent.isError()) {
        qWarning() << popupAnimationComponent.errors();
    }
    QScopedPointer<QObject> popupAnimation(popupAnimationComponent.create(engine.rootContext()));
    if (!popupAnimation) {
        qWarning() << "Failed to create PopupAnimation.qml";
        qWarning() << popupAnimationComponent.errors();
    }

    QQmlComponent trayPopupComponent(&engine, QUrl(QStringLiteral("qrc:/qt/qml/magicpods/src/app/qml/TrayPopup.qml")));
    QScopedPointer<QObject> trayPopupObject(trayPopupComponent.create(engine.rootContext()));
    auto *trayPopup = qobject_cast<QWindow *>(trayPopupObject.get());
    if (!trayPopup) {
        qWarning() << "Failed to create TrayPopup.qml" << trayPopupComponent.errors();
    }
    bool trayPopupAnchored = false;
#ifdef HAVE_LAYERSHELLQT
    // Wayland doesn't let windows place themselves; a layer surface anchored to the top right corner
    // lands right under a top panel's tray.
    // ponytail: assumes the panel is at the top; a bottom panel would need AnchorBottom
    if (trayPopup && QGuiApplication::platformName().startsWith(QLatin1String("wayland"))) {
        auto *layer = LayerShellQt::Window::get(trayPopup);
        layer->setLayer(LayerShellQt::Window::LayerTop);
        layer->setAnchors({LayerShellQt::Window::AnchorTop, LayerShellQt::Window::AnchorRight});
        layer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityOnDemand);
        layer->setActivateOnShow(true);
        layer->setWantsToBeOnActiveScreen(true);
        layer->setScope(QStringLiteral("mypods-tray-popup"));
        trayPopupAnchored = true;
    }
#endif
    auto toggleTrayPopup = [&]() {
        // X11: under (or above) the pointer, inside the work area, so a panel on any edge works
        if (!trayPopupAnchored && !trayPopup->isVisible()) {
            const QPoint cursor = QCursor::pos();
            if (QScreen *screen = QGuiApplication::screenAt(cursor)) {
                const QRect area = screen->availableGeometry();
                const int x = qBound(area.left(), cursor.x() - trayPopup->width() / 2, area.right() + 1 - trayPopup->width());
                const int y = cursor.y() < area.center().y() ? area.top() : area.bottom() + 1 - trayPopup->height();
                trayPopup->setPosition(x, y);
            }
        }
        QMetaObject::invokeMethod(trayPopup, "toggle");
    };

    root = engine.rootObjects().constFirst();
    if (raisePending) {
        raiseWindow();
    }

    QMenu trayMenu;
    auto toggleMainWindow = [&]() {
        if (!root) {
            return;
        }
        if (auto window = qobject_cast<QWindow *>(root)) {
            const bool visible = window->isVisible();
            if (visible) {
                window->hide();
            } else {
                window->show();
                window->raise();
                window->requestActivate();
            }
        } else {
            const bool visible = root->property("visible").toBool();
            root->setProperty("visible", !visible);
        }
    };
    TrayIconManager trayIconManager(&trayIcon, &trayMenu, &backend, toggleMainWindow, [&app]() {
        app.quit();
    });
    // The menu is rebuilt on every open; only the tooltip needs a nudge
    QObject::connect(&engine, &QQmlEngine::uiLanguageChanged, &trayIconManager, &TrayIconManager::updateTrayIcon);

    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        trayIcon.setContextMenu(&trayMenu);
        // Click: the popup for the connected headphones (the window when there are none).
        // Double click: the window.
        QObject::connect(&trayIcon, &TrayIcon::leftClicked, [&]() {
            if (trayPopup && trayPopup->property("hasInfo").toBool()) {
                toggleTrayPopup();
            } else {
                toggleMainWindow();
            }
        });
        QObject::connect(&trayIcon, &TrayIcon::doubleClicked, [&]() {
            if (trayPopup) {
                trayPopup->hide();
            }
            raiseWindow();
        });
        if (trayPopup) {
            QObject::connect(trayPopup, SIGNAL(openAppRequested()), &trayIcon, SIGNAL(doubleClicked()));
        }

        trayIcon.show();
    }

    const QMetaObject::Connection backendRecoveryConnection =
        QObject::connect(&backend, &Backend::connectedChanged, &backendManager, [&backend, &backendManager]() {
        if (!backend.connected())
            backendManager.recover();
    });

    QObject::connect(&app, &QCoreApplication::aboutToQuit, &backendManager, [&backend, &backendManager, backendRecoveryConnection]() {
        QObject::disconnect(backendRecoveryConnection);
        backendManager.beginShutdown();
        backend.disconnectSocket();
        backendManager.stop();
    });

    return app.exec();
}
