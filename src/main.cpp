#include <qdiriterator.h>
#include <qfontdatabase.h>
#include <qqml.h>
#include <QApplication>
#include <QWheelEvent>
#include <QDateTime>
#include <QFile>
#include <QGuiApplication>
#include <QIcon>
#include <QLocale>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QMouseEvent>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTimer>
#include <QImage>
#include <QSettings>
#include <QStandardPaths>
#include <QString>
#include <QTextStream>
#include <QTranslator>
#include <iostream>
#include <memory>
#include <tools_service.hpp>
#include "app_info_controller.hpp"
#include "book_dto.hpp"
#include "book_operation_status.hpp"
#include "book_service.hpp"
#include "dependency_injection.hpp"
#include "external_book_controller.hpp"
#include "folder_dto.hpp"
#include "free_books_model.hpp"
#include "i_free_books_service.hpp"
#include "i_library_service.hpp"
#include "i_user_service.hpp"
#include "key_sequence_recorder.hpp"
#include "library_proxy_model.hpp"
#include "lute_controller.hpp"
#include "message_handler.hpp"
#include "page_view.hpp"
#include "setting_groups.hpp"
#include "setting_keys.hpp"
#include "shortcuts_proxy_model.hpp"
#include "tag_dto.hpp"
#include "tools_controller.hpp"
#include "user_controller.hpp"
#include "word_definition_dto.hpp"


using namespace adapters::controllers;
using namespace application::services;



// Fork QA helper: find the page item visually (delegates have no QObject parent
// chain to the window, so QObject::findChild does not see them).
static QQuickItem* findLutePageItem(QQuickItem* root)
{
    if(!root)
        return nullptr;

    if(QString::fromLatin1(root->metaObject()->className())
           .contains(QStringLiteral("PageView"))
       && root->property("pageNumber").toInt() == 0 && root->width() > 0
       && root->height() > 0)
        return root;

    for(QQuickItem* child : root->childItems())
    {
        if(QQuickItem* found = findLutePageItem(child))
            return found;
    }
    return nullptr;
}

void registerTypes();
void setupGlobalSettings();
void setupFonts();

// Fork QA helper: `librum.exe --lute-selftest <file.pdf>` extracts the words of
// the document and prints them, then exits. Used to verify the Lute slice
// without a GUI session.
static int runLuteSelfTest(const QString& filePath)
{
    lute::LuteController controller;
    controller.initialize();
    if(!controller.databaseOpen())
    {
        qWarning() << "lute selftest: database could not be opened";
        return 2;
    }
    if(!controller.setDocumentPath(filePath))
    {
        qWarning() << "lute selftest: document could not be opened";
        return 3;
    }
    qInfo() << "lute selftest: database" << controller.databasePath();
    qInfo() << "lute selftest: language" << controller.languageName();
    qInfo() << "lute selftest: pages" << controller.pageCount();
    qInfo() << "lute selftest: title" << controller.documentTitle();
    controller.addDocumentToLuteLibrary();
    qInfo() << "lute selftest: registered" << controller.documentRegistered();
    for(int page = 0; page < controller.pageCount(); ++page)
    {
        const auto words = controller.pageWords(page);
        qInfo() << "lute selftest: page" << page << "words" << words.size();
        for(int i = 0; i < words.size() && i < 5; ++i)
        {
            const auto word = words.at(i).toMap();
            qInfo() << "   " << word.value("word").toString() << "|"
                    << word.value("sentence").toString().left(70);
        }
    }
    return 0;
}

