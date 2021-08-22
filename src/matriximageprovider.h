// SPDX-FileCopyrightText: 2018-2019 Black Hat <bhat@encom.eu.org>
// SPDX-FileCopyrightText: 2019 Kitsune Ral <kitsune-ral@users.sf.net>
// SPDX-FileCopyrightText: 2021 Tobias Fella <fella@posteo.de>
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QQuickAsyncImageProvider>

#include <connection.h>
#include <jobs/mediathumbnailjob.h>

#include <QAtomicPointer>
#include <QReadWriteLock>

#include <jobs/downloadfilejob.h>

namespace Quotient
{
class Connection;
}
class MatrixImageResponse : public QQuickImageResponse
{
    Q_OBJECT
public:    MatrixImageResponse(QString mediaId, QSize size);
    ~MatrixImageResponse() override = default;

private Q_SLOTS:
    void startRequest();
    void prepareResult();
    void doCancel();

private:
    QString mediaId;
    QByteArray key;
    QByteArray iv;
    QByteArray sha256;

    Quotient::BaseJob *job = nullptr;

    QString localFile;
    QSize requestedSize;
    QImage image;
    QString errorStr;
    mutable QReadWriteLock lock; // Guards ONLY these two members above
    bool encrypted;

    QQuickTextureFactory *textureFactory() const override;
    QString errorString() const override;
    void cancel() override;
};

class MatrixImageProvider : public QQuickAsyncImageProvider
{
public:
    MatrixImageProvider();
    QQuickImageResponse *requestImageResponse(const QString &id, const QSize &requestedSize) override;
};
