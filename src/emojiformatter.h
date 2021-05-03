// SPDX-FileCopyrightText: Carson Black <uhhadd@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>

class QQuickTextDocument;
class QTextDocument;

class EmojiFormatter : public QObject
{

    Q_OBJECT

    void format(QTextDocument* doc, bool emojiOnly, bool hasEdit, int editedLength);

public:

    Q_INVOKABLE void attach(QQuickTextDocument* doc);

};