int main(int argc, char* argv[])
{
    // clang-format off
    // App
    QApplication app(argc, argv);
    QApplication::setOrganizationName("Librum-Reader");
    QApplication::setOrganizationDomain("librumreader.com");
    QApplication::setApplicationName("Librum");
    QQuickStyle::setStyle(QStringLiteral("Default"));

    QIcon icon(":/src/logo.ico");
    app.setWindowIcon(icon);

    qInstallMessageHandler(logging::messageHandler);
    setupGlobalSettings();

    // Fork: allow pointing the Lute layer at a specific lute.db.
    const int luteDbIndex = app.arguments().indexOf(QStringLiteral("--lute-db"));
    if(luteDbIndex >= 0 && luteDbIndex + 1 < app.arguments().size())
    {
        QSettings().setValue(QStringLiteral("lute/databasePath"),
                             app.arguments().at(luteDbIndex + 1));
    }

    if(app.arguments().contains("--lute-import"))
    {
        const int index = app.arguments().indexOf("--lute-import");
        if(index + 1 < app.arguments().size())
        {
            lute::LuteController controller;
            controller.initialize();
            const QString path = app.arguments().at(index + 1);
            if(!controller.setDocumentPath(path))
                return 4;
            const bool imported = controller.addPdfToLuteLibrary(path);
            qInfo() << "lute import:" << path << imported
                    << "pages" << controller.pageCount();
            return imported ? 0 : 5;
        }
    }
    if(app.arguments().contains("--lute-selftest"))
    {
        const int index = app.arguments().indexOf("--lute-selftest");
        if(index + 1 < app.arguments().size())
            return runLuteSelfTest(app.arguments().at(index + 1));
    }
    setupFonts();

    // Register types
    qmlRegisterSingletonType(QUrl("qrc:/StyleSheet.qml"), "Librum.style", 1, 0, "Style");
    qmlRegisterSingletonType(QUrl("qrc:/IconSheet.qml"), "Librum.icons", 1, 0, "Icons");
    qmlRegisterSingletonType(QUrl("qrc:/FontSheet.qml"), "Librum.fonts", 1, 0, "Fonts");
    qmlRegisterSingletonType(QUrl("qrc:/Globals.qml"), "Librum.globals", 1, 0, "Globals");
    qmlRegisterSingletonType(QUrl("qrc:/GlobalSettings.qml"), "Librum.globalSettings", 1, 0, "GlobalSettings");
    qmlRegisterSingletonType(QUrl("qrc:/TranslationsModel.qml"), "Librum.models", 1, 0, "TranslationsModel");
    qmlRegisterSingletonType(QUrl("qrc:/modules/CustomComponents/MLanguageModel.qml"), "Librum.models", 1, 0, "MLanguageModel");
    qmlRegisterType<adapters::data_models::LibraryProxyModel>("Librum.models", 1, 0, "LibraryProxyModel");
    qmlRegisterType<adapters::data_models::BookTitleProxyModel>("Librum.models", 1, 0, "BookTitleModel");
    qmlRegisterType<adapters::data_models::FreeBooksModel>("Librum.models", 1, 0, "FreeBooksModel");
    qmlRegisterType<adapters::data_models::ShortcutsProxyModel>("Librum.models", 1, 0, "ShortcutsProxyModel");
    qmlRegisterType<cpp_elements::KeySequenceRecorder>("Librum.elements", 1, 0, "KeySequenceRecorder");
    qmlRegisterType<cpp_elements::PageView>("Librum.elements", 1, 0, "PageView");
    qRegisterMetaType<adapters::dtos::BookDto>();
    qRegisterMetaType<adapters::dtos::TagDto>();
    qRegisterMetaType<adapters::dtos::FolderDto>();
    qRegisterMetaType<adapters::dtos::DictionaryEntryDto>();
    qRegisterMetaType<adapters::dtos::WordTypeDto>();
    qRegisterMetaType<adapters::dtos::WordDefinitionDto>();


    // Authentication Stack
    auto* authenticationService =
        config::diConfig().create<application::IAuthenticationService*>();
    auto authenticationController =
        std::make_unique<AuthenticationController>(authenticationService);
    qmlRegisterSingletonInstance("Librum.controllers", 1, 0, "AuthController",
                                 authenticationController.get());

    // App Info Stack
    auto* appInfoService =
        config::diConfig().create<application::IAppInfoService*>();
    auto appInfoController =
        std::make_unique<AppInfoController>(appInfoService);
    qmlRegisterSingletonInstance("Librum.controllers", 1, 0, "AppInfoController",
                                 appInfoController.get());

    // Ai explanation
    auto* aiExplanationService =
        config::diConfig().create<application::IAiExplanationService*>();
    auto aiExplanationController =
        std::make_unique<AiExplanationController>(aiExplanationService);
    qmlRegisterSingletonInstance("Librum.controllers", 1, 0, "AiExplanationController",
                                 aiExplanationController.get());

    // User Stack
    auto* userService = config::diConfig().create<application::IUserService*>();
    auto userController = std::make_unique<UserController>(userService);
    qmlRegisterSingletonInstance("Librum.controllers", 1, 0, "UserController",
                                 userController.get());

    // Dictionary Stack
    auto* dictionaryService = config::diConfig().create<application::IDictionaryService*>();
    auto dictionaryController = std::make_unique<DictionaryController>(dictionaryService);
    qmlRegisterSingletonInstance("Librum.controllers", 1, 0, "DictionaryController",
                                 dictionaryController.get());

    // Folder Stack
    auto* folderService = config::diConfig().create<application::IFolderService*>();
    auto folderController = std::make_unique<FolderController>(folderService);
    qmlRegisterSingletonInstance("Librum.controllers", 1, 0, "FolderController",
                                 folderController.get());

    // Library Stack
    auto* libraryService = config::diConfig().create<application::ILibraryService*>();
    auto libraryController = std::make_unique<LibraryController>(libraryService);
    qmlRegisterSingletonInstance("Librum.controllers", 1, 0, "LibraryController",
                                 libraryController.get());

    // Book Stack
    auto bookService = std::make_unique<application::services::BookService>();
    auto bookController = std::make_unique<BookController>(bookService.get(), libraryService);
    qmlRegisterSingletonInstance("Librum.controllers", 1, 0, "BookController",
                                 bookController.get());

    // External Book Stack
    auto externalBookService = std::make_unique<application::services::BookService>();
    auto externalBookController = std::make_unique<ExternalBookController>(externalBookService.get());
    qmlRegisterSingletonInstance("Librum.controllers", 1, 0, "ExternalBookController",
                                 externalBookController.get());

    // Lute language-learning stack (fork addition)
    auto luteController = std::make_unique<lute::LuteController>();
    luteController->initialize();
    qmlRegisterSingletonInstance("Librum.lute", 1, 0, "LuteController",
                                 luteController.get());

    // External books (EPUB/MOBI/PDF) share their MuPDF document with Lute so
    // page numbering and word coordinates match the renderer exactly.
    QObject::connect(
        externalBookController.get(), &ExternalBookController::documentReady,
        luteController.get(),
        [externalBookController = externalBookController.get(),
         luteController = luteController.get()](const QString& filePath) {
            luteController->setDocumentFromFzDocument(
                externalBookController->getFzDocument(), filePath);
        });
    QObject::connect(
        bookController.get(), &BookController::bookOpened,
        luteController.get(),
        [luteController = luteController.get(),
         bookController = bookController.get()](const QString& filePath) {
            if(!filePath.isEmpty())
                luteController->setDocumentFromFzDocument(
                    bookController->getFzDocument(), filePath);
        });


    // Free books stack
    auto* freeBooksService = config::diConfig().create<application::IFreeBooksService*>();
    auto freeBooksController = std::make_unique<FreeBooksController>(freeBooksService);
    qmlRegisterSingletonInstance("Librum.controllers", 1, 0, "FreeBooksController",
                                 freeBooksController.get());

    // Settings Stack
    auto* settingsService = config::diConfig().create<application::ISettingsService*>();
    auto settingsController = std::make_unique<SettingsController>(settingsService);
    qmlRegisterSingletonInstance("Librum.controllers", 1, 0, "SettingsController",
                                 settingsController.get());

    // Fork: the reading UI needs the appearance/behavior/shortcut values even
    // when no account is used (offline reading of a local PDF). Loading the
    // local profile here seeds them with the shipped defaults.
    settingsService->loadUserSettings(QString(), QString());

    // Tools Stack
    auto toolsService = std::make_unique<application::services::ToolsService>(libraryService);
    auto toolsController = std::make_unique<ToolsController>(toolsService.get());
    qmlRegisterSingletonInstance("Librum.controllers", 1, 0, "ToolsController",
                                 toolsController.get());

    // Enums
    qmlRegisterUncreatableMetaObject(application::book_operation_status::staticMetaObject, "Librum.controllers",
                                     1, 0, "BookOperationStatus",
                                     "This is an uncreatable enum!");
    
    qmlRegisterUncreatableMetaObject(application::setting_keys::staticMetaObject, "Librum.controllers",
                                     1, 0, "SettingKeys",
                                     "This is an uncreatable enum!");

    qmlRegisterUncreatableMetaObject(application::setting_groups::staticMetaObject, "Librum.controllers",
                                     1, 0, "SettingGroups",
                                     "This is an uncreatable enum!");

    qmlRegisterUncreatableMetaObject(application::error_codes::staticMetaObject, "Librum.controllers",
                                     1, 0, "ErrorCode",
                                     "This is an uncreatable enum!");

    // Connect the user service's loggedOut with the authentication service's loggedOut
    // signal, since both services need to be able to log out the user.
    QObject::connect(userService, &application::IUserService::logoutUser,
                     authenticationService, &application::IAuthenticationService::logoutUser);


    // Setup login connections
    QObject::connect(authenticationService, &application::IAuthenticationService::loggedIn,
                     libraryService, &application::ILibraryService::setupUserData);
    QObject::connect(authenticationService, &application::IAuthenticationService::loggedOut,
                     libraryService, &application::ILibraryService::clearUserData);

    QObject::connect(authenticationService, &application::IAuthenticationService::loggedIn,
                     folderService, &application::IFolderService::setupUserData);
    QObject::connect(authenticationService, &application::IAuthenticationService::loggedOut,
                     folderService, &application::IFolderService::clearUserData);

    QObject::connect(authenticationService, &application::IAuthenticationService::loggedIn,
                     freeBooksService, &application::IFreeBooksService::setupUserData);
    QObject::connect(authenticationService, &application::IAuthenticationService::loggedIn,
                     aiExplanationService, &application::IAiExplanationService::setupUserData);

    QObject::connect(authenticationService, &application::IAuthenticationService::loggedOut,
                     aiExplanationService, &application::IAiExplanationService::clearUserData);
    QObject::connect(authenticationService, &application::IAuthenticationService::loggedOut,
        freeBooksService, &application::IFreeBooksService::clearUserData);

    QObject::connect(authenticationService, &application::IAuthenticationService::loggedIn,
                     userService, &application::IUserService::setupUserData);
    QObject::connect(authenticationService, &application::IAuthenticationService::loggedOut,
                     userService, &application::IUserService::clearUserData);

    QObject::connect(authenticationService, &application::IAuthenticationService::loggedIn,
                     settingsService, &application::ISettingsService::loadUserSettings);
    QObject::connect(authenticationService, &application::IAuthenticationService::loggedOut,
                     settingsService, &application::ISettingsService::clearUserData);


    // Setup other connections
    QObject::connect(userService, &application::IUserService::bookStorageDataUpdated,
                     libraryService, &application::ILibraryService::updateUsedBookStorage);

    QObject::connect(libraryController.get(),
                     &adapters::ILibraryController::downloadedProjectGutenbergIdsReady,
                     freeBooksController.get(),
                     &adapters::IFreeBooksController::proccessDownloadedProjectGutenbergIds);

    // Startup
    QQmlApplicationEngine engine;
    QQuickStyle::setStyle("Basic");
    engine.addImportPath("qrc:/modules");
    engine.addImportPath(QCoreApplication::applicationDirPath() + "/src/presentation/qt_tree_view/qml/");
    appInfoController->setQmlApplicationEngine(&engine);


    // Setup translations
    QSettings settings;
    auto storedLanguage = settings.value("language", QVariant("")).toString();
    if(storedLanguage.isEmpty())
    {
        // If no language was specified in the settings, deduce the system language
        const QStringList uiLanguages = QLocale::system().uiLanguages();
        for(const QString& locale : uiLanguages)
        {
            const QString name = QLocale(locale).name();
            if(appInfoController->switchToLanguage(name))
            {
                break;
            }
        }
    }
    else
    {
        appInfoController->switchToLanguage(storedLanguage);
    }

    // Fork: the first positional argument is the book to open. Flags such as
    // --lute-demo and --out do not count as book paths.
    QStringList positionalArguments;
    QString demoOutputPath;
    QString demoLibraryPath;
    for(int i = 1; i < app.arguments().size(); ++i)
    {
        const QString argument = app.arguments().at(i);
        if(argument == "--out")
        {
            if(i + 1 < app.arguments().size())
                demoOutputPath = app.arguments().at(++i);
            continue;
        }
        if(argument == "--lute-demo-library")
        {
            if(i + 1 < app.arguments().size())
                demoLibraryPath = app.arguments().at(++i);
            continue;
        }
        if(argument == "--lute-db")
        {
            if(i + 1 < app.arguments().size())
                ++i;
            continue;
        }
        if(argument.startsWith("--"))
            continue;
        positionalArguments.append(argument);
    }
    if(demoOutputPath.isEmpty())
        demoOutputPath = QStringLiteral("lute-demo.png");

    if(!positionalArguments.isEmpty())
    {
        QString filePath = positionalArguments.at(0);
        QFileInfo fileInfo(filePath);
        if(!fileInfo.isFile())
        {
            std::cerr << "Book at: " << filePath.toStdString() << " does not exist.\n";
            return -1;
        }

        auto setupSuccess = externalBookController->setUp(filePath);
        if(!setupSuccess)
        {
            std::cerr << "The file type of: " << filePath.toStdString() << " is not supported.\n";
            return -1;
        }
        // Let Lute share the renderer's MuPDF document for word lookups and
        // highlights (same pagination, same coordinates).
        luteController->setDocumentFromFzDocument(
            externalBookController->getFzDocument(), filePath);
        engine.rootContext()->setContextProperty("externalBook", true);
    }
    else {
        engine.rootContext()->setContextProperty("externalBook", false);
    }

    const QUrl url("qrc:/main.qml");
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreated, &app,
        [url](QObject* obj, const QUrl& objUrl)
        {
            if(obj == nullptr && url == objUrl)
                QCoreApplication::exit(-1);
        },
        Qt::QueuedConnection);
    engine.load(url);

    // Fork QA helper: `librum.exe --lute-demo <file.pdf> [out.png]` opens the
    // reader, seeds a few Lute statuses, opens the word popup and saves a
    // screenshot of the real window, then quits. Used for screenshots/QA.
    // Fork QA helper: `--lute-demo-external-page <n>` navigates the external
    // reader to page n before screenshotting it.
    if(app.arguments().contains("--lute-demo-external-page"))
    {
        const int index = app.arguments().indexOf("--lute-demo-external-page");
        int page = 0;
        if(index + 1 < app.arguments().size())
            page = app.arguments().at(index + 1).toInt();
        const QString outputPath = demoOutputPath;
        QTimer::singleShot(3000, &app, [&engine, page]() {
            auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().value(0));
            if(!window)
                return;
            window->requestActivate();
            for(int i = 0; i < page; ++i)
            {
                QTimer::singleShot(i * 60, window, [window]() {
                    const QPointF pos(window->width() / 2.0,
                                      window->height() / 2.0);
                    QWheelEvent event(pos, window->mapToGlobal(pos), QPoint(),
                                      QPoint(0, -120), Qt::NoButton,
                                      Qt::NoModifier, Qt::NoScrollPhase, false);
                    QCoreApplication::sendEvent(window, &event);
                });
            }
        });
        QTimer::singleShot(25000, &engine, [&engine, outputPath]() {
            if(auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().value(0)))
                window->grabWindow().save(outputPath);
            QCoreApplication::quit();
        });
    }

    // Fork QA helper: `--lute-demo-external` screenshots the external reader
    // (EPUB/MOBI opened via the positional file argument).
    if(app.arguments().contains("--lute-demo-external"))
    {
        const QString outputPath = demoOutputPath;
        QTimer::singleShot(5000, &engine, [&engine, outputPath]() {
            if(auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().value(0)))
                window->grabWindow().save(outputPath);
            QCoreApplication::quit();
        });
    }

    const int luteDemoIndex = app.arguments().indexOf("--lute-demo");
    if(luteDemoIndex >= 0 && luteDemoIndex + 1 < app.arguments().size())
    {
        const QString outputPath = demoOutputPath;

        QTimer::singleShot(3000, &app, [&engine, externalBookController = externalBookController.get(),
                                        luteController = luteController.get(), outputPath]() {
            qInfo() << "lute demo: step 1 setCurrentPage";
            externalBookController->setCurrentPage(0);

            QTimer::singleShot(700, &engine, [&engine, luteController, outputPath]() {
                const QStringList words = {QStringLiteral("harbor"),
                                           QStringLiteral("lighthouse"),
                                           QStringLiteral("murmured"),
                                           QStringLiteral("resilience"),
                                           QStringLiteral("patience"),
                                           QStringLiteral("boat")};
                const QList<int> statuses = {3, 5, 4, 1, 99, 2};

                qInfo() << "lute demo: step 2 pageWords";
                const QVariantList pageWords = luteController->pageWords(0);
                qInfo() << "lute demo: got" << pageWords.size() << "words";
                QVariantMap harbor;
                for(const QVariant& entry : pageWords)
                {
                    const QVariantMap word = entry.toMap();
                    const QString text = word.value(QStringLiteral("word")).toString();
                    const int index = word.value(QStringLiteral("index")).toInt();
                    for(int i = 0; i < words.size(); ++i)
                    {
                        if(text.compare(words.at(i), Qt::CaseInsensitive) == 0)
                        {
                            qInfo() << "lute demo: seeding" << text << index;
                            luteController->setStatusForWord(0, index, statuses.at(i));
                        }
                    }
                    if(text.compare(QStringLiteral("harbor"), Qt::CaseInsensitive) == 0
                       && harbor.isEmpty())
                        harbor = word;
                }

                if(!harbor.isEmpty())
                {
                    const QVariantMap rect = harbor.value(QStringLiteral("rect")).toMap();
                    qInfo() << "lute demo: selectAtPagePoint";
                    luteController->selectAtPagePoint(
                        0, rect.value(QStringLiteral("x")).toDouble()
                               + rect.value(QStringLiteral("w")).toDouble() / 2,
                        rect.value(QStringLiteral("y")).toDouble()
                            + rect.value(QStringLiteral("h")).toDouble() / 2);
                }

                QTimer::singleShot(1600, &engine, [&engine, outputPath]() {
                    qInfo() << "lute demo: grabbing window";
                    if(auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().value(0)))
                    {
                        qInfo() << "lute demo: window" << window;
                        window->grabWindow().save(outputPath);
                    }
                    QCoreApplication::quit();
                });
            });
        });
    }

    // Fork QA helper: `--lute-demo-settings` opens Settings -> Lute.
    if(app.arguments().contains("--lute-demo-settings"))
    {
        const QString outputPath = demoOutputPath;
        QTimer::singleShot(2500, &app, [luteController = luteController.get()]() {
            luteController->openSettings();
        });
        QTimer::singleShot(5200, &engine, [&engine, outputPath]() {
            if(auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().value(0)))
                window->grabWindow().save(outputPath);
            QCoreApplication::quit();
        });
    }

    // Fork QA helper: `--lute-demo-login` captures the start page.
    if(app.arguments().contains("--lute-demo-login"))
    {
        const QString outputPath = demoOutputPath;
        QTimer::singleShot(3500, &engine, [&engine, outputPath]() {
            if(auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().value(0)))
                window->grabWindow().save(outputPath);
            QCoreApplication::quit();
        });
    }

    // Fork QA helper: `--lute-demo-click` clicks a word through the real
    // QML event path (PageView -> selection -> Lute popup) and screenshots it.
    if(app.arguments().contains("--lute-demo-click"))
    {
        const QString outputPath = demoOutputPath;

        QTimer::singleShot(3000, &app, [&engine, externalBookController = externalBookController.get()]() {
            externalBookController->setCurrentPage(0);
        });

        auto* pollTimer = new QTimer(&engine);
        pollTimer->setInterval(400);
        QObject::connect(pollTimer, &QTimer::timeout, &engine,
                         [&engine, pollTimer, luteController = luteController.get()]() {
            auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().value(0));
            if(!window)
                return;

            QQuickItem* pageItem = findLutePageItem(window->contentItem());
            if(!pageItem)
                return;

            const QVariantList words = luteController->pageWords(0);
            QVariantMap target;
            for(const QVariant& entry : words)
            {
                const QVariantMap word = entry.toMap();
                if(word.value(QStringLiteral("word")).toString().compare(
                       QStringLiteral("harbor"), Qt::CaseInsensitive) == 0)
                {
                    target = word;
                    break;
                }
            }
            if(target.isEmpty())
                return;

            pollTimer->stop();

            const QVariantMap rect = target.value(QStringLiteral("rect")).toMap();
            const QPointF local((rect.value(QStringLiteral("x")).toDouble()
                                 + rect.value(QStringLiteral("w")).toDouble() / 2)
                                    * pageItem->width(),
                                (rect.value(QStringLiteral("y")).toDouble()
                                 + rect.value(QStringLiteral("h")).toDouble() / 2)
                                    * pageItem->height());
            const QPointF scenePos = pageItem->mapToScene(local);

            qInfo() << "lute click: page" << pageItem->size() << "local" << local
                    << "scene" << scenePos;

            window->requestActivate();
            QMouseEvent press(QEvent::MouseButtonPress, scenePos,
                              window->mapToGlobal(scenePos.toPoint()),
                              Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            QCoreApplication::sendEvent(window, &press);
            QMouseEvent release(QEvent::MouseButtonRelease, scenePos,
                                window->mapToGlobal(scenePos.toPoint()),
                                Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
            QCoreApplication::sendEvent(window, &release);

            qInfo() << "lute click: popup visible" << luteController->wordPopupVisible();

            QTimer::singleShot(1200, &engine, [&engine]() {
                if(auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().value(0)))
                    window->grabWindow().save(QStringLiteral("lute-click-debug.png"));
            });
        });
        pollTimer->start();

        QTimer::singleShot(6000, &engine, [&engine, outputPath, pollTimer]() {
            pollTimer->stop();
            if(auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().value(0)))
                window->grabWindow().save(outputPath);
            QCoreApplication::quit();
        });
    }

    // Fork QA helper: `--lute-demo-phrase` drags over three words, checks that
    // the phrase popup opens and stores the phrase with a status.
    if(app.arguments().contains("--lute-demo-phrase"))
    {
        const QString outputPath = demoOutputPath;

        QTimer::singleShot(3000, &app, [&engine, externalBookController = externalBookController.get()]() {
            externalBookController->setCurrentPage(0);
        });

        auto* pollTimer = new QTimer(&engine);
        pollTimer->setInterval(400);
        QObject::connect(pollTimer, &QTimer::timeout, &engine,
                         [&engine, pollTimer, luteController = luteController.get()]() {
            auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().value(0));
            if(!window)
                return;
            QQuickItem* pageItem = findLutePageItem(window->contentItem());
            if(!pageItem)
                return;

            const QVariantList words = luteController->pageWords(0);
            int startIndex = -1;
            int endIndex = -1;
            for(int i = 0; i < words.size(); ++i)
            {
                const QString text = words.at(i).toMap().value("word").toString();
                if(startIndex < 0 && text.compare("run", Qt::CaseInsensitive) == 0)
                    startIndex = i;
                else if(startIndex >= 0 && text.compare("of", Qt::CaseInsensitive) == 0)
                {
                    endIndex = i;
                    break;
                }
            }
            if(startIndex < 0 || endIndex < 0)
                return;
            pollTimer->stop();

            auto pointFor = [pageItem](const QVariantMap& word) {
                const QVariantMap rect = word.value("rect").toMap();
                const QPointF local((rect.value("x").toDouble()
                                     + rect.value("w").toDouble() / 2)
                                        * pageItem->width(),
                                    (rect.value("y").toDouble()
                                     + rect.value("h").toDouble() / 2)
                                        * pageItem->height());
                return pageItem->mapToScene(local);
            };

            const QPointF start = pointFor(words.at(startIndex).toMap());
            const QPointF end = pointFor(words.at(endIndex).toMap());

            window->requestActivate();
            QMouseEvent press(QEvent::MouseButtonPress, start,
                              window->mapToGlobal(start.toPoint()),
                              Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            QCoreApplication::sendEvent(window, &press);
            QMouseEvent move(QEvent::MouseMove, end,
                             window->mapToGlobal(end.toPoint()), Qt::NoButton,
                             Qt::LeftButton, Qt::NoModifier);
            QCoreApplication::sendEvent(window, &move);
            QMouseEvent release(QEvent::MouseButtonRelease, end,
                                window->mapToGlobal(end.toPoint()),
                                Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
            QCoreApplication::sendEvent(window, &release);

            qInfo() << "lute phrase: popup" << luteController->wordPopupVisible()
                    << "text"
                    << luteController->activeWord().value("word").toString()
                    << "isPhrase" << luteController->activeWord().value("isPhrase");
            if(luteController->wordPopupVisible())
                luteController->setActiveStatus(3);

            QTimer::singleShot(1400, &engine, [&engine]() {
                if(auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().value(0)))
                    window->grabWindow().save(QStringLiteral("lute-phrase-debug.png"));
            });
        });
        pollTimer->start();

        QTimer::singleShot(6000, &engine, [&engine, outputPath, pollTimer]() {
            pollTimer->stop();
            if(auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().value(0)))
                window->grabWindow().save(outputPath);
            QCoreApplication::quit();
        });
    }

    // Fork QA helper: `--lute-demo-ai` points the AI client at a mock Ollama
    // server, opens a word and stores/verifies the returned suggestion.
    if(app.arguments().contains("--lute-demo-ai"))
    {
        const QString outputPath = demoOutputPath;
        QSettings settings;
        const QVariant previousProvider = settings.value(QStringLiteral("lute/aiProvider"));
        const QVariant previousModel = settings.value(QStringLiteral("lute/aiModel"));
        const QVariant previousBaseUrl = settings.value(QStringLiteral("lute/aiBaseUrl"));
        settings.setValue(QStringLiteral("lute/aiProvider"), QStringLiteral("ollama"));
        settings.setValue(QStringLiteral("lute/aiModel"), QStringLiteral("mock-model"));
        settings.setValue(QStringLiteral("lute/aiBaseUrl"),
                          QStringLiteral("http://127.0.0.1:11888"));
        settings.setValue(QStringLiteral("lute/aiTargetLanguage"), QStringLiteral("German"));

        QTimer::singleShot(3000, &app, [&engine, externalBookController = externalBookController.get()]() {
            externalBookController->setCurrentPage(0);
        });

        auto* pollTimer = new QTimer(&engine);
        pollTimer->setInterval(400);
        QObject::connect(pollTimer, &QTimer::timeout, &engine,
                         [&engine, pollTimer, luteController = luteController.get()]() {
            auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().value(0));
            if(!window)
                return;
            QQuickItem* pageItem = findLutePageItem(window->contentItem());
            if(!pageItem)
                return;
            pollTimer->stop();

            const QVariantList words = luteController->pageWords(0);
            for(const QVariant& entry : words)
            {
                const QVariantMap word = entry.toMap();
                if(word.value("word").toString().compare("harbor", Qt::CaseInsensitive) != 0)
                    continue;
                const QVariantMap rect = word.value("rect").toMap();
                luteController->selectAtPagePoint(
                    0, rect.value("x").toDouble() + rect.value("w").toDouble() / 2,
                    rect.value("y").toDouble() + rect.value("h").toDouble() / 2);
                break;
            }
            luteController->requestAiSuggestion();
            qInfo() << "lute ai: requested, busy" << luteController->aiBusy();
        });
        pollTimer->start();

        QTimer::singleShot(6200, &engine, [&engine, luteController = luteController.get(), outputPath,
                                          previousProvider, previousModel, previousBaseUrl]() {
            qInfo() << "lute ai: suggestion" << luteController->aiSuggestion()
                    << "error" << luteController->aiError();
            if(!luteController->aiSuggestion().isEmpty())
                luteController->applyAiSuggestion();
            qInfo() << "lute ai: stored translation"
                    << luteController->activeWord().value("translation").toString();

            if(auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().value(0)))
                window->grabWindow().save(outputPath);

            QSettings settings;
            if(previousProvider.isValid())
                settings.setValue(QStringLiteral("lute/aiProvider"), previousProvider);
            else
                settings.remove(QStringLiteral("lute/aiProvider"));
            if(previousModel.isValid())
                settings.setValue(QStringLiteral("lute/aiModel"), previousModel);
            else
                settings.remove(QStringLiteral("lute/aiModel"));
            if(previousBaseUrl.isValid())
                settings.setValue(QStringLiteral("lute/aiBaseUrl"), previousBaseUrl);
            else
                settings.remove(QStringLiteral("lute/aiBaseUrl"));

            QCoreApplication::quit();
        });
    }

    // Fork QA helper: `--lute-demo-restore` backs up the open lute.db and
    // restores it again, logging both results.
    if(app.arguments().contains("--lute-demo-restore"))
    {
        const QString backupPath = demoOutputPath + QStringLiteral(".backup.db");
        QTimer::singleShot(2000, &app, [luteController = luteController.get(), backupPath]() {
            const bool backedUp = luteController->backupDatabase(backupPath);
            qInfo() << "lute restore demo: backup" << backedUp
                    << luteController->lastMessage();
            const bool restored = luteController->restoreDatabase(backupPath);
            qInfo() << "lute restore demo: restore" << restored
                    << luteController->lastMessage()
                    << "databaseOpen" << luteController->databaseOpen()
                    << "languages" << luteController->languages().size()
                    << "books" << luteController->books().size();
            QCoreApplication::quit();
        });
    }

    // Fork QA helper: `--lute-demo-status` opens a word and sets status 3.
    if(app.arguments().contains("--lute-demo-status"))
    {
        const QString outputPath = demoOutputPath;
        QTimer::singleShot(2500, &app, [luteController = luteController.get()]() {
            luteController->openLibrary();
        });
        QTimer::singleShot(3300, &app, [luteController = luteController.get()]() {
            const QVariantList books = luteController->books();
            if(books.isEmpty())
                return;
            if(luteController->openBook(books.first().toMap().value("id").toLongLong()))
                luteController->showReader();
        });
        QTimer::singleShot(4600, &app, [luteController = luteController.get()]() {
            luteController->selectTextWord(12);
        });
        QTimer::singleShot(5200, &app, [luteController = luteController.get()]() {
            luteController->setActiveStatus(3);
            qInfo() << "lute status demo:"
                    << luteController->activeWord().value("status")
                    << luteController->activeWord().value("statusLabel");
        });
        QTimer::singleShot(6400, &engine, [&engine, outputPath]() {
            if(auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().value(0)))
                window->grabWindow().save(outputPath);
            QCoreApplication::quit();
        });
    }

    // Fork QA helper: `--lute-demo-family` links and unlinks a parent term in
    // the open word popup and logs both results.
    if(app.arguments().contains("--lute-demo-family"))
    {
        const QString outputPath = demoOutputPath;
        QTimer::singleShot(2500, &app, [luteController = luteController.get()]() {
            luteController->openLibrary();
        });
        QTimer::singleShot(3300, &app, [luteController = luteController.get()]() {
            const QVariantList books = luteController->books();
            if(books.isEmpty())
                return;
            if(luteController->openBook(books.first().toMap().value("id").toLongLong()))
                luteController->showReader();
        });
        QTimer::singleShot(4600, &app, [luteController = luteController.get()]() {
            luteController->selectTextWord(12);
        });
        QTimer::singleShot(5200, &app, [luteController = luteController.get()]() {
            const bool linked = luteController->linkActiveParent(QStringLiteral("dom"));
            qInfo() << "lute family demo: link" << linked
                    << luteController->activeWord().value("parents");
        });
        QTimer::singleShot(5800, &app, [luteController = luteController.get()]() {
            const QVariantList parents = luteController->activeWord().value("parents").toList();
            if(parents.isEmpty())
            {
                qInfo() << "lute family demo: no parents";
                return;
            }
            const qint64 parentId = parents.first().toMap().value("id").toLongLong();
            qInfo() << "lute family demo: unlink" << parentId
                    << luteController->unlinkActiveParent(parentId)
                    << luteController->activeWord().value("parents");
        });
        QTimer::singleShot(6800, &engine, [&engine, outputPath]() {
            if(auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().value(0)))
                window->grabWindow().save(outputPath);
            QCoreApplication::quit();
        });
    }

    // Fork QA helper: `--lute-demo-tag-toggle` opens a word popup and toggles
    // its first tag through the controller, logging the result.
    if(app.arguments().contains("--lute-demo-tag-toggle"))
    {
        const QString outputPath = demoOutputPath;
        QTimer::singleShot(2500, &app, [luteController = luteController.get()]() {
            luteController->openLibrary();
            luteController->refreshTags();
        });
        QTimer::singleShot(3300, &app, [luteController = luteController.get()]() {
            const QVariantList books = luteController->books();
            if(books.isEmpty())
                return;
            if(luteController->openBook(books.first().toMap().value("id").toLongLong()))
                luteController->showReader();
        });
        QTimer::singleShot(4600, &app, [luteController = luteController.get()]() {
            luteController->selectTextWord(12);
            luteController->refreshTags();
        });
        QTimer::singleShot(5200, &app, [luteController = luteController.get()]() {
            const QVariantList tags = luteController->tags();
            if(tags.isEmpty())
            {
                qInfo() << "lute tag toggle demo: no tags";
                return;
            }
            const qint64 tagId = tags.first().toMap().value("id").toLongLong();
            qInfo() << "lute tag toggle demo:" << tagId
                    << luteController->toggleActiveTag(tagId)
                    << luteController->activeWord().value("tags");
        });
        QTimer::singleShot(6400, &engine, [&engine, outputPath]() {
            if(auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().value(0)))
                window->grabWindow().save(outputPath);
            QCoreApplication::quit();
        });
    }

    // Fork QA helper: `--lute-demo-tags` opens the tag manager.
    if(app.arguments().contains("--lute-demo-tags"))
    {
        const QString outputPath = demoOutputPath;
        QTimer::singleShot(2500, &app, [luteController = luteController.get()]() {
            luteController->openLibrary();
        });
        QTimer::singleShot(3300, &app, [luteController = luteController.get()]() {
            luteController->openTags();
        });
        QTimer::singleShot(5000, &engine, [&engine, outputPath]() {
            if(auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().value(0)))
                window->grabWindow().save(outputPath);
            QCoreApplication::quit();
        });
    }

    // Fork QA helper: `--lute-demo-hub-stats` opens the Lute hub statistics.
    if(app.arguments().contains("--lute-demo-hub-stats"))
    {
        const QString outputPath = demoOutputPath;
        QTimer::singleShot(2500, &app, [luteController = luteController.get()]() {
            luteController->openLibrary();
        });
        QTimer::singleShot(3300, &app, [luteController = luteController.get()]() {
            luteController->openLuteStatistics();
        });
        QTimer::singleShot(5000, &engine, [&engine, outputPath]() {
            if(auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().value(0)))
                window->grabWindow().save(outputPath);
            QCoreApplication::quit();
        });
    }

    // Fork QA helper: `--lute-demo-languages` opens the language manager.
    if(app.arguments().contains("--lute-demo-languages"))
    {
        const QString outputPath = demoOutputPath;
        QTimer::singleShot(2500, &app, [luteController = luteController.get()]() {
            luteController->openLibrary();
        });
        QTimer::singleShot(3300, &app, [luteController = luteController.get()]() {
            luteController->openLanguages();
        });
        QTimer::singleShot(5000, &engine, [&engine, outputPath]() {
            if(auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().value(0)))
                window->grabWindow().save(outputPath);
            QCoreApplication::quit();
        });
    }

    // Fork QA helper: `--lute-demo-terms` opens the Lute term list.
    if(app.arguments().contains("--lute-demo-terms"))
    {
        const QString outputPath = demoOutputPath;
        QTimer::singleShot(2500, &app, [luteController = luteController.get()]() {
            luteController->openLibrary();
        });
        QTimer::singleShot(3300, &app, [luteController = luteController.get()]() {
            luteController->openTerms();
        });
        QTimer::singleShot(5200, &engine, [&engine, outputPath]() {
            if(auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().value(0)))
                window->grabWindow().save(outputPath);
            QCoreApplication::quit();
        });
    }

    // Fork QA helper: `--lute-demo-create` creates a Lute book from text and
    // shows it in the library.
    if(app.arguments().contains("--lute-demo-create"))
    {
        const QString outputPath = demoOutputPath;
        QTimer::singleShot(2500, &app, [luteController = luteController.get()]() {
            QString text;
            for(int i = 0; i < 40; ++i)
                text += QStringLiteral("The quiet harbor was full of small boats. ");
            text += QStringLiteral("\n\n---\n\n");
            for(int i = 0; i < 40; ++i)
                text += QStringLiteral("Winter changed the old harbor and the lighthouse. ");
            qInfo() << "lute create demo:"
                    << luteController->createBook(QStringLiteral("Harbor demo"),
                                                  text, false);
            luteController->openLibrary();
        });
        QTimer::singleShot(4800, &engine, [&engine, outputPath]() {
            if(auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().value(0)))
                window->grabWindow().save(outputPath);
            QCoreApplication::quit();
        });
    }

    // Fork QA helper: `--lute-demo-hub` opens the Lute library page.
    if(app.arguments().contains("--lute-demo-hub"))
    {
        const QString outputPath = demoOutputPath;
        QTimer::singleShot(2500, &app, [luteController = luteController.get()]() {
            luteController->openLibrary();
        });
        QTimer::singleShot(4500, &engine, [&engine, outputPath]() {
            if(auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().value(0)))
                window->grabWindow().save(outputPath);
            QCoreApplication::quit();
        });
    }

    // Fork QA helper: `--lute-demo-word` opens a Lute text and selects a word
    // so the word popup renders above the text.
    if(app.arguments().contains("--lute-demo-word"))
    {
        const QString outputPath = demoOutputPath;
        QTimer::singleShot(2500, &app, [luteController = luteController.get()]() {
            luteController->openLibrary();
        });
        QTimer::singleShot(3300, &app, [luteController = luteController.get()]() {
            const QVariantList books = luteController->books();
            if(books.isEmpty())
                return;
            const qint64 bookId = books.first().toMap().value(QStringLiteral("id")).toLongLong();
            if(!luteController->openBook(bookId))
                return;
            luteController->showReader();
        });
        QTimer::singleShot(4600, &app, [luteController = luteController.get()]() {
            luteController->selectTextWord(12);
        });
        QTimer::singleShot(6200, &engine, [&engine, outputPath]() {
            if(auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().value(0)))
                window->grabWindow().save(outputPath);
            QCoreApplication::quit();
        });
    }

    // Fork QA helper: `--lute-demo-reading` opens the Lute library and the
    // first book in the Lute text reader.
    if(app.arguments().contains("--lute-demo-reading"))
    {
        const QString outputPath = demoOutputPath;
        QTimer::singleShot(2500, &app, [luteController = luteController.get()]() {
            luteController->openLibrary();
        });
        QTimer::singleShot(3300, &app, [luteController = luteController.get()]() {
            const QVariantList books = luteController->books();
            if(books.isEmpty())
                return;
            const qint64 bookId = books.first().toMap().value(QStringLiteral("id")).toLongLong();
            if(luteController->openBook(bookId))
                luteController->showReader();
        });
        QTimer::singleShot(5600, &engine, [&engine, outputPath]() {
            if(auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().value(0)))
                window->grabWindow().save(outputPath);
            QCoreApplication::quit();
        });
    }

    // Fork QA helper: `--lute-demo-library` adds a PDF to the Lute library
    // without opening it, then shows the settings page with the result banner.
    if(app.arguments().contains("--lute-demo-library"))
    {
        const QString outputPath = demoOutputPath;

        QTimer::singleShot(2500, &app, [luteController = luteController.get(), demoLibraryPath]() {
            qInfo() << "lute library demo: adding" << demoLibraryPath
                    << luteController->addPdfToLuteLibrary(demoLibraryPath);
        });
        QTimer::singleShot(3500, &app, [luteController = luteController.get()]() {
            luteController->openSettings();
        });
        QTimer::singleShot(5200, &engine, [&engine, outputPath]() {
            if(auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().value(0)))
                window->grabWindow().save(outputPath);
            QCoreApplication::quit();
        });
    }

    // Fork QA helper: `--lute-demo-back` opens the Lute settings from the
    // reader and returns with the "Back to reading" action.
    if(app.arguments().contains("--lute-demo-back"))
    {
        const QString outputPath = demoOutputPath;

        QTimer::singleShot(3000, &app, [luteController = luteController.get()]() {
            luteController->openSettings();
        });
        QTimer::singleShot(4500, &app, [luteController = luteController.get()]() {
            luteController->openReader();
        });
        QTimer::singleShot(5600, &engine, [&engine, outputPath]() {
            if(auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().value(0)))
                window->grabWindow().save(outputPath);
            QCoreApplication::quit();
        });
    }

    // Fork QA helper: `--lute-demo-settings` / `--lute-demo-stats` open the
    // Lute settings or statistics page and save a screenshot of the window.
    const bool demoSettings = app.arguments().contains("--lute-demo-settings");
    const bool demoStats = app.arguments().contains("--lute-demo-stats");
    if(demoSettings || demoStats)
    {
        const QString outputPath = demoOutputPath;

        QTimer::singleShot(3000, &app, [demoSettings, luteController = luteController.get()]() {
            if(demoSettings)
                luteController->openSettings();
            else
                luteController->openStatistics();
        });

        QTimer::singleShot(5000, &engine, [&engine, outputPath]() {
            if(auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().value(0)))
                window->grabWindow().save(outputPath);
            QCoreApplication::quit();
        });
    }

    return QGuiApplication::exec();
    // clang-format on
}

void setupGlobalSettings()
{
    QSettings settings;
    QString cfgFile = settings.value("serverHost", QVariant("")).toString();
    if(cfgFile.isEmpty())
        settings.setValue("serverHost", "https://api.librumreader.com");

    QString sslSettings = settings.value("selfHosted", QVariant("")).toString();
    if(sslSettings.isEmpty())
        settings.setValue("selfHosted", "false");
}

void loadFont(const QString& path)
{
    int result = QFontDatabase::addApplicationFont(path);
    if(result == -1)
        qWarning() << QString("Loading font file: %1 failed.").arg(path);
}

void setupFonts()
{
    loadFont(":/resources/fonts/SF-Pro-Display-Bold.otf");
    loadFont(":/resources/fonts/SF-Pro-Display-Medium.otf");
    loadFont(":/resources/fonts/SF-Pro-Display-Regular.otf");
    loadFont(":/resources/fonts/SF-Pro-Display-Semibold.otf");

    QFont defaultFont("SF Pro Display");
    defaultFont.setLetterSpacing(QFont::AbsoluteSpacing, 0.1);
    QGuiApplication::setFont(defaultFont);
}
