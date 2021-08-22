// SPDX-FileCopyrightText: 2018-2019 Black Hat <bhat@encom.eu.org>
// SPDX-FileCopyrightText: 2019 Kitsune Ral <kitsune-ral@users.sf.net>
// SPDX-FileCopyrightText: 2021 Tobias Fella <fella@posteo.de>
// SPDX-License-Identifier: GPL-3.0-only

#include "matriximageprovider.h"

#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QStandardPaths>
#include <QThread>

#include <openssl/evp.h>

#include "controller.h"

using Quotient::BaseJob;

QByteArray decrypt(const QByteArray &ciphertext, const QByteArray &key, const QByteArray &iv)
{
    QByteArray plaintext(ciphertext.size(), 0);
    EVP_CIPHER_CTX *ctx;
    int length;
    ctx = EVP_CIPHER_CTX_new();
    EVP_DecryptInit_ex(ctx, EVP_aes_256_ctr(), NULL, (const unsigned char *)key.data(), (const unsigned char *)iv.data());
    EVP_DecryptUpdate(ctx, (unsigned char *)plaintext.data(), &length, (const unsigned char *)ciphertext.data(), ciphertext.size());
    EVP_DecryptFinal_ex(ctx, (unsigned char *)plaintext.data() + length, &length);
    EVP_CIPHER_CTX_free(ctx);
    return plaintext;
}

MatrixImageResponse::MatrixImageResponse(QString id, QSize size)
    : mediaId(std::move(id))
    , requestedSize(size)
    , errorStr("Image request hasn't started")
{
    if (!Controller::instance().activeConnection()) {
        qWarning() << "Current connection is null";
        return;
    }

    if(mediaId.startsWith("mxc://")) {
        mediaId = mediaId.mid(6);
    }
    if(!mediaId.contains(QLatin1Char('/'))) {
        errorStr = "Invalid media id";
        Q_EMIT finished();
        return;
    }

    encrypted = mediaId.count(QLatin1Char('/')) != 1;
    auto parts = mediaId.split("/");
    mediaId = parts[0] + QStringLiteral("/") + parts[1];

    localFile = QStringLiteral("%1/image_provider/%2-%3x%4.png").arg(QStandardPaths::writableLocation(QStandardPaths::CacheLocation), mediaId, QString::number(requestedSize.width()), QString::number(requestedSize.height()));

    if(encrypted) {
        key = QByteArray::fromBase64(parts[2].replace(QLatin1Char('_'), QLatin1Char('/')).replace(QLatin1Char('-'), QLatin1Char('+')).toLatin1());
        iv = QByteArray::fromBase64(parts[3].toLatin1());
        sha256 = QByteArray::fromBase64(parts[4].toLatin1());
    }

    if(QFile::exists(localFile)) {
        QFile cacheFile(localFile);
        cacheFile.open(QIODevice::ReadOnly);
        QByteArray data = cacheFile.readAll();

        if(!encrypted) {
            QImage cachedImage(localFile);
            if(!cachedImage.isNull()) {
                image = cachedImage;
                errorStr.clear();
                Q_EMIT finished();
                return;
            }
        } else {
            auto actualSha256 = QCryptographicHash::hash(data, QCryptographicHash::Sha256);

            if(actualSha256 == sha256) {
                image.loadFromData(decrypt(data, key, iv));
                errorStr.clear();
                errorStr.clear();
                Q_EMIT finished();
                return;
            }
        }
    }

    moveToThread(Controller::instance().activeConnection()->thread());
    QMetaObject::invokeMethod(this, &MatrixImageResponse::startRequest, Qt::QueuedConnection);
}

void MatrixImageResponse::startRequest()
{
    if (!Controller::instance().activeConnection()) {
        return;
    }
    Q_ASSERT(QThread::currentThread() == Controller::instance().activeConnection()->thread());
    if(encrypted) {
        job = Controller::instance().activeConnection()->downloadFile(mediaId);
    } else {
        job = Controller::instance().activeConnection()->getThumbnail(mediaId, requestedSize);
    }
    connect(job, &BaseJob::finished, this, &MatrixImageResponse::prepareResult);
}

void MatrixImageResponse::prepareResult()
{
    Q_ASSERT(QThread::currentThread() == job->thread());
    Q_ASSERT(job->error() != BaseJob::Pending);
    {
        QWriteLocker _(&lock);
        if (job->error() == BaseJob::Success) {
            if(QFile::exists(localFile)) {
                QFile::remove(localFile);
            }
            if(encrypted) {
                QFile::rename(dynamic_cast<DownloadFileJob *>(job)->targetFileName(), localFile);
                QFile encryptedFile(localFile);
                encryptedFile.open(QIODevice::ReadOnly);
                auto data = encryptedFile.readAll();
                auto actualHash = QCryptographicHash::hash(data, QCryptographicHash::Sha256);
                if (sha256 != actualHash) {
                    image = QImage();
                    errorStr = "Hash value for encrypted image is not correct";
                    return;
                }
                image = QImage::fromData(decrypt(data, key, iv));
            } else {
                image = dynamic_cast<MediaThumbnailJob *>(job)->thumbnail();
                image.save(localFile);
            }

            errorStr.clear();
        } else if (job->error() == BaseJob::Abandoned) {
            errorStr = "Image request has been cancelled";
        } else {
            errorStr = job->errorString();
            qWarning() << "ThumbnailResponse: no valid image for" << mediaId << "-" << errorStr;
        }
        job = nullptr;
    }
    Q_EMIT finished();
}

void MatrixImageResponse::doCancel()
{
    if (!Controller::instance().activeConnection()) {
        return;
    }
    // Runs in the main thread, not QML thread
    if (job) {
        Q_ASSERT(QThread::currentThread() == job->thread());
        job->abandon();
    }
}

QQuickTextureFactory *MatrixImageResponse::textureFactory() const
{
    QReadLocker _(&lock);
    return QQuickTextureFactory::textureFactoryForImage(image);
}

QString MatrixImageResponse::errorString() const
{
    QReadLocker _(&lock);
    return errorStr;
}

void MatrixImageResponse::cancel()
{
    QMetaObject::invokeMethod(this, &MatrixImageResponse::doCancel, Qt::QueuedConnection);
}

QQuickImageResponse *MatrixImageProvider::requestImageResponse(const QString &id, const QSize &size)
{
    return new MatrixImageResponse(id, size);
}

MatrixImageProvider::MatrixImageProvider()
{
    QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation)).mkdir("image_provider");
}
