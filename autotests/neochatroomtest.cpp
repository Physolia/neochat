// SPDX-FileCopyrightText: 2022 Carl Schwan <carl@carlschwan.eu>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <qtestcase.h>
#include <quotient_common.h>
#define protected public
#include "neochatroom.h"
#undef protected
#include "syncdata.h"
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
    connection->setupDummyConnection(QStringLiteral("@bob:kde.org"), QStringLiteral("my pinephone"), QStringLiteral("https://kde.org"));
    room = new NeoChatRoom(connection, QStringLiteral("#myroom:kde.org"), JoinState::Join);

    auto json = QJsonDocument::fromJson(R"EVENT({
  "account_data": {
    "events": [
      {
        "content": {
          "tags": {
            "u.work": {
              "order": 0.9
            }
          }
        },
        "type": "m.tag"
      },
      {
        "content": {
          "custom_config_key": "custom_config_value"
        },
        "type": "org.example.custom.room.config"
      }
    ]
  },
  "ephemeral": {
    "events": [
      {
        "content": {
          "user_ids": [
            "@alice:matrix.org",
            "@bob:example.com"
          ]
        },
        "room_id": "!jEsUZKDJdhlrceRyVU:example.org",
        "type": "m.typing"
      }
    ]
  },
  "state": {
    "events": [
      {
        "content": {
          "avatar_url": "mxc://example.org/SEsfnsuifSDFSSEF",
          "displayname": "Alice Margatroid",
          "membership": "join",
          "reason": "Looking for support"
        },
        "event_id": "$143273582443PhrSn:example.org",
        "origin_server_ts": 1432735824653,
        "room_id": "!jEsUZKDJdhlrceRyVU:example.org",
        "sender": "@example:example.org",
        "state_key": "@alice:example.org",
        "type": "m.room.member",
        "unsigned": {
          "age": 1234
        }
      }
    ]
  },
  "summary": {
    "m.heroes": [
      "@alice:example.com",
      "@bob:example.com"
    ],
    "m.invited_member_count": 0,
    "m.joined_member_count": 2
  },
  "timeline": {
    "events": [
      {
        "content": {
          "avatar_url": "mxc://example.org/SEsfnsuifSDFSSEF",
          "displayname": "Alice Margatroid",
          "membership": "join",
          "reason": "Looking for support"
        },
        "event_id": "$143273582443PhrSn:example.org",
        "origin_server_ts": 1432735824653,
        "room_id": "!jEsUZKDJdhlrceRyVU:example.org",
        "sender": "@example:example.org",
        "state_key": "@alice:example.org",
        "type": "m.room.member",
        "unsigned": {
          "age": 1234
        }
      },
      {
        "content": {
          "body": "This is an example text message",
          "format": "org.matrix.custom.html",
          "formatted_body": "<b>This is an example text message</b>",
          "msgtype": "m.text"
        },
        "event_id": "$143273582443PhrSn:example.org",
        "origin_server_ts": 1432735824653,
        "room_id": "!jEsUZKDJdhlrceRyVU:example.org",
        "sender": "@example:example.org",
        "type": "m.room.message",
        "unsigned": {
          "age": 1234
        }
      }
    ],
    "limited": true,
    "prev_batch": "t34-23535_0_0"
  }
})EVENT");
    SyncRoomData roomData(QStringLiteral("@bob:kde.org"), JoinState::Join, json.object());
    room->updateData(std::move(roomData));
}

void NeoChatRoomTest::testSubtitleTest()
{
    QVERIFY(room->timelineSize() > 0);
}

QTEST_MAIN(NeoChatRoomTest)
#include "neochatroomtest.moc"
