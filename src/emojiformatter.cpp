// SPDX-FileCopyrightText: Carson Black <uhhadd@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QQuickTextDocument>
#include <QTextBoundaryFinder>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QGuiApplication>
#include <QPalette>

#include <KLocalizedString>

#include <unicode/urename.h>
#include <unicode/uchar.h>

#include "emojiformatter.h"

bool isEmoji(const QString& text)
{
    QTextBoundaryFinder finder(QTextBoundaryFinder::Grapheme, text);
    int pos = 0;
    while (finder.toNextBoundary() != -1) {
        auto range = finder.position();

        auto first = text.mid(pos, range-pos).toUcs4()[0];

        if (text[pos].isSpace()) {
            pos = range;
            continue;
        }

        if (!u_hasBinaryProperty(first, UCHAR_EMOJI_PRESENTATION)) {
            return false;
        }

        pos = range;
    }

    return true;
}

void EmojiFormatter::attach(QQuickTextDocument* doc)
{
    auto txt = doc->textDocument()->toRawText();
    auto editString = ki18n(" (edited").toString();
    format(doc->textDocument(), isEmoji(txt), txt.endsWith(editString), editString.length());
}

void EmojiFormatter::format(QTextDocument* doku, bool emojiOnly, bool hasEdit, int editedLength)
{

    disconnect(doku, nullptr, this, nullptr);

    QTextCursor curs(doku);

    QTextCharFormat cfmt;

    QTextBoundaryFinder finder(QTextBoundaryFinder::Grapheme, doku->toRawText());
    int pos = 0;
    while (finder.toNextBoundary() != -1) {
        auto range = finder.position();

        auto first = doku->toRawText().mid(pos, range-pos).toUcs4()[0];

        if (u_hasBinaryProperty(first, UCHAR_EMOJI_PRESENTATION)) {
            curs.setPosition(pos, QTextCursor::MoveAnchor);
            curs.setPosition(range, QTextCursor::KeepAnchor);

            QTextCharFormat cfmt;
            auto font = QGuiApplication::font();
            font.setFamily("emoji");
            if (emojiOnly) {
                font.setPointSize(font.pointSize()*4);
            }
            cfmt.setFont(font);

            curs.setCharFormat(cfmt);
        }

        pos = range;
    }

    if (hasEdit) {
        auto len = doku->toRawText().length();
        curs.setPosition(len-editedLength, QTextCursor::MoveAnchor);
        curs.setPosition(len, QTextCursor::KeepAnchor);

        auto fg = QGuiApplication::palette().text().color();
        fg.setAlphaF(0.5);

        QTextCharFormat it;
        it.setForeground(fg);

        curs.setCharFormat(it);
    }

    connect(doku, &QTextDocument::contentsChanged, this, [this, doku]() {
        auto txt = doku->toRawText();
        auto editString = ki18n(" (edited").toString();
        format(doku, isEmoji(doku->toRawText()), txt.endsWith(editString), editString.length());
    });

    return;
}