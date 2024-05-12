// SPDX-FileCopyrightText: 2024 James Graham <james.h.graham@protonmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts

import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard
import org.kde.kitemmodels

import org.kde.neochat

ColumnLayout {
    id: root

    required property NeoChatConnection connection

    required property string title
    required property string medium

    FormCard.FormHeader {
        title: root.title
    }
    FormCard.FormCard {
        id: devicesCard

        Repeater {
            id: deviceRepeater
            model: KSortFilterProxyModel {
                sourceModel: root.connection.threePIdModel
                filterRoleName: "medium"
                filterString: root.medium
            }

            delegate: FormCard.AbstractFormDelegate {
                id: threePIdDelegate
                required property string address
                required property string medium

                contentItem: RowLayout {
                    QQC2.Label {
                        Layout.fillWidth: true
                        text: threePIdDelegate.address
                        textFormat: Text.PlainText
                        elide: Text.ElideRight
                        wrapMode: Text.Wrap
                        maximumLineCount: 2
                        color: Kirigami.Theme.textColor
                    }
                    QQC2.ToolButton {
                        text: i18nc("@action:button", "Remove")
                        icon.name: "edit-delete-remove"
                        onClicked: threePIdAddHelper.remove3PId(threePIdDelegate.address, threePIdDelegate.medium)
                    }
                }
            }



            FormCard.FormTextDelegate {
                required property string address
                text: address
            }
        }
        FormCard.FormTextFieldDelegate {
            id: newCountryCode
            visible: root.medium === "msisdn"
            readOnly: threePIdAddHelper.newIdStatus ==  ThreePIdAddHelper.Verification ||
                      threePIdAddHelper.newIdStatus == ThreePIdAddHelper.Authentication ||
                      threePIdAddHelper.newIdStatus == ThreePIdAddHelper.AuthFailure ||
                      threePIdAddHelper.newIdStatus == ThreePIdAddHelper.VerificationFailure
            label: i18nc("@label:textbox", "Country Code for new phone number")

            Connections {
                target: root.connection.threePIdModel

                function onModelReset() {
                    newCountryCode.text = ""
                }
            }
        }
        FormCard.FormTextFieldDelegate {
            id: newId
            readOnly: threePIdAddHelper.newIdStatus ==  ThreePIdAddHelper.Verification ||
                      threePIdAddHelper.newIdStatus == ThreePIdAddHelper.Authentication ||
                      threePIdAddHelper.newIdStatus == ThreePIdAddHelper.AuthFailure ||
                      threePIdAddHelper.newIdStatus == ThreePIdAddHelper.VerificationFailure
            label: root.medium === "email" ? i18nc("@label:textbox", "New email address") : i18nc("@label:textbox", "New phone number")

            statusMessage: switch(threePIdAddHelper.newIdStatus) {
            case ThreePIdAddHelper.Verification:
                return i18n("%1. Please follow the instructions there and then click the button below", root.medium == "email" ? i18n("We've sent you an email") : i18n("We've sent you a text message"));
            case ThreePIdAddHelper.Invalid:
                return root.medium == "email" ? i18n("The entered email is not valid") : i18n("The entered phone number is not valid");
            case ThreePIdAddHelper.AuthFailure:
                return i18n("Incorrect password entered");
            case ThreePIdAddHelper.VerificationFailure:
                return root.medium == "email" ? i18n("The email has not been verified. Please go to the email and follow the instructions there and then click the button below") : i18n("The phone number has not been verified. Please go to the text message and follow the instructions there and then click the button below");
            default:
                return "";
            }
            status: switch(threePIdAddHelper.newIdStatus) {
            case ThreePIdAddHelper.Invalid:
            case ThreePIdAddHelper.AuthFailure:
                return Kirigami.MessageType.Error;
            case ThreePIdAddHelper.VerificationFailure:
                return Kirigami.MessageType.Warning;
            default:
                return Kirigami.MessageType.Information;
            }

            onAccepted: _private.openPasswordSheet()

            Connections {
                target: root.connection.threePIdModel

                function onModelReset() {
                    newId.text = ""
                }
            }
        }
        FormCard.FormButtonDelegate {
            text: threePIdAddHelper.newIdStatus == ThreePIdAddHelper.Ready ? i18nc("@action:button Add new email or phone number", "Add") : i18nc("@action:button", "Continue")
            onClicked: _private.openPasswordSheet()
        }
        FormCard.FormButtonDelegate {
            visible: threePIdAddHelper.newIdStatus ==  ThreePIdAddHelper.Verification ||
                     threePIdAddHelper.newIdStatus == ThreePIdAddHelper.Authentication ||
                     threePIdAddHelper.newIdStatus == ThreePIdAddHelper.AuthFailure ||
                     threePIdAddHelper.newIdStatus == ThreePIdAddHelper.VerificationFailure
            text: i18nc("@action:button As in 'go back'", "Back")
            onClicked: threePIdAddHelper.back()
        }
    }

    ThreePIdAddHelper {
        id: threePIdAddHelper
        connection: root.connection
        medium: root.medium
        newId: newId.text
    }

    QtObject {
        id: _private
        function openPasswordSheet() {
            if (threePIdAddHelper.newIdStatus == ThreePIdAddHelper.Ready) {
                threePIdAddHelper.initiateNewIdAdd();
            } else {
                let dialog = Qt.createComponent('org.kde.neochat.settings', 'PasswordSheet').createObject(root, {});
                dialog.submitPassword.connect(password => threePIdAddHelper.finalizeNewIdAdd(password));
                dialog.open();
            }
        }
    }
}
