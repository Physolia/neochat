# SPDX-FileCopyrightText: 2020-2021 Carl Schwan <carl@carlschwan.eu>
# SPDX-FileCopyrightText: 2020-2021 Nicolas Fella <nicolas.fella@gmx.de>
# SPDX-FileCopyrightText: 2020-2021 Tobias Fella <fella@posteo.de>
# SPDX-License-Identifier: BSD-2-Clause

add_library(neochatlib STATIC
    controller.cpp
    actionshandler.cpp
    emojimodel.cpp
    customemojimodel.cpp
    customemojimodel+network.cpp
    clipboard.cpp
    matriximageprovider.cpp
    messageeventmodel.cpp
    messagefiltermodel.cpp
    roomlistmodel.cpp
    roommanager.cpp
    neochatroom.cpp
    neochatuser.cpp
    userlistmodel.cpp
    publicroomlistmodel.cpp
    userdirectorylistmodel.cpp
    utils.cpp
    notificationsmanager.cpp
    sortfilterroomlistmodel.cpp
    chatdocumenthandler.cpp
    devicesmodel.cpp
    filetypesingleton.cpp
    login.cpp
    stickerevent.cpp
    chatboxhelper.cpp
    commandmodel.cpp
    webshortcutmodel.cpp
    spellcheckhighlighter.cpp
    blurhash.cpp
    blurhashimageprovider.cpp
    joinrulesevent.cpp
    collapsestateproxymodel.cpp
)

add_executable(neochat main.cpp ../res.qrc)
target_link_libraries(neochat PUBLIC neochatlib)

if(Quotient_VERSION_MINOR GREATER 6)
    target_compile_definitions(neochatlib PUBLIC QUOTIENT_07)
else()
    target_sources(neochatlib PUBLIC neochataccountregistry.cpp)
endif()

ecm_add_app_icon(NEOCHAT_ICON ICONS ${CMAKE_SOURCE_DIR}/128-logo.png)

target_sources(neochatlib PUBLIC ${NEOCHAT_ICON})

if(NOT ANDROID)
    target_sources(neochatlib PRIVATE colorschemer.cpp)
    if (NOT WIN32 AND NOT APPLE)
        target_sources(neochatlib PRIVATE trayicon_sni.cpp)
    else()
        target_sources(neochatlib PRIVATE trayicon.cpp)
    endif()
    target_link_libraries(neochatlib PUBLIC KF5::ConfigWidgets KF5::WindowSystem KF5::SonnetCore)
    target_compile_definitions(neochatlib PUBLIC -DHAVE_COLORSCHEME)
    target_compile_definitions(neochatlib PUBLIC -DHAVE_WINDOWSYSTEM)
endif()

if (NOT ANDROID AND NOT WIN32 AND NOT APPLE)
    target_sources(neochatlib PRIVATE runner.cpp)
    target_sources(neochat PRIVATE ../res_desktop.qrc)
    target_compile_definitions(neochatlib PUBLIC -DHAVE_RUNNER)
else()
    target_sources(neochatlib PUBLIC ../res_android.qrc)
endif()

target_include_directories(neochatlib PUBLIC ${CMAKE_BINARY_DIR})
target_link_libraries(neochatlib PUBLIC Qt::Quick Qt::Qml Qt::Gui Qt::Network Qt::QuickControls2 KF5::I18n KF5::Kirigami2 KF5::Notifications KF5::ConfigCore KF5::ConfigGui KF5::CoreAddons Quotient cmark::cmark ${QTKEYCHAIN_LIBRARIES})
if(TARGET QCoro5::Coro)
    target_link_libraries(neochatlib PUBLIC QCoro5::Coro)
else()
    target_link_libraries(neochatlib PUBLIC QCoro::QCoro)
endif()
kconfig_add_kcfg_files(neochatlib GENERATE_MOC neochatconfig.kcfgc)

if(NEOCHAT_FLATPAK)
    target_compile_definitions(neochatlib PUBLIC NEOCHAT_FLATPAK)
endif()

if(ANDROID)
    target_sources(neochatlib PUBLIC notifyrc.qrc)
    target_link_libraries(neochatlib PUBLIC Qt5::Svg OpenSSL::SSL)
    kirigami_package_breeze_icons(ICONS
        "arrow-down"
        "help-about"
        "im-user"
        "im-invisible-user"
        "im-kick-user"
        "mail-attachment"
        "dialog-cancel"
        "preferences-desktop-emoticons"
        "document-open"
        "document-save"
        "document-send"
        "dialog-close"
        "edit-delete-remove"
        "code-context"
        "document-edit"
        "list-user-add"
        "list-add-user"
        "user-others"
        "media-playback-pause"
        "media-playback-start"
        "go-up"
        "go-down"
        "list-add"
        "irc-join-channel"
        "settings-configure"
        "configure"
        "rating"
        "rating-unrated"
        "search"
        "mail-replied-symbolic"
        "edit-copy"
        "gtk-quit"
        "compass"
        "network-connect"
        "list-remove-user"
        "org.kde.neochat"
        "preferences-system-users"
        "preferences-desktop-theme-global"
    )
else()
    target_link_libraries(neochatlib PUBLIC Qt5::Widgets KF5::KIOWidgets)
    install(FILES neochat.notifyrc DESTINATION ${KNOTIFYRC_INSTALL_DIR})
endif()

if(TARGET KF5::DBusAddons)
    target_link_libraries(neochatlib PUBLIC KF5::DBusAddons)
    target_compile_definitions(neochatlib PRIVATE -DHAVE_KDBUSADDONS)
endif()

if (TARGET KF5::KIOWidgets)
    target_compile_definitions(neochatlib PUBLIC -DHAVE_KIO)
endif()

install(TARGETS neochatlib ${KF5_INSTALL_TARGETS_DEFAULT_ARGS})

if (NOT ANDROID AND NOT WIN32 AND NOT APPLE)
    install(FILES plasma-runner-neochat.desktop DESTINATION ${KDE_INSTALL_DATAROOTDIR}/krunner/dbusplugins)
endif()
