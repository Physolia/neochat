// SPDX-FileCopyrightText: 2022 Carl Schwan <carl@carlschwan.eu>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "neochatroom.h"
#include <QObject>
#include <QSignalSpy>
#include <QTest>

class NeoChatRoomTest : public QObject {
    Q_OBJECT

private:
    Connection *connection = nullptr;
    NeoChatRoom *room = nullptr;

private Q_SLOTS:
    void initTestCase();
    void testSubtitleTest();
};

void NeoChatRoomTest::initTestCase()
{
    connection = new Connection();
    connection->setHomeserver(QStringLiteral("https://kde.org"));
    connection->assumeIdentity(QStringLiteral("@bob:kde.org"), QStringLiteral("totally-not-bob-accesstokem"), QStringLiteral("my pinephone"));
    connection->setupDummyConnection();
    room = new NeoChatRoom(connection, QStringLiteral("#myroom:kde.org"), JoinState::Join);

}

void NeoChatRoomTest::testSubtitleTest()
{

}

QTEST_MAIN(NeoChatRoomTest)
#include "neochatroomtest.moc"
