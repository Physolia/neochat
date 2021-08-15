// SPDX-FileCopyrightText: 2020 Tobias Fella <fella@posteo.de>
// SPDX-FileCopyrightText: 2021 Carl Schwan <carl@carlschwan.eu>
// SPDX-License-Identifier: LGPL-2.0-or-later

import QtQuick 2.15
import QtQuick.Controls 2.15 as QQC2
import QtQuick.Layouts 1.15

import org.kde.kirigami 2.18 as Kirigami
import org.kde.sonnet 1.0 as Sonnet

import org.kde.neochat 1.0
import NeoChat.Settings 1.0

Kirigami.CategorizedSettings {
    actions: [
        Kirigami.SettingAction {
            text: i18n("General")
            icon.name: "org.kde.neochat"
            page: "qrc:/imports/NeoChat/Settings/GeneralSettingsPage.qml"
        },
        Kirigami.SettingAction {
            text: i18n("Appearance")
            icon.name: "preferences-desktop-theme-global"
            page: "qrc:/imports/NeoChat/Settings/AppearanceSettingsPage.qml"
        },
        Kirigami.SettingAction {
            text: i18n("Accounts")
            icon.name: "preferences-system-users"
            page: "qrc:/imports/NeoChat/Page/AccountsPage.qml"
        },
        Kirigami.SettingAction {
            text: i18n("Custom Emoji")
            icon.name: "preferences-desktop-emoticons"
            page: "qrc:/imports/NeoChat/Settings/Emoticons.qml"
        },
        Kirigami.SettingAction {
            text: i18n("Devices")
            icon.name: "network-connect"
            page: "qrc:/imports/NeoChat/Page/DevicesPage.qml"
        },
        Kirigami.SettingAction {
            text: i18n("Spell Checking")
            icon.name: "tools-check-spelling"
            page: "qrc:/imports/NeoChat/Settings/SpellCheckingPage.qml"
        },
        Kirigami.SettingAction {
            text: i18n("About NeoChat")
            icon.name: "help-about"
            page: "qrc:/imports/NeoChat/Settings/AboutPage.qml"
        }
    ]
}
